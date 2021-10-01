// TODO: add copyright

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

#include <docopt.h>
#include <ndn-ind/face.hpp>
#include <cnl-cpp/namespace.hpp>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>
#include <cli/cli.h>
#include <cli/loopscheduler.h>
#include <cli/clilocalsession.h>

#include <ndn-sd/ndn-sd.hpp>

#include "config.hpp"
#include "logging.hpp"
#include "ndnapp.hpp"
#include "mime.hpp"
#include "uuid.hpp"



using namespace std;
using namespace ndnsd;
using namespace ndn;
using namespace cnl_cpp;

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

vector<Proto> loadProtocols(const map<string, docopt::value>& args);
NdnSd::AdvertiseParameters loadParameters(const string& instanceId, const map<string, docopt::value>& args);

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
    NLOG_DEBUG("signal caught");
}

#include "mime.hpp"
class FilePublisher {
public:
    FilePublisher(std::string rootPath, std::string prefix, 
        ndn::Face *face, ndn::KeyChain* keyChain,
        std::shared_ptr<spdlog::logger> logger)
        : rootPath_(rootPath)
        , prefix_(prefix, keyChain)
        , prefixRegisterFailure_(false)
        , logger_(logger)
    {
        prefix_.setFace(face, [this](auto prefix) {
            logger_->error("failed to register prefix {}", prefix->toUri());
            prefixRegisterFailure_ = true;
        });
        prefix_.addOnObjectNeeded(bind(&FilePublisher::onObjectNeeded, this, 
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    ~FilePublisher() {}

    void processEvents() {
        if (prefixRegisterFailure_)
            throw std::runtime_error("failed to register prefix " + prefix_.getName().toUri());
    }

private:
    bool prefixRegisterFailure_;
    std::string rootPath_;
    std::shared_ptr<spdlog::logger> logger_;
    cnl_cpp::Namespace prefix_;

    bool onObjectNeeded(cnl_cpp::Namespace&, cnl_cpp::Namespace& neededNamespace, uint64_t);
};

bool FilePublisher::onObjectNeeded(Namespace& nmspc, Namespace& neededNamespace, uint64_t callbackId)
{
    assert(nmspc.getName().compare(neededNamespace.getName()) == -1);
    Name fileSuffix = neededNamespace.getName().getSubName(nmspc.getName().size());
    
    logger_->trace("request for {}", fileSuffix.toUri());

    filesystem::path filePath(rootPath_);
    filePath /= fileSuffix.toUri();

    if (filesystem::exists(filePath))
    {
        Namespace& fileNamespace = nmspc[fileSuffix];
        MetaInfo fileMeta;
        //fileMeta.setFreshnessPeriod(1000); // TODO: what freshness to use
        //fileNamespace.setNewDataMetaInfo(fileMeta);

        // TODO: this should be refactored for huge files (can't read all into memory)
        ifstream file(filePath, ios::binary | ios::ate);
        streamsize size = file.tellg();
        file.seekg(0, ios::beg);
        ptr_lib::shared_ptr<vector<uint8_t>> fileContents = ptr_lib::make_shared<vector<uint8_t>>(size);

        if (file.read((char*)fileContents->data(), size))
        {
            Blob fileBlob(fileContents, false);
            GeneralizedObjectHandler handler;
            handler.setObject(fileNamespace, fileBlob, mime::content_type(filePath.extension().string()));

            logger_->info("published {}", filePath.string());

            return true;
        }
        else
        {
            logger_->error("failed to read file {}", filePath.string());
        }
    }
    else
    {
        logger_->warn("file {} does not exist", filePath.string());
    }

    return false;
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

    ndnapp::App app("ndnshare", instanceId, spdlog::default_logger());
    
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

    // setup cli
    auto rootMenu = make_unique<cli::Menu>("cli");
    rootMenu->Insert("get", { "ndn_name" },
        [](ostream& os, string name) 
    {
        os << "will fetch " << name << endl;
    },
        "Fetch NDN generalized object");

    cli::Cli cli(move(rootMenu));
    cli.ExitAction([&](auto& out) { run = false; });

    cli::LoopScheduler runLoop;
    cli::CliLocalTerminalSession session(cli, runLoop, cout);

    thread cliThread([&]() {
        runLoop.Run();
    });

    // main runloop
    while (run)
    {
        app.processEvents();
        this_thread::sleep_for(chrono::milliseconds(5));
    }

    runLoop.Stop();
    cliThread.join();

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
