#ifndef __ndnapp_hpp__
#define __ndnapp_hpp__

#include <functional>
#include <set>
#include <string>

#include <spdlog/spdlog.h>
#include <ndn-sd/ndn-sd.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>

namespace ndnapp
{
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

        ndntools::MicroForwarder* getMfd() const { return mfd_; }

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
        void setupKeyChain() {}

        void addRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);
        void removeRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);

        void printAppInfo();
    };
}

#endif