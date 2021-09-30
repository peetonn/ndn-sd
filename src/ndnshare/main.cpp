#include <iostream>
#include <csignal>
#include <map>

#include "config.hpp"
#include "logging.hpp"

#include <docopt.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <ndn-ind/face.hpp>
#include <ndn-ind/transport/udp-transport.hpp>
#include <ndn-ind/transport/tcp-transport.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>
#include <cnl-cpp/namespace.hpp>

#include <ndn-sd/ndn-sd.hpp>

using namespace std;
using namespace ndnsd;
using namespace ndn;

static const char USAGE[] =
//R"(ndnshare.
//
//    Usage:
//      ndnshare <path> <prefix> --cert=<certificate> [--udp | --tcp] [--quiet] [--anchor=<trust_anchor>] [--id=<node_id>] [--logfile=<file>]
//      ndnshare (-h | --help)
//      ndnshare --version
//
//    Options:
//      -h --help                 Show this screen.
//      --version                 Show version.
//      --cert=<certificate>      Certificate used to sign data.
//      --udp                     Advertise over Bonjour as UDP-only service.
//      --tcp                     Advertise over Bonjour as TCP-only service.
//      --anchor=<tust_anchor>    Trust anchor (certificate) used to verify connections and incoming data.
//      --id=<node_id>            Custom node ID (generated, if not provided).
//      --logfile=<file>          Log file(defaults to stdout if not provided).
//      --quiet                   No output mode.
//)";
R"(ndnshare.

    Usage:
      ndnshare <path> <prefix> --cert=<certificate> [--id=<node_id>] [--anchor=<trust_anchor>] [--logfile=<log_file>] [--tcp | --udp]
      ndnshare get <prefix>
      ndnshare browse
      ndnshare (-h | --help)
      ndnshare --version

    Options:
      -h --help                 Show this screen.
      --version                 Show version.
      --cert=<certificate>      Certificate used to sign data.
      --id=<node_id>            Custom node ID (generated, if not provided).
      --anchor=<tust_anchor>    Trust anchor (certificate) used to verify connections and incoming data.
      --logfile=<log_file>      Log file(defaults to stdout if not provided).
      -t, --tcp                 Advertise over Bonjour as TCP-only service.
      -u, --udp                 Advertise over Bonjour as UDP-only service.
)";
//--logfile = <file>          Log file(defaults to stdout if not provided).
//--quiet                   No output mode.
//R"(Naval Fate.
//
//    Usage:
//      naval_fate ship new <name>...
//      naval_fate ship <name> move <x> <y> [--speed=<kn>]
//      naval_fate ship shoot <x> <y>
//      naval_fate mine (set|remove) <x> <y> [--moored | --drifting]
//      naval_fate (-h | --help)
//      naval_fate --version
//
//    Options:
//      -h --help     Show this screen.
//      --version     Show version.
//      --speed=<kn>  Speed in knots [default: 10].
//      --moored      Moored (anchored) mine.
//      --drifting    Drifting mine.
//)";

// uuid v4 generation
#include <random>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>

namespace uuid {

    auto randomlySeededMersenneTwister() {
        // Magic number 624: The number of unsigned ints the MT uses as state
        std::vector<unsigned int> random_data(624);
        std::random_device source;
        std::generate(begin(random_data), end(random_data), [&]() {return source(); });
        std::seed_seq seeds(begin(random_data), end(random_data));
        std::mt19937 seededEngine(seeds);
        return seededEngine;
    }

    static std::mt19937                    gen(randomlySeededMersenneTwister());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::string generate_uuid_v4() {
        std::stringstream ss;
        int i;
        ss << std::hex;
        for (i = 0; i < 8; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 4; i++) {
            ss << dis(gen);
        }
        ss << "-4";
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 12; i++) {
            ss << dis(gen);
        };
        return ss.str();
    }
}

vector<Proto> loadProtocols(const map<string, docopt::value>& args);
NdnSd::AdvertiseParameters loadParameters(const string& instanceId, const map<string, docopt::value>& args);

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
    NLOG_DEBUG("signal caught");
}

class App {
public:
    typedef std::function<void(const std::shared_ptr<const ndnsd::NdnSd>&)> OnInstanceAnnouncement;
    typedef OnInstanceAnnouncement OnInstanceAdd;
    typedef OnInstanceAnnouncement OnInstanceRemove;

    App(std::string appName, std::string id, const std::shared_ptr<spdlog::logger>& logger, 
        bool filterInterface = true) 
        : appName_(appName)
        , instanceId_(id)
        , logger_(logger)
        , mfd_(ndntools::MicroForwarder::get())
        , filterInterface_(filterInterface)
    {}
    ~App() {}

    void setAddInstanceCallback(OnInstanceAdd onInstanceAdd) {
        onInstanceAdd_ = onInstanceAdd;
    }
    void setRemoveInstanceCallback(OnInstanceRemove onInstanceRemove) {
        onInstanceRemove_ = onInstanceRemove;
    }

