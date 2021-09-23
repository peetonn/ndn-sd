#ifndef __ndn_sd_hpp__
#define __ndn_sd_hpp__

#include <string>
#include <ndn-ind/interest.hpp>

namespace ndnsd 
{   
    class NdnSd;

    extern const std::string kNdnDnsServiceType;
    extern const std::string kNdnDnsServiceSubtypeMFD;
    extern const std::string kNdnDnsServiceSubtypeNFD;

    enum class Proto : uint8_t {
        UDP = 1,
        TCP = 1 << 1
    };
    inline std::ostream& operator<< (std::ostream& os, Proto p)
    {
        switch (p)
        {
        case Proto::TCP: return os << "TCP";
        case Proto::UDP: return os << "UDP";
        default: return os << "protocol {" << int(p) << '}';
        }
    }

    // service browsing/resolving announcements
    enum class Announcement {
        Added,
        Removed,
        Resolved
    };
    inline std::ostream& operator<< (std::ostream& os, Announcement a)
    {
        switch (a)
        {
        case Announcement::Added: return os << "Added";
        case Announcement::Removed: return os << "Removed";
        case Announcement::Resolved: return os << "Resolved";
        default: return os << "announcement {" << int(a) << '}';
        }
    }

    typedef std::function<void(int, Announcement, std::shared_ptr<const NdnSd>, void*)> OnServiceAnnouncement;
    typedef OnServiceAnnouncement OnResolvedService;
    typedef std::function<void(void*)> OnServiceRegistered;
    typedef std::function<void(int, int, std::string, bool, void*)> OnError;
    typedef OnError OnBrowseError;
    typedef OnError OnRegisterError;

    /**
    * NDN Service Discovery class.
    * Provides functionality for advertising NDN service and discoverying link-local NDN services.
    * NDN-SD uses DNS-SD technology for muticast service discovery.
    */
    class NdnSd {
    public:
        typedef struct _BrowseConstraints {
            Proto protocol_ = Proto::UDP;
            uint32_t interfaceIdx_ = 0;
            std::string subtype_;
            std::string domain_;
            void* userData_ = nullptr;
        } BrowseConstraints;

        typedef struct _AdvertiseParameters : BrowseConstraints {
            uint16_t port_;
            std::string prefix_;
            std::string cert_;
        } AdvertiseParameters;

        NdnSd(std::string uuid);
        ~NdnSd();

        int announce(const AdvertiseParameters& parameters,
            OnServiceRegistered onRegisteredCb,
            OnRegisterError onRegisterErrorCb);
        int browse(BrowseConstraints constraints,
            OnServiceAnnouncement onAnnouncementCb,
            OnBrowseError onBrowseErrorCb);
        void cancel(int requestId);
        void resolve(std::shared_ptr<const NdnSd> sd, 
            OnResolvedService onResolvedServiceCb,
            OnError onResolveErrorCb,
            void *userData = nullptr);

        // TODO: see if this should be static
        // timeout or till the first read
        int run(uint32_t timeoutMs = 0);

        Proto getProtocol() const;
        std::string getUuid() const;

        uint16_t getPort() const;
        int getInterface() const;
        std::string getSubtype() const;
        std::string getDomain() const;

        // for registered or discovered instances
        std::string getPrefix() const;
        std::string getCertificate() const;

        // only on resolved instances
        std::string getHostname() const;
        std::string getFullname() const;

        static std::string getVersion();

    private:
        struct Impl;
        std::shared_ptr<Impl> pimpl_;
    };
}

#endif // !__ndn_sd_hpp__