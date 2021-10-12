// TODO: add copyright

#ifndef __fileshare_hpp__
#define __fileshare_hpp__

#include <memory>
#include <stdexcept>
#include <vector>

#include <cnl-cpp/namespace.hpp>

namespace spdlog {
    class logger;
}

namespace ndn {
    class Face;
    class KeyChain;
}

    class FileshareClient {
    public:
        FileshareClient(std::string rootPath, std::string prefix,
            ndn::Face* face, ndn::KeyChain* keyChain,
            std::shared_ptr<spdlog::logger> logger);
        ~FileshareClient() {}

        void processEvents() {
            if (prefixRegisterFailure_)
                throw std::runtime_error("failed to register prefix " + prefix_.getName().toUri());
        }

        void fetch(const std::string& prefx);

        std::string getRootPath() const { return rootPath_; }
        std::vector<std::string> getFilesList() const;

    private:
        bool prefixRegisterFailure_;
        std::string rootPath_;
        std::shared_ptr<spdlog::logger> logger_;
        cnl_cpp::Namespace prefix_;

        bool onObjectNeeded(cnl_cpp::Namespace&, cnl_cpp::Namespace& neededNamespace, uint64_t);
        void writeData(const std::string& fileName, const ndn::Blob& data);
    };


#endif