    void setup(const std::vector<ndnsd::Proto>& protocols,
        const ndnsd::NdnSd::AdvertiseParameters& params);
    void processEvents();

private:
    std::string appName_;
    std::string instanceId_;
    std::shared_ptr<spdlog::logger> logger_;
    bool filterInterface_;
    std::set<std::string> discoveredInstances_;
    ndntools::MicroForwarder* mfd_;
    std::vector<ndnsd::Proto> protocols_;
    ndnsd::NdnSd::AdvertiseParameters params_;
    std::vector<std::shared_ptr<ndnsd::NdnSd> > ndnsds_;
    OnInstanceAdd onInstanceAdd_;
    OnInstanceRemove onInstanceRemove_;

    void setupMicroforwarder();
    void setupNdnSd();
    void setupKeyChain(){}

    void addRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);
    void removeRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);

    void printAppInfo();
};

void App::setup(const vector<Proto>& protocols, const NdnSd::AdvertiseParameters& params)
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
        auto udpTransport = ndn::ptr_lib::make_shared<ndn::UdpTransport>();
        bool res = mfd_->addChannel(udpTransport,
            ndn::ptr_lib::make_shared<ndn::UdpTransport::ConnectionInfo>("", 0));

        if (!res)
            logger_->error("failed to add UDP listen channel");

        params_.port_ = udpTransport->getBoundPort();

        // TODO: remove this in production
        protocols_.clear();
        protocols_.push_back(Proto::UDP);
    }
    catch (exception& e)
    {
        run = false;
        NLOG_ERROR("caught exception setting up microforwarder: {}", e.what());
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
                run = false;
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
                    logger_->warn("iface filtering is ON: skip duplicate {} on iface {}", sd->getUuid(), sd->getInterface());
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
                        [sd](int reqId, int errCode, std::string msg, bool ismDns, void*)
                    {
                        NLOG_ERROR("resolve error {} (is mDNS {}): {} - {}", sd->getUuid(),
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
    {} v{}
        instance id {}
        announce NDN service: {}, subtype {}
        prefix {}
        listen port {}

)",
appName_,
fmt::format(fmt::emphasis::bold, "{}", NDNSHARE_VERSION),
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

}

int main (int argc, char **argv)
{
    signal(SIGINT, signal_handler);
    spdlog::set_level(spdlog::level::trace);

    map<string, docopt::value> args;
    try {
        args = docopt::docopt(USAGE, { argv + 1, argv + argc },
            true, NDNSHARE_VERSION);
    }
    catch (std::exception& e)
    {
        NLOG_ERROR("exception parsing command-line arguments: {} - {}", typeid(e).name(), e.what());
        return -1;
    }

#if 0
    for (auto const& arg : args) {
        fmt::print("{:>40}: {}\n",
            fmt::format(fg(fmt::color::crimson) | fmt::emphasis::bold, arg.first),
            arg.second);
    }
#endif

    string instanceId = (args["--id"] ? args["--id"].asString() : uuid::generate_uuid_v4());
    vector<Proto> protocols = loadProtocols(args);
    NdnSd::AdvertiseParameters params = loadParameters(instanceId, args);

    App app("ndnshare", instanceId, spdlog::default_logger());
    
    app.setAddInstanceCallback([](auto sd) {
        NLOG_DEBUG("will setup face here for {}/{} {}:{}", 
            sd->getUuid(), sd->getProtocol(),
            sd->getHostname(), sd->getPort());

    });
    app.setRemoveInstanceCallback([](auto sd) {
        // TODO: remove face or leave it?
        NLOG_DEBUG("will remove face, maybe?");
    });
    
    app.setup(protocols, params);

    // main runloop
    while (run)
    {
        app.processEvents();
        
        // TODO: face process events here
        // TODO: folder watch here

        this_thread::sleep_for(chrono::milliseconds(5));
    }

    NLOG_INFO("shutting down.");
	return 0;
}

// helper code
vector<Proto> loadProtocols(const map<string, docopt::value>& args)
{
    vector<Proto> protocols;

    if (!args.at("--tcp") && !args.at("--udp"))
    {
        protocols.push_back(Proto::TCP);
        protocols.push_back(Proto::UDP);
    }
    else
    {
        if (args.at("--tcp"))
            protocols.push_back(Proto::TCP);
        if (args.at("--udp"))
            protocols.push_back(Proto::UDP);
    }
    return protocols;
}

NdnSd::AdvertiseParameters loadParameters(const string& instanceId, const map<string, docopt::value>& args)
{
    NdnSd::AdvertiseParameters params;

    ndn::CertificateV2 cert; // TODO: read from file

    //params.cert_ = /*load from file*/;
    params.port_ = 0;
    params.prefix_ = args.at("<prefix>").asString() + "/" + instanceId;
    params.subtype_ = kNdnDnsServiceSubtypeMFD;

    return params;
}
