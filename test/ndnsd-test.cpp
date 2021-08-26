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

    auto ndnSdErrorCb = [](int errCode, string msg, bool, void*) {
        FAIL("error occurred: " << errCode << " " << msg);
    };

    GIVEN("advertised NDN service over UDP") {

        auto srvRef = dnsRegisterHelper("_udp", "", "test-udp-srv-uuid", 45312,
            "/test/prefix");

        WHEN("NdnSd instance browse for NDN/UDP services") {
            
            shared_ptr<const NdnSd> discoveredSd;
            int nDiscovered = 0;

            NdnSd sd("test-uuid1");
            sd.browse(ndnsd::Proto::UDP,
                [&](shared_ptr<const NdnSd> discovered, void*) 
            {
                nDiscovered += 1;
                discoveredSd = discovered;

                REQUIRE(discovered->getUuid() == "test-udp-srv-uuid");
                REQUIRE(discovered->getProtocol() == Proto::UDP);

                REQUIRE(discovered->getCertificate().size() == 0);
                REQUIRE(discovered->getPrefix().size() == 0);
                REQUIRE(discovered->getPort() == 0);
                REQUIRE(discovered->getDomain().size() > 0);

                /*sd.resolve(discoveredSd,
                    [](shared_ptr<const NdnSd> sd, void*)
                {

                }, ndnSdErrorCb);*/
            }, ndnSdErrorCb, kDNSServiceInterfaceIndexLocalOnly);

            THEN("service is discovered") {
                sd.run(1000);
                REQUIRE(nDiscovered > 0);
                REQUIRE(discoveredSd);
            }
        }
        WHEN("NdnSd instance browse for NDN/TCP services") {

            NdnSd sd("test-uuid1");
            sd.browse(ndnsd::Proto::TCP,
                [&](shared_ptr<const NdnSd> discovered, void*)
            {
                FAIL("service should not be discovered");
            }, ndnSdErrorCb, kDNSServiceInterfaceIndexLocalOnly);

            THEN("service is NOT discovered") {
                sd.run(1000);
            }
        }

        dnsServiceCleanupHelper(srvRef);
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