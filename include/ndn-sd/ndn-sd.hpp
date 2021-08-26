
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

    typedef std::function<void(std::shared_ptr<const NdnSd>, void*)> OnResolvedService;
    typedef OnResolvedService OnDiscoveredService;
    typedef std::function<void(int, std::string, bool, void*)> OnError;

    /**
    * NDN Service Discovery class.
    * Provides functionality for advertising NDN service and discoverying link-local NDN services.
    * NDN-SD uses DNS-SD technology for muticast service discovery.
    */
    class NdnSd {
    public:
        typedef struct _Parameters {
            Proto protocol_;
            std::string subtype_;
            uint16_t port_;
            std::string prefix_;
            std::string cert_;
        } Parameters;

        NdnSd(std::string uuid);
        ~NdnSd();

        void advertise(const Parameters& parameters);
        void browse(Proto protocol, 
            OnDiscoveredService onResolvedServiceCb,
            OnError onBrowseErrorCb,
            uint32_t iface = 0, const char* domain = nullptr,
            void* userData = nullptr);
        void resolve(std::shared_ptr<const NdnSd> sd, 
            OnResolvedService onResolvedServiceCb,
            OnError onResolveErrorCb);

        // TODO: see if this should be static
        // timeout or till the first read
        int run(uint32_t timeoutMs = 0);

        Proto getProtocol() const;
        std::string getUuid() const;

        uint16_t getPort() const;
        std::string getDomain() const;
        int getInterface() const;
        std::string getPrefix() const;
        std::string getCertificate() const;   

        static std::string getVersion();

    private:
        struct Impl;
        std::shared_ptr<Impl> pimpl_;
    };
}
