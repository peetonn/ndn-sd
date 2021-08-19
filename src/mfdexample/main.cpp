#include <iostream>
#include <ndn-sd/ndn-sd.hpp>
#include "dns_sd.h"
#include <csignal>
#include <map>

using namespace std;

map<int, DNSServiceRef> dnsServiceFdMap;

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
	void* context
)
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
	void* context
)
{
	if (errorCode == kDNSServiceErr_NoError)
	{
		if (flags & kDNSServiceFlagsAdd)
		{
			cout << "ADD " << serviceName << " regtype " << regtype << " domain " << replyDomain << endl;

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

	{
		DNSServiceRef dnsServiceRef;

		auto res = DNSServiceBrowse(&dnsServiceRef, 0, 0, "_ndn._tcp", nullptr, &browseReply, nullptr);

		if (res != kDNSServiceErr_NoError)
		{
			cerr << "failed to initialize browsing for services: " << res << endl;
		}
		else
		{
			dnsServiceFdMap[DNSServiceRefSockFD(dnsServiceRef)] = dnsServiceRef;

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

			DNSServiceRefDeallocate(dnsServiceRef);
		}
	}
	
	return 0;
}