
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

const string ndnsd::kNdnDnsServiceType = "_ndn";
const string ndnsd::kNdnDnsServiceSubtypeMFD = "mfd";
const string ndnsd::kNdnDnsServiceSubtypeNFD = "nfd";

namespace ndnsd
{
	enum class ServiceState {
		Created = 0,
		Discovered,
		Resolved,
		Registering,
		Registered
	};

	const string kNdnDnsServiceTxtPrefixKey = "p";
	const string kNdnDnsServiceTxtCertificateKey = "c";

	struct NdnSd::Impl : enable_shared_from_this<NdnSd::Impl> 
	{
		typedef struct _ServiceParameters : AdvertiseParameters {
			string hostname_;
			string fullname_;

			struct _ServiceParameters& operator=(const AdvertiseParameters& ap)
			{
				*((AdvertiseParameters*)this) = ap;
				return *this;
			}
		} ServiceParameters;

		typedef struct _DnsRequest {
			int id_ = -1;
			DNSServiceRef serviceRef_;
			shared_ptr<Impl> pimpl_;
			OnBrowseError onError_;
			OnServiceAnnouncement onAnnouncement_;
		} DnsRequest;

		typedef struct _BrowseRequest : DnsRequest {
			BrowseConstraints constraints_;
			vector<shared_ptr<NdnSd>> discovered_;
		} BrowseRequest;

		typedef struct _ResolveRequest : DnsRequest {
			shared_ptr<NdnSd> sd_;
			void* userData_;
		} ResolveRequest;

		Impl(std::string uuid) {
			uuid_ = uuid; 
			state_ = ServiceState::Created;
		}
		~Impl(){ /*TODO: cleanup*/ }

		OnResolvedService onResolvedServiceCb_;

		ServiceState state_;
		string uuid_;
		ServiceParameters parameters_;

		DNSServiceRef advertisedRef_;
		OnServiceRegistered onRegistered_;
		OnRegisterError onRegisterError_;

