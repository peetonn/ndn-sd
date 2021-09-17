#include <iostream>
#include <csignal>
#include <map>

#include "config.hpp"

#include <spdlog/spdlog.h>
#include <docopt.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/ostream.h>

#include <ndn-ind/face.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>
#include <cnl-cpp/namespace.hpp>

#include <ndn-sd/ndn-sd.hpp>

using namespace std;

static const char USAGE[] =
R"(ndnshare.

    Usage:
      ndnshare <path> <prefix> --cert=<certificate> [--udp | --tcp] [--anchor=<trust_anchor>] [--id=<node-id>]
      ndnshare (-h | --help)
      ndnshare --version

    Options:
      -h --help                 Show this screen.
      --version                 Show version.
      --cert=<certificate>      Certificate used to sign data.
      --udp                     Advertise over Bonjour as UDP-only service.
      --tcp                     Advertise over Bonjour as TCP-only service.
      --anchor=<tust_anchor>    Trust anchor (certificate) used to verify connections and incoming data.
      --id=<node-id>            Custom node ID (generated, if not provided).
      --logfile=<file>          Log file (defaults to stdout if not provided).
      --quiet                   No output mode.
)";
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

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
}

int main (int argc, char **argv)
{
    map<string, docopt::value> args = docopt::docopt(USAGE, { argv + 1, argv + argc },
        true, NDNSHARE_VERSION);

    for (auto const& arg : args) {
        fmt::print("{:>40}: {}\n",
            fmt::format(fg(fmt::color::crimson) | fmt::emphasis::bold, arg.first),
            arg.second);
    }

    shared_ptr<ndnsd::NdnSd> ndnDnsService = make_shared<ndnsd::NdnSd>("uuid");
    ndn::CertificateV2 cert; // TODO: read from file
    ndnsd::NdnSd::AdvertiseParameters params;
    //params.cert_ = /*load from file*/;
    //params.port_ = /*pick randomly?*/;
    params.prefix_ = args["<prefix>"].asString();
    //params.protocol_ = /*pick protocol based on args["--udp"] and args["--tcp"]*/;
    params.subtype_ = ndnsd::kNdnDnsServiceSubtypeMFD;

    //ndnDnsService->announce(params,
    //    []()
    //{
    //    // good, proceed
    //}, []()
    //{
    //    // report error
    //});


	return 0;
}
