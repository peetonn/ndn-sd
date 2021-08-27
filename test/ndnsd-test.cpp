#include <catch2/catch_test_macros.hpp>
#include <ndn-sd/ndn-sd.hpp>

#include "dns_sd.h"

using namespace std;
using namespace ndnsd;

void dnsRegisterReplyHelper(
    DNSServiceRef                       sdRef,
    DNSServiceFlags                     flags,
    DNSServiceErrorType                 errorCode,
    const char* name,
    const char* regtype,
    const char* domain,
    void* context);
DNSServiceRef* dnsRegisterHelper(string protocol, string subtype, string uuid,
    int port, string prefix, bool addCert = false);
void dnsServiceCleanupHelper(DNSServiceRef* ref);

TEST_CASE( "NDN-SD construction", "[ndnsd]" ) {
    SECTION("declared as variable")
    {
        NdnSd sd("test-uuid1");
        REQUIRE("test-uuid1" == sd.getUuid());
        REQUIRE(sd.getCertificate().size() == 0);
        REQUIRE(sd.getPrefix().size() == 0);
        REQUIRE(sd.getPort() == 0);
        REQUIRE(sd.getDomain().size() == 0);
    }
}

TEST_CASE("NDN-SD service browse", "[ndnsd]") {

    auto ndnSdErrorCb = [](int reqId, int errCode, string msg, bool, void*) {
        FAIL("error occurred: " << errCode << " " << msg);
    };

    GIVEN("advertised NDN service over UDP") {

        auto srvRef = dnsRegisterHelper("_udp", "", "test-udp-uuid", 45312,
            "/test/prefix");

        WHEN("NdnSd instance browse for NDN/UDP services") {
            
            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::UDP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*) 
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-udp-uuid");
                REQUIRE(discovered->getProtocol() == Proto::UDP);
                REQUIRE(discovered->getDomain().size() > 0);

                REQUIRE(discovered->getSubtype().size() == 0);
                REQUIRE(discovered->getCertificate().size() == 0);
                REQUIRE(discovered->getPrefix().size() == 0);
                REQUIRE(discovered->getPort() == 0);

            }, ndnSdErrorCb);

            THEN("service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered == 1);
                REQUIRE(discoveredSd);
            }
        }
        WHEN("NdnSd instance browse for NDN/TCP services") {

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                FAIL("service should not be discovered");
            }, ndnSdErrorCb);

            THEN("service is NOT discovered") {
                sd.run(1000);
            }
        }

        dnsServiceCleanupHelper(srvRef);
    }

    GIVEN("advertised NDN service over TCP") {

        auto srvRef = dnsRegisterHelper("_tcp", "", "test-tcp-uuid", 45312,
            "/test/prefix");

        WHEN("NdnSd instance browse for NDN/TCP services") {

            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-tcp-uuid");
                REQUIRE(discovered->getProtocol() == Proto::TCP);
                REQUIRE(discovered->getDomain().size() > 0);

                REQUIRE(discovered->getSubtype().size() == 0);
                REQUIRE(discovered->getCertificate().size() == 0);
                REQUIRE(discovered->getPrefix().size() == 0);
                REQUIRE(discovered->getPort() == 0);

            }, ndnSdErrorCb);

            THEN("service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered == 1);
                REQUIRE(discoveredSd);
            }
        }
        WHEN("NdnSd instance browse for NDN/UDP services") {

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::UDP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                FAIL("service should not be discovered");
            }, ndnSdErrorCb);

            THEN("service is NOT discovered") {
                sd.run(1000);
            }
        }

        dnsServiceCleanupHelper(srvRef);
    }

    GIVEN("advertised NDN service with subtype \""+ ndnsd::kNdnDnsServiceSubtypeMFD +"\"") {

        auto srvRef = dnsRegisterHelper("_tcp", ndnsd::kNdnDnsServiceSubtypeMFD, "test-tcp-uuid", 45312,
            "/test/prefix");

        WHEN("NdnSd instance browse for NDN services with subtype \""+ ndnsd::kNdnDnsServiceSubtypeMFD +"\"") {

            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly, ndnsd::kNdnDnsServiceSubtypeMFD },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-tcp-uuid");
                REQUIRE(discovered->getProtocol() == Proto::TCP);
                REQUIRE(discovered->getDomain().size() > 0);
                REQUIRE(discovered->getSubtype() == ndnsd::kNdnDnsServiceSubtypeMFD);

            }, ndnSdErrorCb);

            THEN("service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered > 0);
                REQUIRE(discoveredSd);
            }
        }
        WHEN("NdnSd instance browse for NDN services with subtype \""+ ndnsd::kNdnDnsServiceSubtypeNFD +"\"") {

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly, ndnsd::kNdnDnsServiceSubtypeNFD },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                FAIL("service should not be discovered");
            }, ndnSdErrorCb);

            THEN("service is NOT discovered") {
                sd.run(1000);
            }
        }
        WHEN("NdnSd instance browse for NDN services without any subtype") {

            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-tcp-uuid");
                REQUIRE(discovered->getProtocol() == Proto::TCP);
                REQUIRE(discovered->getDomain().size() > 0);
                // subtype will be not known to us even if the advertised service had it
                REQUIRE(discovered->getSubtype().size() == 0);

            }, ndnSdErrorCb);

            THEN("service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered == 1);
                REQUIRE(discoveredSd);
            }
        }

        dnsServiceCleanupHelper(srvRef);
    }

    GIVEN("advertised two NDN services: one without a subtype "
          "and another with subtype \"" + ndnsd::kNdnDnsServiceSubtypeMFD + "\"") {

        auto srvRef1 = dnsRegisterHelper("_tcp", ndnsd::kNdnDnsServiceSubtypeMFD, 
            "test-mfd-uuid", 45312, "/test/prefix/mfd");
        auto srvRef2 = dnsRegisterHelper("_tcp", "", 
            "test-nfd-uuid", 45312, "/multicast");

        WHEN("NdnSd instance browse for NDN services with subtype \"" + ndnsd::kNdnDnsServiceSubtypeMFD + "\"") {

            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly, ndnsd::kNdnDnsServiceSubtypeMFD },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-mfd-uuid");
                REQUIRE(discovered->getSubtype() == ndnsd::kNdnDnsServiceSubtypeMFD);

            }, ndnSdErrorCb);

            THEN("mfd service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered == 1);
                REQUIRE(discoveredSd);
            }
        }
        WHEN("NdnSd instance browse for NDN services without any subtype") {

            bool mfdDiscovered = false, nfdDiscovered = false;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::TCP, kDNSServiceInterfaceIndexLocalOnly },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                nDiscovered += 1;

                bool allowedUuid = (discovered->getUuid() == "test-mfd-uuid" ||
                        discovered->getUuid() == "test-nfd-uuid");
                REQUIRE(allowedUuid);

                if (discovered->getUuid() == "test-mfd-uuid")
                    mfdDiscovered = true;
                if (discovered->getUuid() == "test-nfd-uuid")
                    nfdDiscovered = true;

            }, ndnSdErrorCb);

            THEN("both services are discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered == 2);
                REQUIRE(mfdDiscovered);
                REQUIRE(nfdDiscovered);
            }
        }
        WHEN("NdnSd instance browse for services with custom subtype") {

            NdnSd sd("test-uuid1");
            sd.browse({ ndnsd::Proto::UDP, kDNSServiceInterfaceIndexLocalOnly, "foofd" },
                [&](int, shared_ptr<const NdnSd> discovered, void*)
            {
                FAIL("service should not be discovered");
            }, ndnSdErrorCb);

            THEN("no services are discovered") {
                sd.run(1000);
                SUCCEED("no services");
            }
        }

        dnsServiceCleanupHelper(srvRef1);
        dnsServiceCleanupHelper(srvRef2);
    }
}