		// all browse requests
		map<int, shared_ptr<BrowseRequest>> brequests_;
		// all resolution requests
		map<int, shared_ptr<ResolveRequest>> rrequests_;

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
		vector<shared_ptr<NdnSd>> getDiscoveredServices() const;
		void removeRequest(shared_ptr<DnsRequest> rr);

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
			for (auto it = pimpl_->fdServiceRefMap_.cbegin(), nextIt = it;
				it != pimpl_->fdServiceRefMap_.cend(); it = nextIt)
			{
				// this setup with nextIt is needed because fdServiceRefMap can be 
				// modified from within this loop (in callbacks invoked by DNSServiceProcessResult)
				++nextIt; 
				if (FD_ISSET(it->first, &readfds))
				{
					auto err = DNSServiceProcessResult(it->second);
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
	TXTRecordSetValue(&txtRecRef, kNdnDnsServiceTxtPrefixKey.c_str(),
		parameters.prefix_.size(), (void*)parameters.prefix_.data());

	if (parameters.cert_.size())
	{
		TXTRecordSetValue(&txtRecRef, kNdnDnsServiceTxtCertificateKey.c_str(), 
			parameters.cert_.size(), (void*)parameters.cert_.data());
	}

	DNSServiceRef dnsServiceRef;
	auto err = DNSServiceRegister(&dnsServiceRef, kDNSServiceFlagsNoAutoRename,
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
		pimpl_->removeRequest(it->second);
		pimpl_->brequests_.erase(it);
	}
}

void NdnSd::resolve(shared_ptr<const NdnSd> sd,
	OnResolvedService onResolvedServiceCb,
	OnError onResolveErrorCb,
	void* userData)
{

	auto allDiscovered = pimpl_->getDiscoveredServices();
	auto it = find(allDiscovered.begin(), allDiscovered.end(), sd);
	if (it == allDiscovered.end())
	{
		try
		{
			onResolveErrorCb(-1, -1, "service discovery instance is unknown", false, userData);
		}
		catch (runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
	else
	{
		shared_ptr<Impl::ResolveRequest> rr = make_shared<Impl::ResolveRequest>();

		DNSServiceRef dnsServiceRef;
		auto res = DNSServiceResolve(&dnsServiceRef, 0, 
			sd->getInterface(),
			sd->getUuid().c_str(),
			makeRegType(sd->getProtocol(), ""/*sd->getSubtype()*/).c_str(),
			sd->getDomain().c_str(),
			&Impl::resolveReply, 
			rr.get());

		if (res == kDNSServiceErr_NoError)
		{
			int rrId = (pimpl_->rrequests_.size() ? pimpl_->rrequests_.rbegin()->first + 1 : 1);
			assert(pimpl_->rrequests_.find(rrId) == pimpl_->rrequests_.end());

			rr->id_ = rrId;
			rr->pimpl_ = pimpl_;
			rr->onError_ = onResolveErrorCb;
			rr->onAnnouncement_ = onResolvedServiceCb;
			rr->userData_ = userData;
			rr->sd_ = *it;
			rr->serviceRef_ = dnsServiceRef;

			pimpl_->rrequests_[rrId] = rr;
			pimpl_->addRefToRunLoop(dnsServiceRef);
		}
		else
		{
			try
			{
				onResolveErrorCb(-1, res, dnsSdErrorMessage(res), true, userData);
			}
			catch (runtime_error& e)
			{
				cerr << "caught exception while calling user callback: " << e.what() << endl;
			}
		}
	}
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

string NdnSd::getHostname() const 
{
	if (pimpl_->state_ == ServiceState::Resolved)
		return pimpl_->parameters_.hostname_;

	return "";
}

string NdnSd::getFullname() const 
{
	if (pimpl_->state_ == ServiceState::Resolved)
		return pimpl_->parameters_.fullname_;

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

vector<shared_ptr<NdnSd>> NdnSd::Impl::getDiscoveredServices() const
{
	vector<shared_ptr<NdnSd>> all;

	for (auto r : brequests_)
		all.insert(all.end(),
			r.second->discovered_.begin(), r.second->discovered_.end());

	return all;
}

void NdnSd::Impl::removeRequest(shared_ptr<DnsRequest> r)
{
	if (r)
	{
		removeRefFromRunloop(r->serviceRef_);
		DNSServiceRefDeallocate(r->serviceRef_);
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
	ResolveRequest* rr = static_cast<ResolveRequest*>(context);

	if (!rr)
	{
		cerr << "DNSServiceRef has no ResolveRequest owner" << endl;
		return;
	}

	if (errorCode == kDNSServiceErr_NoError)
	{
		
		auto resolvedSdImpl = rr->sd_->pimpl_;
		resolvedSdImpl->parameters_.interfaceIdx_ = interfaceIndex;
		resolvedSdImpl->parameters_.port_ = ntohs(port);
		resolvedSdImpl->parameters_.hostname_ = hosttarget;
		resolvedSdImpl->parameters_.fullname_ = fullname;

		if (TXTRecordContainsKey(txtLen, txtRecord, kNdnDnsServiceTxtPrefixKey.c_str()))
		{
			uint8_t prefixLen = 0;
			auto prefixData = TXTRecordGetValuePtr(txtLen, txtRecord, kNdnDnsServiceTxtPrefixKey.c_str(),
				&prefixLen);

			resolvedSdImpl->state_ = ServiceState::Resolved;
			resolvedSdImpl->parameters_.prefix_ = string((const char*)prefixData, prefixLen);

			if (TXTRecordContainsKey(txtLen, txtRecord, kNdnDnsServiceTxtCertificateKey.c_str()))
			{
				uint8_t certLen = 0;
				auto certData = TXTRecordGetValuePtr(txtLen, txtRecord, kNdnDnsServiceTxtCertificateKey.c_str(),
					&certLen);

				resolvedSdImpl->parameters_.cert_ = string((const char*)certData, certLen);
			}

			cout << "RESOLVED " << resolvedSdImpl->parameters_.fullname_ 
					<< " " << resolvedSdImpl->parameters_.hostname_ << endl;

			rr->onAnnouncement_(rr->id_, Announcement::Resolved, rr->sd_, rr->userData_);
			rr->pimpl_->removeRequest(rr->pimpl_->rrequests_[rr->id_]);
			rr->pimpl_->rrequests_.erase(rr->id_);
		}
		else
		{
			try
			{
				rr->onError_(rr->id_, -1, "prefix data was not found in resolved service TXT record",
					false, rr->userData_);
			}
			catch (runtime_error& e)
			{
				cerr << "caught exception while calling user callback: " << e.what() << endl;
			}
		}

	}
	else
	{
		try
		{
			rr->onError_(rr->id_, errorCode, dnsSdErrorMessage(errorCode), true, rr->userData_);
		}
		catch (runtime_error& e)
		{
			cerr << "caught exception while calling user callback: " << e.what() << endl;
		}
	}
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