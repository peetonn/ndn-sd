#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <ndn-ind/face.hpp>
#include <ndn-ind/security/key-chain.hpp>
#include <ndn-ind/security/safe-bag.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder.hpp>
#include <ndn-ind-tools/micro-forwarder/micro-forwarder-transport.hpp>

#include "key-chain-manager.hpp"

using namespace std;
using namespace ndn;
using namespace ndnapp;
using namespace ndnapp::helpers;

SafeBag loadSafeBag(string fileName);

TEST_CASE( "KeyChainManager generate instance identities", "[inst-id]" )
{
	GIVEN("initialized keychain manager") 
	{
		Face f(ptr_lib::make_shared<ndntools::MicroForwarderTransport>(),
			ptr_lib::make_shared<ndntools::MicroForwarderTransport::ConnectionInfo>(ndntools::MicroForwarder::get()));
		KeyChain kc("pib-memory:", "tpm-memory:"); // use mem keychain to avoid polluting system keychain
		KeyChainManager kcm(&f, &kc);

		WHEN("try to generate signed identity with SafeBag")
		{
			string encryptedSafeBag = "data/encrypted.safebag";
			string safeBagPassword = "0000";
			SafeBag sb = loadSafeBag(encryptedSafeBag);
			shared_ptr<CertificateV2> instanceCert;
			string idName = "/test-identity";

			REQUIRE_NOTHROW(
				instanceCert = kcm.generateInstanceIdentity(idName, sb, safeBagPassword)
			);

			THEN("generate certificate is valid")
			{
				REQUIRE(instanceCert.get() != nullptr);
				REQUIRE(instanceCert->isValid());
			}
		}

		WHEN("try to generate signed identity using existing identity in keychain")
		{
			kc.createIdentityV2("/test-signing-id");

			shared_ptr<CertificateV2> instanceCert;
			string idName = "/test-identity";

			REQUIRE_NOTHROW(
				instanceCert = kcm.generateInstanceIdentity(idName, "/test-signing-id")
			);

			THEN("identity and certificate are created")
			{
				REQUIRE(instanceCert.get() != nullptr);
				REQUIRE(instanceCert->isValid());
			}
		}
	}
}

SafeBag loadSafeBag(string fileName)
{
	string curDir = filesystem::current_path().string();
	string filePath = filesystem::absolute(fileName).string();

	REQUIRE(filesystem::exists(filePath));

	ifstream bagFile(filePath, ios::binary);
	vector<uint8_t> bagData(istreambuf_iterator<char>(bagFile), {});
	return SafeBag(bagData.data(), bagData.size());
}
