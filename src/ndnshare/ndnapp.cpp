// TODO: add copyright

#include "ndnapp.hpp"

#include <ndn-ind/transport/udp-transport.hpp>
#include <ndn-ind/transport/tcp-transport.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

using namespace std;
using namespace ndn;
using namespace ndnsd;

namespace ndnapp
{

    void App::configure(const vector<Proto>& protocols, const NdnSd::AdvertiseParameters& params)
    {
        protocols_ = protocols;
        params_ = params;

        setupMicroforwarder();
        printAppInfo();

        setupNdnSd();
        setupKeyChain();
    }

    void App::setupMicroforwarder()
    {
        try
        {
            auto udpTransport = mfd_->addChannel(
                ndn::ptr_lib::make_shared<ndn::UdpTransport::ConnectionInfo>("", 0));

            if (!udpTransport)
                logger_->error("failed to add UDP listen channel");

            params_.port_ = udpTransport->getBoundPort();

            // TODO: remove this in production
            protocols_.clear();
            protocols_.push_back(Proto::UDP);
        }
        catch (exception& e)
        {
            logger_->error("caught exception setting up microforwarder: {}", e.what());
            throw e;
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
                    logger_->info("add {}/{} iface {}", sd->getUuid(), sd->getProtocol(), sd->getInterface());

                    if (filterInterface_ && discoveredInstances_.count(sd->getUuid()))
                        logger_->warn("iface filtering: skip duplicate {} on iface {}", sd->getUuid(), sd->getInterface());
                    else
                    {
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

    void App::printAppInfo()
    {
        fmt::print(R"(
    {}
        instance id {}
        announce NDN service: {}, subtype {}
        prefix {}
        listen port {}

)",
appName_,
fmt::format(fmt::emphasis::bold, "{}", instanceId_),
fmt::format(fmt::emphasis::bold, "{}", protocols_), params_.subtype_,
fmt::format(fmt::emphasis::bold, "{}", params_.prefix_),
fmt::format(fmt::emphasis::bold, "{}", params_.port_)
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

}