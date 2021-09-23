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
// ***********

vector<Proto> loadProtocols(const map<string, docopt::value>& args);
NdnSd::AdvertiseParameters loadParameters(const string& instanceId, const map<string, docopt::value>& args);

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
    NLOG_DEBUG("signal caught");
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
    vector<shared_ptr<NdnSd>> ndnSds;
    
    fmt::print(R"(
    ndnshare v{}
        instance id {}
        announce NDN service: {}, subtype {}
        prefix {}
        listen port {}

)",
        fmt::format(fmt::emphasis::bold, "{}", NDNSHARE_VERSION),
        fmt::format(fmt::emphasis::bold, "{}", instanceId),
        fmt::format(fmt::emphasis::bold, "{}", protocols), params.subtype_,
        fmt::format(fmt::emphasis::bold, "{}", params.prefix_),
        fmt::format(fmt::emphasis::bold, "{}", params.port_)
    );

    // service setup
    NLOG_INFO("announcing services...");
    for (auto p : protocols)
    {
        NdnSd::AdvertiseParameters prm = params;
        prm.protocol_ = p;
        shared_ptr<NdnSd> s = make_shared<NdnSd>(instanceId);

        s->announce(prm,
            [s](void*)
        {
            NLOG_INFO("announce {}/{} iface {} {}:{} -- {}", s->getUuid(), s->getProtocol(),
                s->getInterface(), s->getDomain(), s->getPort(), s->getPrefix());
        },
            [s](int reqId, int errCode, std::string msg, bool, void*)
        {
            run = false;
            NLOG_ERROR("announce error {}/{}: {} - {}", s->getUuid(), s->getProtocol(), errCode, msg);
        });

        // TODO: browse constraints -- shall browse with empty subtype or our subtype?
        NdnSd::BrowseConstraints cnstr = static_cast<NdnSd::BrowseConstraints>(prm);
        s->browse(cnstr,
            [s](int, Announcement a, shared_ptr<const NdnSd> sd, void*)
        {
            if (a == Announcement::Added)
            {
                NLOG_INFO("add {}/{}", sd->getUuid(), sd->getProtocol());
                s->resolve(sd,
                    [](int, Announcement a, shared_ptr<const NdnSd> sd, void*)
                {
                    if (a == Announcement::Resolved)
                    {
                        NLOG_INFO("resolve {} iface {} {}://{}:{} -- {}", sd->getUuid(), sd->getInterface(),
                            sd->getProtocol(), sd->getHostname(), sd->getPort(), sd->getPrefix());
                    }
                },
                    [sd](int reqId, int errCode, std::string msg, bool ismDns, void*)
                {
                    NLOG_ERROR("resolve error {} (is mDNS {}): {} - {}", sd->getUuid(), 
                        ismDns, errCode, msg);
                });
            }

            if (a == Announcement::Removed)
                NLOG_INFO("remove {0}/{1}", sd->getUuid(), sd->getProtocol());
        },
            [](int reqId, int errCode, std::string msg, bool ismDns, void*)
        {
            NLOG_ERROR("add error (is mDNS {}): {} - {}", ismDns, errCode, msg);
        });

        ndnSds.push_back(s);
    }

    // main runloop
    while (run)
    {
        for (auto s : ndnSds)
            s->run(1);

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
    //params.port_ = /*pick randomly?*/;

    params.prefix_ = args.at("<prefix>").asString() + "/" + instanceId;
    params.subtype_ = kNdnDnsServiceSubtypeMFD;

    return params;
}