DNSServiceRef* dnsRegisterHelper(string protocol, string subtype, string uuid,
    int port, string prefix, bool addCert)
{
    uint16_t txtBufLen = 512;
    uint8_t* txtBuf = (uint8_t*)malloc(txtBufLen);
    TXTRecordRef txtRecRef;
    TXTRecordCreate(&txtRecRef, txtBufLen, txtBuf);
    TXTRecordSetValue(&txtRecRef, "p", prefix.size(), (void*)prefix.data());

    if (addCert)
    {
        string certBase64 = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyBXGZLp9OFSFuxz"
                            "DZoEcCB1jL9X30Qf8wftT710AZjpUJuAFq0uWrC40qsl09VTantqrbPpMLV"
                            "U6kz4rT+uyV/YeTXq6o/kI8FqOJu1pzSXd2jzLFl1dExkElYyxbsXFs/PZT"
                            "tTe7Im2m3IQ1cxAvL+WitO8IwsHqx4uvaBzHNMoYh0JJAq1dty2mPbQ5Rf0"
                            "jFVXIoWjRUHFs5A6in+aiTDOq3jyJyfDkHrDXzbSnhdPdoxPLF9umV7cHhi"
                            "yWq1hwoK7OyySsQ0I4dFkLUeobLklNA6q0uqWSQQODoB765PbhT5nMjpFVa"
                            "4SWTgsMVJFui5gOuYozPRW0hk5z0cSYwIDAQAB";
        TXTRecordSetValue(&txtRecRef, "c", certBase64.size(), (void*)certBase64.data());
    }

    DNSServiceRef* dnsServiceRef = new DNSServiceRef;
    string regtype = "_ndn." + protocol;
    if (subtype.size()) regtype += "," + subtype;

    auto err = DNSServiceRegister(dnsServiceRef, 0, kDNSServiceInterfaceIndexLocalOnly, uuid.c_str(), regtype.c_str(),
        nullptr, nullptr, htons(port),
        TXTRecordGetLength(&txtRecRef), TXTRecordGetBytesPtr(&txtRecRef),
        &dnsRegisterReplyHelper, nullptr);

    CHECKED_IF(err == kDNSServiceErr_NoError)
    {
        DNSServiceProcessResult(*dnsServiceRef);
        return dnsServiceRef;
    }
    CHECKED_ELSE(err == kDNSServiceErr_NoError)
    {
        dnsServiceCleanupHelper(dnsServiceRef);
        FAIL("failed to register test service");
    }
    return nullptr;
}

void dnsServiceCleanupHelper(DNSServiceRef* ref)
{
    if (ref)
    {
        DNSServiceRefDeallocate(*ref);
        delete ref;
    }
}

void dnsRegisterReplyHelper(
    DNSServiceRef                       sdRef,
    DNSServiceFlags                     flags,
    DNSServiceErrorType                 errorCode,
    const char* name,
    const char* regtype,
    const char* domain,
    void* context)
{
    if (errorCode == kDNSServiceErr_NoError)
    {
        INFO("TEST SRV REGISTERED: " << name << " " << regtype << " " << domain);
    }
    else
        FAIL("TEST SRV REGISTER FAILURE: " << errorCode);
}