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

#ifndef __identity_manager_hpp__
#define __identity_manager_hpp__

#include <chrono>
#include <memory>
#include <string>

#include <ndn-ind/name.hpp>

namespace spdlog {
    class logger;
}

namespace ndn {
    class CertificateV2;
    class Face;
    class KeyChain;
    class PibIdentity;
    class PibKey;
    class MemoryContentCache;
    class SafeBag;    
}

namespace ndnapp
{
    class App;

namespace helpers
{
    class IdentityManager {
    public:
        typedef struct _Parameters {
            std::chrono::seconds appIdentityLifetime_;
            std::chrono::seconds instIdentityLifetime_;
        } Parameters;

        static Parameters getDefaultParameters();

        IdentityManager(const App* app, std::shared_ptr<spdlog::logger> logger, 
            ndn::KeyChain* keyChain, Parameters p = getDefaultParameters());
        ~IdentityManager() {}

        std::shared_ptr<ndn::CertificateV2> generateSignedIdentity(const ndn::Name& identityName,
            const ndn::Name& signingIdentityName, ndn::KeyChain* storeKeyChain = nullptr,
            std::chrono::seconds lifetime = std::chrono::hours(1));

        std::shared_ptr<ndn::CertificateV2> generateSignedIdentity(const ndn::Name& identityName,
            const ndn::SafeBag& identity, const std::string& password = "",
            ndn::KeyChain* storeKeyChain = nullptr,
            std::chrono::seconds lifetime = std::chrono::hours(1));
        
        const std::shared_ptr<const ndn::KeyChain> getInstanceKeyChain() const
        {
            return instanceKeyChain_;
        }

        const ndn::Name& getSigningIdentity() const;
        const ndn::Name& getAppIdentity() const;
        const ndn::Name& getInstanceIdentity() const;
        std::shared_ptr<ndn::CertificateV2> getAppCertificate() const;
        std::shared_ptr<ndn::CertificateV2> getInstanceCertificate() const;

        void setup(const std::string& signingIdentityOrPath, const std::string& password = "");

        void createNewAppIdentity();
        void createNewInstanceIdentity();
        
        static std::shared_ptr<ndn::SafeBag> loadSafeBag(const std::string& filePath);

    private:
        std::shared_ptr<spdlog::logger> logger_;
        const App* app_;

        Parameters parameters_;
        ndn::KeyChain* defaultKeyChain_;
        std::shared_ptr<ndn::KeyChain> instanceKeyChain_;
        std::shared_ptr<ndn::PibIdentity> signingIdentity_, appIdentity_, instanceIdentity_;

        void setupAppIdentity();
        void setupInstanceIdentity();
        
        std::shared_ptr<ndn::CertificateV2>  createSignedIdentity(const ndn::Name& identityName, 
            const std::shared_ptr<ndn::PibKey>& signingKey, ndn::KeyChain* storeKeyChain, 
            std::chrono::seconds validity);
        
        ndn::Name makeAppIdentityName(const ndn::Name& signingIdentity);
        ndn::Name makeInstanceIdentityName(const ndn::Name& appIdentity);

        std::shared_ptr<ndn::PibIdentity> getIdentity(const ndn::Name& idName, 
            ndn::KeyChain *keyChain, bool createIfNotFound = false, bool* wasCreated = nullptr);
    };
}
}

#endif
