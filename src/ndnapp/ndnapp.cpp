// TODO: add copyright

#include "ndnapp.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <ndn-ind/transport/udp-transport.hpp>
#include <ndn-ind/transport/tcp-transport.hpp>
#include <ndn-ind/face.hpp>
#include <ndn-ind/security/key-chain.hpp>
#include <ndn-ind/util/memory-content-cache.hpp>
#include <ndn-ind/security/certificate/certificate.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

using namespace std;
using namespace ndn;
using namespace ndnsd;
using namespace ndnapp;

static chrono::seconds kCertRenewWindow = chrono::minutes(15);

App::App(string appName, string id, const shared_ptr<spdlog::logger>& logger,
    Face* face, KeyChain* keyChain, bool filterInterface)
    : appName_(appName)
    , instanceId_(id)
    , logger_(logger)
    , mfd_(ndntools::MicroForwarder::get())
    , face_(face)
    , keyChain_(keyChain)
    , filterInterface_(filterInterface)
    , identityManager_(this, logger_, keyChain)
    , memCache_(make_shared<MemoryContentCache>(face_))
{
}

void App::configure(const vector<Proto>& protocols, const NdnSd::AdvertiseParameters& params,
    const string& signingIdentityOrPath, const string& password)
{
    protocols_ = protocols;
    params_ = params;

    setupMicroforwarder();
    printAppInfo();

    setupNdnSd();

    identityManager_.setup(signingIdentityOrPath, password);
    
    memCache_->registerPrefix(identityManager_.getAppIdentity(), [this](const auto& prefix)
    {
        logger_->error("Failed to register prefix for app identity: {}", prefix);
    });
    memCache_->add(*identityManager_.getAppCertificate());
    memCache_->add(*identityManager_.getInstanceCertificate());

    // setup cert auto-renew
    setupCertificateAutoRenew(identityManager_.getAppCertificate(), [this]() 
    {
        identityManager_.createNewAppIdentity();
        identityManager_.createNewInstanceIdentity();
    });
    setupCertificateAutoRenew(identityManager_.getInstanceCertificate(), [this]() 
    {
        identityManager_.createNewInstanceIdentity();
    });
}

void App::setupCertificateAutoRenew(const shared_ptr<const CertificateV2>& cert, function<void()> renewRoutine)
{
    auto renewT = (cert->getValidityPeriod().getNotBefore() - kCertRenewWindow);
    auto now = chrono::system_clock::now();

    // capture "this" -- ndnapp assumed to be a singleton and live through the application lifecycle
    face_->callLater(now - renewT, [this, cert, renewRoutine]() {
        logger_->info("Certificate {} expires in {} seconds. Triggered renew...", 
            cert->getName().toUri(),
            kCertRenewWindow.count());

        renewRoutine();
    });
}

void App::setupMicroforwarder()
{
    for (auto p : protocols_)
    {
        try
        {
            ptr_lib::shared_ptr<const Transport> t;

            if (p == Proto::UDP)
                t = mfd_->addChannel(
                    ndn::ptr_lib::make_shared<ndn::UdpTransport::ConnectionInfo>("", 0));
            if (p == Proto::TCP)
                t = mfd_->addChannel(
                    ndn::ptr_lib::make_shared<ndn::TcpTransport::ConnectionInfo>("", 0));

            if (!t)
                logger_->error("failed to add UDP listen channel");
            else
            {
                protocolPort_[p] = t->getBoundPort();
            }
        }
        catch (exception& e)
        {
            logger_->error("caught exception setting up microforwarder: {}", e.what());
            throw e;
        }
    }
}

