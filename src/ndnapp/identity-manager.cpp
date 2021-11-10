/**
 * Copyright (C) 2019 Regents of the University of California.
 * @author: Peter Gusev <peter@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#include "identity-manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#include <winsock.h>
#else
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

#include <ndn-ind/security/key-chain.hpp>
#include <ndn-ind/security/certificate/certificate.hpp>
#include <ndn-ind/security/safe-bag.hpp>

#include "logging.hpp"
#include "ndnapp.hpp"

using namespace std;
using namespace std::chrono;
using namespace ndn;
using namespace ndnapp::helpers;

static chrono::seconds kInstanceCertLifetime = chrono::hours(2);
static chrono::seconds kAppCertLifetime = chrono::hours(365 * 24);

IdentityManager::Parameters IdentityManager::getDefaultParameters()
{
	return IdentityManager::Parameters{ chrono::hours(24 * 365), chrono::hours(1) };
}

IdentityManager::IdentityManager(const App* app, shared_ptr<spdlog::logger> logger,
	KeyChain* keyChain, Parameters p)
	: app_(app)
	, logger_(logger)
	, defaultKeyChain_(keyChain)
	, instanceKeyChain_(make_shared<KeyChain>("pib-memory:", "tpm-memory:"))
	, parameters_(p)
{
	if (!app_)
		throw "Can't create IdentityManager: app instance should be provided";
}

const Name& IdentityManager::getSigningIdentity() const
{
	return (signingIdentity_ ? signingIdentity_->getName() : Name());
}

const Name& IdentityManager::getAppIdentity() const
{
	return (appIdentity_ ? appIdentity_->getName() : Name());
}

const Name& IdentityManager::getInstanceIdentity() const
{
	return (instanceIdentity_ ? instanceIdentity_->getName() : Name());
}

shared_ptr<CertificateV2> IdentityManager::getAppCertificate() const
{
	return (appIdentity_ ? appIdentity_->getDefaultKey()->getDefaultCertificate() :
		shared_ptr<CertificateV2>());
}

shared_ptr<CertificateV2> IdentityManager::getInstanceCertificate() const
{
	return (instanceIdentity_ ? instanceIdentity_->getDefaultKey()->getDefaultCertificate() : 
		shared_ptr<CertificateV2>());
}

void IdentityManager::setup(const std::string& signingIdentityOrPath, const std::string& password)
{
	shared_ptr<SafeBag> safeBag;
	// check if passed argument is a file path for safebag
	if (filesystem::exists(signingIdentityOrPath))
		safeBag = loadSafeBag(signingIdentityOrPath);

	bool createdNewIdentity = false;

	if (safeBag)
	{
		CertificateV2 signingCert(*safeBag->getCertificate());
		signingIdentity_ = getIdentity(signingCert.getIdentity(), instanceKeyChain_.get());

		if (!signingIdentity_)
		{
			instanceKeyChain_->importSafeBag(*safeBag, (const uint8_t*)password.c_str(),
				password.size());
			signingIdentity_ = instanceKeyChain_->getPib().getIdentity(signingCert.getIdentity());
		}
	}
	else
	{
		// treat argument as identity name and retrieve it from keychain
		signingIdentity_ = getIdentity(signingIdentityOrPath, defaultKeyChain_, true, &createdNewIdentity);
	}

	setupAppIdentity();
	setupInstanceIdentity();
}

void IdentityManager::setupAppIdentity()
{
	if (!signingIdentity_)
		throw runtime_error("signing identity is not setup");

	appIdentity_ = getIdentity(makeAppIdentityName(signingIdentity_->getName()), defaultKeyChain_);

	if (!appIdentity_)
		createNewAppIdentity();
	
	logger_->info("Using app certificate {}",
		appIdentity_->getDefaultKey()->getDefaultCertificate()->getName().toUri());
}

void IdentityManager::createNewAppIdentity()
{
	if (!signingIdentity_)
		throw runtime_error("signing identity is not setup");

	logger_->info("Creating new app identity...");

	auto cert = createSignedIdentity(makeAppIdentityName(signingIdentity_->getName()), 
		signingIdentity_->getDefaultKey(), defaultKeyChain_, parameters_.appIdentityLifetime_);
	appIdentity_ = defaultKeyChain_->getPib().getIdentity(cert->getIdentity());
}

void IdentityManager::setupInstanceIdentity()
{
	if (!appIdentity_)
		throw runtime_error("app identity is not setup");

	instanceIdentity_ = getIdentity(makeInstanceIdentityName(appIdentity_->getName()), instanceKeyChain_.get());

	if (!instanceIdentity_)
		createNewInstanceIdentity();

	logger_->info("Using instance certificate {}",
		instanceIdentity_->getDefaultKey()->getDefaultCertificate()->getName().toUri());
}

void IdentityManager::createNewInstanceIdentity()
{
	if (!appIdentity_)
		throw runtime_error("app identity is not setup");

	logger_->info("Creating new instance identity...");

	auto cert = createSignedIdentity(makeInstanceIdentityName(appIdentity_->getName()),
		appIdentity_->getDefaultKey(), instanceKeyChain_.get(), parameters_.instIdentityLifetime_);
	instanceIdentity_ = instanceKeyChain_->getPib().getIdentity(cert->getIdentity());
}

shared_ptr<CertificateV2> 
IdentityManager::generateSignedIdentity(const Name& identityName,
	const Name& signingIdentityName, KeyChain* storeKeyChain, chrono::seconds lifetime)
{
	auto signingIdentity = defaultKeyChain_->getPib().getIdentity(Name(signingIdentityName));
	auto instanceCertificate = createSignedIdentity(Name(identityName),
		signingIdentity->getDefaultKey(), (storeKeyChain ? storeKeyChain : instanceKeyChain_.get()),
		lifetime);

	return instanceCertificate;
}

shared_ptr<CertificateV2> 
IdentityManager::generateSignedIdentity(const Name& identityName,
	const SafeBag& identity, const string& password, KeyChain* storeKeyChain, chrono::seconds lifetime)
{
	instanceKeyChain_->importSafeBag(identity, (const uint8_t*)password.c_str(),
		password.size());

	CertificateV2 signingCert(*identity.getCertificate());
	auto signingIdentity = instanceKeyChain_->getPib().getIdentity(signingCert.getIdentity());
	auto instanceCertificate = createSignedIdentity(Name(identityName), 
		signingIdentity->getDefaultKey(), (storeKeyChain ? storeKeyChain : instanceKeyChain_.get()), 
		lifetime);

	return instanceCertificate;
}

shared_ptr<SafeBag> IdentityManager::loadSafeBag(const string& filePath)
{
	NLOG_DEBUG("Loading SafeBag file at {}", filePath);
	shared_ptr<SafeBag> safeBag;

	try {
		ifstream bagFile(filesystem::absolute(filePath), ios::binary);
		vector<uint8_t> bagData(istreambuf_iterator<char>(bagFile), {});

		shared_ptr<SafeBag> safeBag = make_shared<SafeBag>(bagData.data(), bagData.size());

		NLOG_DEBUG("Loaded certificate from SafeBag: {]", safeBag->getCertificate()->getName().toUri());
	}
	catch (runtime_error& e)
	{
		NLOG_ERROR("Failed to load safebag file at {}: {}", filePath, e.what());
	}

	return safeBag;
}

shared_ptr<CertificateV2> IdentityManager::createSignedIdentity(const Name& identityName, const shared_ptr<PibKey>& signingKey, 
	KeyChain* storeKeyChain, chrono::seconds validity) 
{
	auto now = chrono::system_clock::now();
	chrono::duration<double> sec = now.time_since_epoch();

	auto pibId = storeKeyChain->createIdentityV2(identityName);
	auto idKey = pibId->getDefaultKey();
	auto certName = Name(idKey->getName())
		.append(signingKey->getDefaultCertificate()->getIssuerId())
		.appendVersion((uint64_t)(sec.count() * 1000));
	shared_ptr<CertificateV2> cert = make_shared<CertificateV2>();

	cert->setName(certName);
	cert->getMetaInfo().setType(ndn_ContentType_KEY);
	cert->setContent(idKey->getPublicKey());

	SigningInfo signingParams(signingKey);
	signingParams.setValidityPeriod(ValidityPeriod(now, now + validity));
	storeKeyChain->sign(*cert, signingParams);
	storeKeyChain->addCertificate(*idKey, *cert);

	return cert;
}

Name IdentityManager::makeAppIdentityName(const ndn::Name& signingIdentity)
{
	static char hostname[256];
	memset(hostname, 0, 256);

	if (gethostname(hostname, 256) != 0)
		sprintf(hostname, "localhost");

	return Name(signingIdentity).append(hostname).append(app_->getAppName());
}

Name IdentityManager::makeInstanceIdentityName(const ndn::Name& appIdentity)
{
	return Name(appIdentity).append(app_->getInstanceId()).appendTimestamp(chrono::system_clock::now());
}

std::shared_ptr<PibIdentity>
IdentityManager::getIdentity(const ndn::Name& idName, KeyChain *keyChain,
	bool createIfNotFound, bool* wasCreated)
{
	std::shared_ptr<PibIdentity> id;
	vector<Name> idNames;
	keyChain->getPib().getAllIdentityNames(idNames);

	auto it = find(idNames.begin(), idNames.end(), idName);

	if (it == idNames.end())
	{
		logger_->warn("identity {} was not found in KeyChain", idName.toUri());

		if (createIfNotFound)
		{
			logger_->info("creating self-signed identity {}...", idName.toUri());

			id = keyChain->createIdentityV2(idName);

			if (wasCreated)
				*wasCreated = true;
		}
	}
	else
		id = keyChain->getPib().getIdentity(idName);

	return id;
}
