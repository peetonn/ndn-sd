#ifndef __ndnapp_hpp__
#define __ndnapp_hpp__

#include <functional>
#include <set>
#include <string>

#include <spdlog/spdlog.h>
#include <ndn-sd/ndn-sd.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>

#include "identity-manager.hpp"

namespace ndnapp
{
    class App {
    public:
        typedef std::function<void(const std::shared_ptr<const ndnsd::NdnSd>&)> OnInstanceAnnouncement;
        typedef OnInstanceAnnouncement OnInstanceAdd;
        typedef OnInstanceAnnouncement OnInstanceRemove;

        App(std::string appName, std::string id, const std::shared_ptr<spdlog::logger>& logger,
            ndn::Face* face, ndn::KeyChain* keyChain, bool filterInterface = true);
        ~App() {}

        void setAddInstanceCallback(OnInstanceAdd onInstanceAdd) {
            onInstanceAdd_ = onInstanceAdd;
        }
        void setRemoveInstanceCallback(OnInstanceRemove onInstanceRemove) {
            onInstanceRemove_ = onInstanceRemove;
        }

        void configure(const std::vector<ndnsd::Proto>& protocols,
            const ndnsd::NdnSd::AdvertiseParameters& params, const std::string& signingIdentity = "",
            const std::string& password = "");

        void processEvents();

        ndntools::MicroForwarder* getMfd() const { return mfd_; }
        std::vector<std::shared_ptr<const ndnsd::NdnSd>> getDiscoveredNodes() const;
        std::string getAppName() const { return appName_; }
        std::string getInstanceId() const { return instanceId_; }

    private:
        std::string appName_;
        std::string instanceId_;
        std::shared_ptr<spdlog::logger> logger_;
        bool filterInterface_;
        std::set<std::string> discoveredInstances_;
        ndntools::MicroForwarder* mfd_;
        std::vector<ndnsd::Proto> protocols_;
        std::map<ndnsd::Proto, int> protocolPort_;
        ndnsd::NdnSd::AdvertiseParameters params_;
        std::vector<std::shared_ptr<ndnsd::NdnSd> > ndnsds_;
        std::map<std::shared_ptr<const ndnsd::NdnSd>, int> faces_;

        ndn::Face* face_;
        ndn::KeyChain* keyChain_;
        helpers::IdentityManager identityManager_;
        std::shared_ptr<ndn::MemoryContentCache> memCache_;

        OnInstanceAdd onInstanceAdd_;
        OnInstanceRemove onInstanceRemove_;

        void setupMicroforwarder();
        void setupNdnSd();
        void setupKeyChain() {}
        void setupCertificateAutoRenew(const std::shared_ptr<const ndn::CertificateV2>& cert,
            std::function<void()> renewRoutine);

        void addRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);
        void removeRoute(const std::shared_ptr<const ndnsd::NdnSd>& sd);

        void printAppInfo();
    };
}

#endif