void App::setupNdnSd()
{
    logger_->info("announcing services...");
    for (auto p : { Proto::TCP, Proto::UDP })
    {
        NdnSd::AdvertiseParameters prm = params_;
        prm.protocol_ = p;
        shared_ptr<NdnSd> s = make_shared<NdnSd>(instanceId_);

        if (find(protocols_.begin(), protocols_.end(), p) != protocols_.end())
        {
            prm.port_ = protocolPort_[p];

            s->announce(prm,
                [s, this](void*)
            {
                logger_->info("announce {}/{} iface {} {}:{} -- {}", s->getUuid(), s->getProtocol(),
                    s->getInterface(), s->getDomain(), s->getPort(), s->getPrefix());
            },
                [s, this](int reqId, int errCode, std::string msg, bool, void*)
            {
                logger_->error("announce error {}/{}: {} - {}", s->getUuid(), s->getProtocol(), errCode, msg);
            });
        }

        // TODO: browse constraints -- shall browse with empty subtype or our subtype?
        NdnSd::BrowseConstraints cnstr = static_cast<NdnSd::BrowseConstraints>(prm);
        s->browse(cnstr,
            [s, this](int, Announcement a, shared_ptr<const NdnSd> sd, void*)
        {
            if (a == Announcement::Added)
            {
                if (filterInterface_ && discoveredInstances_.count(sd->getUuid()))
                    logger_->warn("iface filtering: skip duplicate {} on iface {}", sd->getUuid(), sd->getInterface());
                else
                {
                    logger_->info("add {}/{} iface {}", sd->getUuid(), sd->getProtocol(), sd->getInterface());

                    discoveredInstances_.insert(sd->getUuid());

                    s->resolve(sd,
                        [this](int, Announcement a, shared_ptr<const NdnSd> sd, void*)
                    {
                        if (a == Announcement::Resolved)
                        {
                            logger_->info("resolve {} iface {} {}://{}:{} -- {}", sd->getUuid(), sd->getInterface(),
                                sd->getProtocol(), sd->getHostname(), sd->getPort(), sd->getPrefix());

                            addRoute(sd);

                            try {

                                if (onInstanceAdd_)
                                    onInstanceAdd_(sd);
                            }
                            catch (exception& e)
                            {
                                logger_->error("caught exception while calling user callback: {}", e.what());
                            }
                        }
                    },
                        [this, sd](int reqId, int errCode, std::string msg, bool ismDns, void*)
                    {
                        logger_->error("resolve error {} (is mDNS {}): {} - {}", sd->getUuid(),
                            ismDns, errCode, msg);
                    });
                }
            }

            if (a == Announcement::Removed)
            {
                if (discoveredInstances_.count(sd->getUuid()))
                {
                    logger_->info("remove {0}/{1}", sd->getUuid(), sd->getProtocol());

                    removeRoute(sd);
                    discoveredInstances_.erase(sd->getUuid());

                    try {
                        if (onInstanceRemove_)
                            onInstanceRemove_(sd);
                    }
                    catch (exception& e)
                    {
                        logger_->error("caught exception while calling user callback: {}", e.what());
                    }
                }
            }
        },
            [this](int reqId, int errCode, std::string msg, bool ismDns, void*)
        {
            logger_->info("add error (is mDNS {}): {} - {}", ismDns, errCode, msg);
        });

        ndnsds_.push_back(s);
    }
}

void App::processEvents()
{
    mfd_->processEvents();

    for (auto s : ndnsds_)
        s->run(1);
}

vector<shared_ptr<const ndnsd::NdnSd>>
App::getDiscoveredNodes() const
{
    vector<shared_ptr<const ndnsd::NdnSd>> nodes;

    for (auto& sd : ndnsds_)
    {
        auto discovered = sd->getDiscoveredServices();
        nodes.insert(nodes.end(), discovered.begin(), discovered.end());
    }

    return nodes;
}

void App::printAppInfo()
{
    fmt::print(R"(
{}
    instance id {}
    announce NDN service: {}, subtype {}
    prefix {}
    listen ports {}

)",
appName_,
fmt::format(fmt::emphasis::bold, "{}", instanceId_),
fmt::format(fmt::emphasis::bold, "{}", protocols_), params_.subtype_,
fmt::format(fmt::emphasis::bold, "{}", params_.prefix_),
fmt::format(fmt::emphasis::bold, "{}", protocolPort_)
);
    }

void App::addRoute(const shared_ptr<const NdnSd>& sd)
{
    if (sd->getPrefix().size())
    {
        // TODO: verify instance here
        //if (verifyInstance(sd->getCertificate()))
        {
            string hostname(sd->getHostname());
            string uri = sd->getProtocol() == Proto::TCP ? "tcp://" : "udp://";
            uri += hostname + ":" + to_string(sd->getPort());

            ptr_lib::shared_ptr<Transport> t;
            if (sd->getProtocol() == Proto::TCP)
                t = ptr_lib::make_shared<TcpTransport>();
            else
                t = ptr_lib::make_shared<UdpTransport>();

            ptr_lib::shared_ptr<Transport::ConnectionInfo> ci;
            if (sd->getProtocol() == Proto::TCP)
                ci = ndn::ptr_lib::make_shared<TcpTransport::ConnectionInfo>(hostname.c_str(), sd->getPort());
            else
                ci = ndn::ptr_lib::make_shared<UdpTransport::ConnectionInfo>(hostname.c_str(), sd->getPort());

            int faceId = mfd_->addFace(uri, t, ci);
            faces_[sd] = faceId;

            if (mfd_->addRoute(Name(sd->getPrefix()), faceId))
            {
                logger_->info("add route {} face {} id {} ", sd->getPrefix(), uri, sd->getUuid());
            }
            else
            {
                logger_->error("failed to add route {} face {} instance {}", sd->getPrefix(), uri, sd->getUuid());
            }
        }
    }
    else
    {
        logger_->warn("instance {} has empty prefix. no route created", sd->getUuid());
    }
}

void App::removeRoute(const shared_ptr<const NdnSd>& sd)
{
    auto it = faces_.find(sd);
    
    if (it != faces_.end())
    {
        mfd_->removeFace(it->second);

        logger_->info("face {} removed for service {}", it->second, it->first->getUuid());
    }
    else
        logger_->warn("remove face error: no face found for discovered service {}", sd->getUuid());
}