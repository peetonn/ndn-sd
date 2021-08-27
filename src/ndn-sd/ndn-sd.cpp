
#include "ndn-sd.hpp"
#include "dns_sd.h"
#include <ndn-ind/interest.hpp>
#include <map>
#include <time.h>
#include <cassert>

#define MAX_TXT_RECORD_SIZE 1000

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
		Registering,
		Registered
	};

	struct NdnSd::Impl : enable_shared_from_this<NdnSd::Impl> 
	{
		typedef struct _BrowseRequest {
			int id_ = -1;
			DNSServiceRef serviceRef_;
			shared_ptr<Impl> pimpl_;
			BrowseConstraints constraints_;
			vector<shared_ptr<NdnSd>> discovered_;
			OnBrowseError onError_;
			OnServiceAnnouncement onAnnouncement_;
		} BrowseRequest;

		Impl(std::string uuid) {
			uuid_ = uuid; 
			state_ = ServiceState::Created;
		}
		~Impl(){ /*TODO: cleanup*/ }

		OnResolvedService onResolvedServiceCb_;

		ServiceState state_;
		string uuid_;
		AdvertiseParameters parameters_;

		DNSServiceRef advertisedRef_;
		OnServiceRegistered onRegistered_;
		OnRegisterError onRegisterError_;

		// all browse requests
		map<int, shared_ptr<BrowseRequest>> brequests_;

		// store discovered-only services (not resolved yet)
		//vector<shared_ptr<NdnSd>> discovered_;
		// store resolved services
		//vector<shared_ptr<NdnSd>> resolved_;

		// TODO:see if this should be static
		map<int, DNSServiceRef> fdServiceRefMap_;

		// helpers
		void deregister();
		void addRefToRunLoop(DNSServiceRef ref);
		void removeRefFromRunloop(DNSServiceRef ref);

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
	string makeRegType(Proto p, string subtype = "");
	Proto parseProtocol(string regtype);
	string dnsSdErrorMessage(DNSServiceErrorType errorCode);
	bool isMaxTxtSizeExceeded(size_t sz);
}

using ndnsd::NdnSd;

NdnSd::NdnSd(string uuid)
	: pimpl_(make_shared<NdnSd::Impl>(uuid))
{

}

NdnSd::~NdnSd()
{
	// cancel all requests
	for (auto it = pimpl_->brequests_.cbegin(), nextIt = it;
		it != pimpl_->brequests_.cend(); it = nextIt)
	{
		++nextIt;
		pimpl_->removeRefFromRunloop(it->second->serviceRef_);
		DNSServiceRefDeallocate(it->second->serviceRef_);
		pimpl_->brequests_.erase(it);
	}

	// deregister if registered
	pimpl_->deregister();
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

			// keep running if timeout is not set
			run = (timeoutMs == 0);
		}
		else
		{
			if (res < 0)
			{
				cerr << "select returned (" << res << ") error: " << errno << " - " << strerror(errno) << endl;
				return errno;
			}

			// timeout reached, stop
			run = false;
		}
	}

	return 0;
}

