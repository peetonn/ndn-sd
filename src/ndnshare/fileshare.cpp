// TODO: add copyright

#include "fileshare.hpp"

#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

#include "logging.hpp"
#include "mime.hpp"

using namespace std;
using namespace ndn;
using namespace cnl_cpp;

FileshareClient::FileshareClient(std::string rootPath, std::string prefix,
    ndn::Face* face, ndn::KeyChain* keyChain,
    std::shared_ptr<spdlog::logger> logger)
    : rootPath_(rootPath)
    , prefix_(prefix, keyChain)
    , prefixRegisterFailure_(false)
    , logger_(logger)
{
    prefix_.setFace(face, [this](auto prefix) {
        logger_->error("failed to register prefix {}", prefix->toUri());
        prefixRegisterFailure_ = true;
    });
    prefix_.addOnObjectNeeded(bind(&FileshareClient::onObjectNeeded, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

bool FileshareClient::onObjectNeeded(Namespace& nmspc, Namespace& neededNamespace, uint64_t callbackId)
{
    assert(nmspc.getName().compare(neededNamespace.getName()) == -1);
    Name fileSuffix = neededNamespace.getName().getSubName(nmspc.getName().size());
    string fileName = fileSuffix[0].toEscapedString();

    //if (fileSuffix[-1] == GeneralizedObjectHandler::getNAME_COMPONENT_META())
    //    fileSuffix = fileSuffix.getPrefix(-1);

    logger_->trace("nmspc: {} needed {}", nmspc.getName().toUri(), neededNamespace.getName().toUri());
    logger_->trace("request for {}", fileName);

    filesystem::path filePath(rootPath_, filesystem::path::format::native_format);
    filePath = filePath / fileName; // filesystem::path(fileName, filesystem::path::format::generic_format);

    if (filesystem::exists(filePath))
    {
        Namespace& fileNamespace = nmspc[Name::Component(fileName)];
        MetaInfo fileMeta;
        fileMeta.setFreshnessPeriod(chrono::milliseconds(1000)); // TODO: what freshness to use
        //fileNamespace.setNewDataMetaInfo(fileMeta);

        // TODO: this should be refactored for huge files (can't read all data into memory)
        ifstream file(filePath, ios::binary | ios::ate);
        logger_->trace("read {}", filePath.string());

        streamsize size = file.tellg();
        file.seekg(0, ios::beg);
        ptr_lib::shared_ptr<vector<uint8_t>> fileContents = ptr_lib::make_shared<vector<uint8_t>>(size);

        logger_->trace("read {} bytes from disk", fileContents->size());

        if (file.read((char*)fileContents->data(), size))
        {
            Blob fileBlob(fileContents, false);
            GeneralizedObjectHandler handler;
            handler.setObject(fileNamespace, fileBlob, mime::content_type(filePath.extension().string()));

            logger_->info("published {}", fileNamespace.getName().toUri());

            return true;
        }
        else
        {
            logger_->error("failed to read file {}", filePath.string());
        }
    }
    else
    {
        logger_->warn("file {} does not exist", filePath.string());
    }

    return false;
}

void FileshareClient::fetch(const std::string& name)
{
    typedef struct _FetchProgress {
        int totalSegments_ = 0;
        int receviedCount_ = 0;
        int lastReceviedNo_ = 0;
    } FetchProgress;

    auto fetchProgress = make_shared<FetchProgress>();
    shared_ptr<Namespace> fileObject = make_shared<Namespace>(name);
    fileObject->setFace(prefix_.getFace_());

    auto onObject = [&, fileObject, fetchProgress, name](const ptr_lib::shared_ptr<ContentMetaInfoObject>& contentMetaInfo,
        Namespace& objectNamespace)
    {
        logger_->info("fetched {}: {} bytes, {} segments, content-type {}", name,
            objectNamespace.getBlobObject().size(), fetchProgress->totalSegments_+1, contentMetaInfo->getContentType());

        writeData(objectNamespace.getName()[-1].toEscapedString(), objectNamespace.getBlobObject());
    };

    auto logger = logger_;
    fileObject->addOnStateChanged([logger, fetchProgress](Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
        uint64_t callbackId) 
    {
        if (changedNamespace.getName()[-1].isSegment())
        {
            fetchProgress->lastReceviedNo_ = changedNamespace.getName()[-1].toSegment();

            if (state == NamespaceState_DATA_RECEIVED)
            {
                fetchProgress->receviedCount_ += 1;

                if (changedNamespace.getName()[-1].toSegment() == 0)
                {
                    fetchProgress->totalSegments_ = changedNamespace.getData()->getMetaInfo().getFinalBlockID().toSegment() + 1;
                }

                if (fetchProgress->totalSegments_)
                {
                    printf("\r>>> fetching %d\\%d", fetchProgress->receviedCount_, fetchProgress->totalSegments_);
                }
            }
        }        
    });

    logger_->info("fetching {}...", name);
    GeneralizedObjectHandler(fileObject.get(), onObject).objectNeeded(true);
};

vector<string> FileshareClient::getFilesList() const
{
    vector<string> files;

    for (auto const& entry : filesystem::directory_iterator(getRootPath()))
    {
        if (entry.is_regular_file())
            files.push_back(entry.path().filename().string());
    }

    return files;
}

void FileshareClient::writeData(const string& fileName, const ndn::Blob& data)
{
    filesystem::path filePath(rootPath_, filesystem::path::format::native_format);
    filePath = filePath / fileName;

    ofstream wf(filePath.string(), ios::out | ios::binary);
    if (!wf) {
        logger_->error("error writing to {}", filePath.string());
        return;
    }

    wf.write((const char*)data.buf(), data.size());
    wf.close();

    if (!wf.good())
        logger_->error("error writing to {}: ", filePath.string());
    else
        logger_->info("stored at {}", filePath.string());
}
