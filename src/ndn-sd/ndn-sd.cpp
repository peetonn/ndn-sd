
#include "ndn-sd.hpp"
#include "dns_sd.h"
#include <ndn-ind/interest.hpp>
#include <map>
#include <time.h>

#ifdef _WIN32
#include <process.h>
typedef int pid_t;
#define getpid _getpid
#define strcasecmp _stricmp
#define snprint _snprintf
#else
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

using namespace std;

const std::string ndnsd::kNdnDnsServiceType = "_ndn";
const std::string ndnsd::kNdnDnsServiceSubtypeMFD = "mfd";
const std::string ndnsd::kNdnDnsServiceSubtypeNFD = "nfd";

namespace ndnsd
{
	enum class ServiceState {
		Created = 0,
		Discovered,
		Resolved,
		Published
	};

	struct NdnSd::Impl : enable_shared_from_this<NdnSd::Impl> {
		Impl(std::string uuid) {
			uuid_ = uuid; 
			state_ = ServiceState::Created;
		}
		~Impl(){ /*TODO: cleanup*/ }

		OnResolvedService onDiscoveredServiceCb_, onResolvedServiceCb_;
		OnError onBrowseErrorCb_;
		void* browseUserData_;
		
		int interafceIdx_;

		ServiceState state_;
		string uuid_;
		string domain_;
		Parameters parameters_;

		// store discovered-only services (not resolved yet)
		vector<shared_ptr<NdnSd>> discovered_;
		// store resolved services
		vector<shared_ptr<NdnSd>> resolved_;

		// TODO:see if this should be static
		map<int, DNSServiceRef> fdServiceRefMap_;

		// DNS-SD callbacks
		static void browseReply(
			DNSServiceRef                       sdRef,
			DNSServiceFlags                     flags,
			uint32_t                            interfaceIndex,
			DNSServiceErrorType                 errorCode,
			const char* serviceName,
			const char* regtype,
			const char* replyDomain,
			void* context);
		static void resolveReply(
			DNSServiceRef                       sdRef,
			DNSServiceFlags                     flags,
			uint32_t                            interfaceIndex,
			DNSServiceErrorType                 errorCode,
			const char* fullname,
			const char* hosttarget,
			uint16_t                            port,        /* In network byte order */
			uint16_t                            txtLen,
			const unsigned char* txtRecord,
			void* context);
		static void registerReply(
			DNSServiceRef                       sdRef,
			DNSServiceFlags                     flags,
			DNSServiceErrorType                 errorCode,
			const char* name,
			const char* regtype,
			const char* domain,
			void* context);
	};

	// helpers
	string makeRegType(Proto p);
	Proto parseProtocol(string regtype);
	string dnsSdErrorMessage(DNSServiceErrorType errorCode);
}

using ndnsd::NdnSd;

NdnSd::NdnSd(string uuid)
	: pimpl_(make_shared<NdnSd::Impl>(uuid))
{

}

NdnSd::~NdnSd()
{

}

int NdnSd::run(uint32_t timeoutMs)
{
	bool run = true;

	while (run)
	{
		fd_set readfds;
		FD_ZERO(&readfds);

		for (auto it : pimpl_->fdServiceRefMap_)
			FD_SET(it.first, &readfds);
		struct timeval tv;
		tv.tv_sec = timeoutMs / 1000;
		tv.tv_usec = (timeoutMs - (timeoutMs / 1000) * 1000) * 1000;

		int res = select(0, &readfds, (fd_set*)nullptr, (fd_set*)nullptr, (timeoutMs ? &tv : 0));
		if (res > 0)
		{
			for (auto it : pimpl_->fdServiceRefMap_)
			{
				if (FD_ISSET(it.first, &readfds))
				{
					auto err = DNSServiceProcessResult(it.second);
					if (err)
					{
						cerr << "process result error: " << err << endl;
						run = false;
						return err;
					}
				}
			}

			run = (timeoutMs == 0);
		}
		else
		{
			if (res != 0)
			{
				cerr << "select returned error: " << errno << " - " << strerror(errno) << endl;
				return errno;
			}

			run = false;
		}
	}

	return 0;
}