int NdnSd::announce(const AdvertiseParameters& parameters,
	OnServiceRegistered onRegisteredCb,
	OnRegisterError onRegisterErrorCb)
{
	if (pimpl_->state_ != ServiceState::Created)
	{
		try
		{
			onRegisterErrorCb(-1, -1, "service is already registered or being registered", false, parameters.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}

		return -1;
	}

	if (!parameters.prefix_.size() || !parameters.port_)
	{
		try 
		{
			onRegisterErrorCb(-1, -1, "prefix or port number is not set", false, parameters.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}

	if (isMaxTxtSizeExceeded(parameters.prefix_.size() + parameters.cert_.size()))
	{
		try
		{
			onRegisterErrorCb(-1, -1, "maximum TXT record size exceeded", false, parameters.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
		
		return -1;
	}

	// prep TXT record
	uint16_t txtBufLen = MAX_TXT_RECORD_SIZE;
	uint8_t* txtBuf = (uint8_t*)malloc(txtBufLen);
	TXTRecordRef txtRecRef;
	TXTRecordCreate(&txtRecRef, txtBufLen, txtBuf);
	TXTRecordSetValue(&txtRecRef, "p", parameters.prefix_.size(), (void*)parameters.prefix_.data());

	if (parameters.cert_.size())
	{
		TXTRecordSetValue(&txtRecRef, "c", parameters.cert_.size(), (void*)parameters.cert_.data());
	}

	DNSServiceRef dnsServiceRef;
	auto err = DNSServiceRegister(&dnsServiceRef, 0, 
		parameters.interfaceIdx_, pimpl_->uuid_.c_str(), 
		makeRegType(parameters.protocol_, parameters.subtype_).c_str(),
		(parameters.domain_.size() ? parameters.domain_.c_str() : nullptr), 
		nullptr, 
		htons(parameters.port_),
		TXTRecordGetLength(&txtRecRef), TXTRecordGetBytesPtr(&txtRecRef),
		&Impl::registerReply, pimpl_.get());
	free(txtBuf);

	if (err != kDNSServiceErr_NoError)
	{
		cerr << "failed to register service: " << err << endl;
		try
		{
			onRegisterErrorCb(-1, err, dnsSdErrorMessage(err), true, parameters.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
	else
	{
		pimpl_->state_ = ServiceState::Registering;
		pimpl_->advertisedRef_ = dnsServiceRef;
		pimpl_->parameters_ = parameters;
		pimpl_->onRegistered_ = onRegisteredCb;
		pimpl_->onRegisterError_ = onRegisterErrorCb;
		pimpl_->addRefToRunLoop(dnsServiceRef);
	}

	return 0;
}

int NdnSd::browse(BrowseConstraints constraints, OnServiceAnnouncement onAnnouncementCb,
	OnError onBrowseErrorCb)
{
	DNSServiceRef ref;
	shared_ptr<Impl::BrowseRequest> br = make_shared<Impl::BrowseRequest>();
	
	auto res = DNSServiceBrowse(&ref, 0, constraints.interfaceIdx_, 
		makeRegType(constraints.protocol_, constraints.subtype_).c_str(), 
		(constraints.domain_.size() ? constraints.domain_.c_str() : nullptr),
		&NdnSd::Impl::browseReply, br.get());

	if (res != kDNSServiceErr_NoError)
	{
		try
		{
			onBrowseErrorCb(-1, res, dnsSdErrorMessage(res), true, constraints.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
	else
	{
		int brId = (pimpl_->brequests_.size() ? pimpl_->brequests_.rbegin()->first + 1 : 1);
		assert(pimpl_->brequests_.find(brId) == pimpl_->brequests_.end());

		br->pimpl_ = pimpl_;
		br->id_ = brId;
		br->serviceRef_ = ref;
		br->constraints_ = constraints;
		br->onAnnouncement_ = onAnnouncementCb;
		br->onError_ = onBrowseErrorCb;

		pimpl_->brequests_[brId] = br;
		pimpl_->addRefToRunLoop(ref);

		return brId;
	}

	return -1;
}

void NdnSd::cancel(int requestId)
{
	auto it = pimpl_->brequests_.find(requestId);

	if (it != pimpl_->brequests_.end())
	{
		// 1. remove fd from runloop
		pimpl_->removeRefFromRunloop(it->second->serviceRef_);

		// 2. deallocate dns service ref
		DNSServiceRefDeallocate(it->second->serviceRef_);

		// 3. remove request from the request dict
		pimpl_->brequests_.erase(it);
	}
}

void NdnSd::resolve(shared_ptr<const NdnSd> sd,
	OnResolvedService onResolvedServiceCb,
	OnError onResolveErrorCb)
{

}

string NdnSd::getUuid() const
{
	return pimpl_->uuid_;
}

ndnsd::Proto NdnSd::getProtocol() const
{
	return pimpl_->parameters_.protocol_;
}

int NdnSd::getInterface() const
{
	return pimpl_->parameters_.interfaceIdx_;
}

uint16_t NdnSd::getPort() const
{
	if (pimpl_->state_ >= ServiceState::Resolved)
		return pimpl_->parameters_.port_;
	return 0;
}

string NdnSd::getSubtype() const 
{
	return pimpl_->parameters_.subtype_;
}

string NdnSd::getPrefix() const
{
	if (pimpl_->state_ >= ServiceState::Resolved)
		return pimpl_->parameters_.prefix_;
	return "";
}

string NdnSd::getCertificate() const
{
	if (pimpl_->state_ >= ServiceState::Resolved)
		return pimpl_->parameters_.cert_;
	return "";
}

string NdnSd::getDomain() const
{
	if (pimpl_->state_ > ServiceState::Created)
		return pimpl_->parameters_.domain_;
	return "";
}

string NdnSd::getVersion()
{
	return "x.x.x";
}

// Impl helpers
void NdnSd::Impl::deregister()
{
	if (state_ >= ServiceState::Registering)
	{
		// TODO: service fd might be under select (in multi-threaded setup). 
		// need to handle
		removeRefFromRunloop(advertisedRef_);
		DNSServiceRefDeallocate(advertisedRef_);
		state_ = ServiceState::Created;
	}
}

void NdnSd::Impl::addRefToRunLoop(DNSServiceRef ref)
{
	// TODO: do mutex access here
	fdServiceRefMap_[DNSServiceRefSockFD(ref)] = ref;
}

void NdnSd::Impl::removeRefFromRunloop(DNSServiceRef ref)
{
	auto it = fdServiceRefMap_.find(DNSServiceRefSockFD(ref));
	if (it != fdServiceRefMap_.end())
	{
		// TODO: do mutex access here
		fdServiceRefMap_.erase(it);
	}
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
	BrowseRequest* br = static_cast<BrowseRequest*>(context);

	if (!br)
	{
		cerr << "DNSServiceRef has no BrowseRequest owner" << endl;
		return;
	}
		
	if (errorCode == kDNSServiceErr_NoError)
	{
		if (flags & kDNSServiceFlagsAdd)
		{
			// TODO: 
			// 1. check it's not our own
			// 2. check if more coming and postpone callback
			// 
			//cout << "ADD " << serviceName << " " << regtype << endl;

			if (br->pimpl_->uuid_ != serviceName)
			{
				// TODO: wrap in a method
				shared_ptr<NdnSd> sd = make_shared<NdnSd>(serviceName);
				sd->pimpl_->parameters_.protocol_ = parseProtocol(regtype);
				sd->pimpl_->parameters_.domain_ = replyDomain;
				sd->pimpl_->parameters_.interfaceIdx_ = interfaceIndex;
				sd->pimpl_->parameters_.subtype_ = br->constraints_.subtype_;
				sd->pimpl_->parameters_.userData_ = br->constraints_.userData_;

				sd->pimpl_->state_ = ServiceState::Discovered;
				br->discovered_.push_back(sd);
				try
				{
					br->onAnnouncement_(br->id_, Announcement::Added, sd, br->constraints_.userData_);
				}
				catch (std::runtime_error& e)
				{
					cerr << "caught exception while calling user callback: " << e.what() << endl;
				}
			}
		}
		else
		{
			auto it = find_if(br->discovered_.begin(), br->discovered_.end(),
				[&](auto sd) 
			{
				return sd->getUuid() == serviceName;
			});

			if (it != br->discovered_.end())
			{
				// TODO: check if it has been resolved and cleanup FD and other structs
				auto sd = *it;
				br->discovered_.erase(it);

				try
				{
					br->onAnnouncement_(br->id_, Announcement::Removed, sd, br->constraints_.userData_);
				}
				catch (std::runtime_error& e)
				{
					cerr << "caught exception while calling user callback: " << e.what() << endl;
				}
			}
		}
	}
	else
	{
		try
		{
			br->onError_(br->id_, errorCode, dnsSdErrorMessage(errorCode), true,
				br->constraints_.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
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
	NdnSd::Impl* pimpl = static_cast<NdnSd::Impl*>(context);

	if (!pimpl)
	{
		cerr << "FATAL: registerReply: no impl pointer provided" << endl;
		return;
	}

	if (errorCode == kDNSServiceErr_NoError)
	{
		assert(pimpl->uuid_ == name);
		assert(makeRegType(pimpl->parameters_.protocol_) == regtype);

		try 
		{
			pimpl->state_ = ServiceState::Registered;
			pimpl->parameters_.domain_ = domain;
			pimpl->onRegistered_(pimpl->parameters_.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
	else
	{
		try
		{
			pimpl->onRegisterError_(-1, errorCode, dnsSdErrorMessage(errorCode), true,
				pimpl->parameters_.userData_);
		}
		catch (std::runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
}

//*** helpers
string ndnsd::makeRegType(Proto p, string subtype)
{
	string regtype;
	switch (p)
	{
	case ndnsd::Proto::UDP:
		regtype = kNdnDnsServiceType + "._udp";
		break;
	case ndnsd::Proto::TCP: // fallthrough
	default:
		regtype = kNdnDnsServiceType + "._tcp";
	}

	if (subtype.size())
		regtype += "," + subtype;
	else
		regtype += ".";

	return regtype;
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

bool ndnsd::isMaxTxtSizeExceeded(size_t sz)
{
	return (sz + 4 > MAX_TXT_RECORD_SIZE);
}