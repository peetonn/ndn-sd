#include <iostream>
#include <ndn-sd/ndn-sd.hpp>
#include "dns_sd.h"
#include <csignal>
#include <map>

using namespace std;

map<int, DNSServiceRef> dnsServiceFdMap;

void registerReply(
	DNSServiceRef                       sdRef,
	DNSServiceFlags                     flags,
	DNSServiceErrorType                 errorCode,
	const char* name,
	const char* regtype,
	const char* domain,
	void* context)
{
	if (errorCode != kDNSServiceErr_NoError)
		cerr << "REGISTER failure: " << errorCode << endl;
	else
		cout << "REGISTER " << name << " " << regtype << " " << domain << endl;
}

void resolveReply(
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
	if (errorCode != kDNSServiceErr_NoError)
	{
		cerr << "failed to resolve service: " << errorCode << endl;
	}
	else
	{
		cout << "RESOLVE " << fullname << " " << hosttarget << " " << ntohs(port) << endl;

		if (flags & kDNSServiceFlagsMoreComing)
			cout << "RESOLVE more..." << endl;
	}
}

void browseReply(
	DNSServiceRef                       sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	DNSServiceErrorType                 errorCode,
	const char* serviceName,
	const char* regtype,
	const char* replyDomain,
	void* context)
{
	if (errorCode == kDNSServiceErr_NoError)
	{
		if (flags & kDNSServiceFlagsAdd)
		{
			cout << "ADD " << serviceName << " " << interfaceIndex << " regtype " << regtype << " domain " << replyDomain << endl;

			DNSServiceRef dnsServiceRef;
			auto res = DNSServiceResolve(&dnsServiceRef, 0, interfaceIndex, serviceName, regtype, replyDomain, &resolveReply, nullptr);
			if (!res)
				dnsServiceFdMap[DNSServiceRefSockFD(dnsServiceRef)] = dnsServiceRef;
		}
		else
		{
			cout << "REMOVE " << serviceName << endl;
		}

		if (flags & kDNSServiceFlagsMoreComing)
			cout << "MORE..." << endl;
	}
	else
	{
		cout << "browsing error: " << errorCode << endl;
	}
}

atomic_bool run = true;
void signal_handler(int signal)
{
	run = !(signal == SIGINT);
}

int main (int argc, char **argv)
{
	
	cout << "NDN-SD version " << ndnsd::getVersionString() << endl;

	{ // register
		uint16_t txtBufLen = 512;
		uint8_t* txtBuf = (uint8_t*)malloc(txtBufLen);
		TXTRecordRef txtRecRef;
		TXTRecordCreate(&txtRecRef, txtBufLen, txtBuf);
		string prefix = "/touchndn/randomuuid";
		TXTRecordSetValue(&txtRecRef, "p", prefix.size(), (void*)prefix.data());
		string certBase64 = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyBXGZLp9OFSFuxzDZoEcCB1jL9X30Qf8wftT710AZjpUJuAFq0uWrC40qsl09VTantqrbPpMLVU6kz4rT+uyV/YeTXq6o/kI8FqOJu1pzSXd2jzLFl1dExkElYyxbsXFs/PZTtTe7Im2m3IQ1cxAvL+WitO8IwsHqx4uvaBzHNMoYh0JJAq1dty2mPbQ5Rf0jFVXIoWjRUHFs5A6in+aiTDOq3jyJyfDkHrDXzbSnhdPdoxPLF9umV7cHhiyWq1hwoK7OyySsQ0I4dFkLUeobLklNA6q0uqWSQQODoB765PbhT5nMjpFVa4SWTgsMVJFui5gOuYozPRW0hk5z0cSYwIDAQAB";
		TXTRecordSetValue(&txtRecRef, "c", certBase64.size(), (void*)certBase64.data());
		
		DNSServiceRef dnsServiceRef;
		auto err = DNSServiceRegister(&dnsServiceRef, 0, 0, "random-uuid", "_ndn._tcp,mfd",
			nullptr, nullptr, htons(45322), 
			TXTRecordGetLength(&txtRecRef), TXTRecordGetBytesPtr(&txtRecRef),
			&registerReply, nullptr);

		if (err != kDNSServiceErr_NoError)
		{
			cerr << "failed to register service: " << err << endl;
		}
		else
		{
			dnsServiceFdMap[DNSServiceRefSockFD(dnsServiceRef)] = dnsServiceRef;
		}
	}

	{ // browse
		DNSServiceRef dnsServiceRef;

		auto res = DNSServiceBrowse(&dnsServiceRef, 0, 0, "_ndn._tcp", nullptr, &browseReply, nullptr);

		if (res != kDNSServiceErr_NoError)
		{
			cerr << "failed to initialize browsing for services: " << res << endl;
		}
		else
		{
			dnsServiceFdMap[DNSServiceRefSockFD(dnsServiceRef)] = dnsServiceRef;
		}
	}

	while (run)
	{
		fd_set readfds;
		FD_ZERO(&readfds);

		for (auto it : dnsServiceFdMap)
			FD_SET(it.first, &readfds);

		int res = select(0, &readfds, (fd_set*)NULL, (fd_set*)NULL, 0);
		if (res > 0)
		{
			for (auto it : dnsServiceFdMap)
			{
				if (FD_ISSET(it.first, &readfds))
				{
					auto err = DNSServiceProcessResult(it.second);
					if (err)
						cerr << "process result error: " << err;
				}
			}

		}
	}

	for (auto it : dnsServiceFdMap)
		DNSServiceRefDeallocate(it.second);
	
	return 0;
}