void NdnSd::advertise(const NdnSd::Parameters& parameters)
{

}

void NdnSd::browse(Proto protocol, OnDiscoveredService onDiscoveredServiceCb, 
	OnError onBrowseErrorCb, uint32_t iface, const char* domain, 
	void *userData)
{
	DNSServiceRef ref;
	
	auto res = DNSServiceBrowse(&ref, 0, iface, makeRegType(protocol).c_str(), domain,
		&NdnSd::Impl::browseReply, pimpl_.get());

	if (res != kDNSServiceErr_NoError)
	{
		try
		{
			onBrowseErrorCb(res, dnsSdErrorMessage(res), true, userData);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
	else
	{
		pimpl_->onDiscoveredServiceCb_ = onDiscoveredServiceCb;
		pimpl_->browseUserData_ = userData;
		pimpl_->fdServiceRefMap_[DNSServiceRefSockFD(ref)] = ref;
	}
}

void NdnSd::resolve(shared_ptr<const NdnSd> sd,
	OnResolvedService onResolvedServiceCb,
	OnError onResolveErrorCb)
{

}

std::string NdnSd::getUuid() const
{
	return pimpl_->uuid_;
}

ndnsd::Proto NdnSd::getProtocol() const
{
	return pimpl_->parameters_.protocol_;
}

uint16_t NdnSd::getPort() const
{
	if (pimpl_->state_ == ServiceState::Published ||
		pimpl_->state_ == ServiceState::Resolved)
		return pimpl_->parameters_.port_;
	return 0;
}

std::string NdnSd::getPrefix() const
{
	if (pimpl_->state_ == ServiceState::Published ||
		pimpl_->state_ == ServiceState::Resolved)
		return pimpl_->parameters_.prefix_;
	return "";
}

std::string NdnSd::getCertificate() const
{
	if (pimpl_->state_ == ServiceState::Published ||
		pimpl_->state_ == ServiceState::Resolved)
		return pimpl_->parameters_.cert_;
	return "";
}

std::string NdnSd::getDomain() const
{
	if (pimpl_->state_ > ServiceState::Created)
		return pimpl_->domain_;
	return "";
}

int NdnSd::getInterface() const 
{
	return pimpl_->interafceIdx_;
}

std::string NdnSd::getVersion()
{
	return "x.x.x";
}

// DNS-SD callbacks
void NdnSd::Impl::browseReply(
	DNSServiceRef                       sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	DNSServiceErrorType                 errorCode,
	const char* serviceName,
	const char* regtype,
	const char* replyDomain,
	void* context)
{
	NdnSd::Impl* browser = static_cast<NdnSd::Impl*>(context);

	if (!browser)
	{
		cerr << "DNSServiceRef has no NdnSd owner" << endl;
		return;
	}
		
	if (errorCode == kDNSServiceErr_NoError)
	{
		cout << "ADD " << serviceName << " " << regtype << " " << replyDomain << endl;
		// TODO: 
		// 1. check it's not our own
		// 2. check if more coming and postpone callback
		// 

		if (browser->uuid_ != serviceName)
		{
			shared_ptr<NdnSd> sd = make_shared<NdnSd>(serviceName);
			sd->pimpl_->interafceIdx_ = interfaceIndex;
			sd->pimpl_->domain_ = replyDomain;
			sd->pimpl_->parameters_.protocol_ = parseProtocol(regtype);
			sd->pimpl_->state_ = ServiceState::Discovered;

			browser->discovered_.push_back(sd);
			browser->onDiscoveredServiceCb_(sd, browser->browseUserData_);
		}
	}
	else
	{
		browser->onBrowseErrorCb_(errorCode, dnsSdErrorMessage(errorCode), true,
			browser->browseUserData_);
	}
}

void NdnSd::Impl::resolveReply(
	DNSServiceRef                       sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	DNSServiceErrorType                 errorCode,
	const char* fullname,
	const char* hosttarget,
	uint16_t                            port,        /* In network byte order */
	uint16_t                            txtLen,
	const unsigned char* txtRecord,
	void* context)
{

}

void NdnSd::Impl::registerReply(
	DNSServiceRef                       sdRef,
	DNSServiceFlags                     flags,
	DNSServiceErrorType                 errorCode,
	const char* name,
	const char* regtype,
	const char* domain,
	void* context)
{

}

//*** helpers
string ndnsd::makeRegType(Proto p)
{
	switch (p)
	{
	case ndnsd::Proto::UDP:
		return kNdnDnsServiceType + "._udp";
	case ndnsd::Proto::TCP:
	default:
		return kNdnDnsServiceType + "._tcp";
	}

	return "";
}

ndnsd::Proto ndnsd::parseProtocol(string regtype)
{
	if (regtype.find("udp") != string::npos)
		return ndnsd::Proto::UDP;
	return ndnsd::Proto::TCP;
}

string ndnsd::dnsSdErrorMessage(DNSServiceErrorType errorCode)
{
	static const map<DNSServiceErrorType, string> errorCodesMap = {
		{ kDNSServiceErr_NoError, "no error" },
		{ kDNSServiceErr_Unknown, "unknown error" },
		{ kDNSServiceErr_NoSuchName, "no such name" },
		{ kDNSServiceErr_NoMemory, "out of memory" },
		{ kDNSServiceErr_BadParam, "bad parameter" },
		{ kDNSServiceErr_BadReference, "bad reference" },
		{ kDNSServiceErr_BadState, "internal error" },
		{ kDNSServiceErr_BadFlags, "invalid values of flags" },
		{ kDNSServiceErr_Unsupported, "operation not supported" },
		{ kDNSServiceErr_NotInitialized, "reference not initialized" },
		{ kDNSServiceErr_AlreadyRegistered, "attempt to register a service that is registered" },
		{ kDNSServiceErr_NameConflict, "attempt to register a service with an already used name" },
		{ kDNSServiceErr_Invalid, "invalid parameter data" },
		{ kDNSServiceErr_Firewall, "firewall" },
		{ kDNSServiceErr_Incompatible, "client library incompatible with daemon" },
		{ kDNSServiceErr_BadInterfaceIndex, "specified interace does not exist" },
		{ kDNSServiceErr_Refused, "refused" },
		{ kDNSServiceErr_NoSuchRecord, "no such record" },
		{ kDNSServiceErr_NoAuth, "no auth" },
		{ kDNSServiceErr_NoSuchKey, "no such key" },
		{ kDNSServiceErr_NATTraversal, "NAT traversal" },
		{ kDNSServiceErr_DoubleNAT, "double NAT" },
		{ kDNSServiceErr_BadTime, "bad time" },
		{ kDNSServiceErr_BadSig, "bad signature" },
		{ kDNSServiceErr_BadKey, "bad key" },
		{ kDNSServiceErr_Transient, "transient" },
		{ kDNSServiceErr_ServiceNotRunning, "background daemon not running" },
		{ kDNSServiceErr_NATPortMappingUnsupported, "NAT doesn't support NAT-PMP or UPnP" },
		{ kDNSServiceErr_NATPortMappingDisabled, "NAT supports NAT-PMP or UPnP but it's disabled by the administrator" },
		{ kDNSServiceErr_NoRouter, "no router currently configured (probably no network connectivity)" },
		{ kDNSServiceErr_PollingMode, "polling mode" },
		{ kDNSServiceErr_Timeout, "timeout" }
	};

	if (errorCodesMap.find(errorCode) != errorCodesMap.end())
		return errorCodesMap.at(errorCode);

	return "unknown error code";
}
