/**
 * @file serverOptThread.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief the server main thread
 * @version 0.1
 * @date 2021-09-17
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/serverOptThread.h"

/**
 * @brief Construct a new Server Opt Thread object
 * 
 * @param serverChannel server communication channel
 * @param fp2ChunkDB the chunk info index
 * @param indexType the index type
 */
ServerOptThread::ServerOptThread(SSLConnection* serverChannel, AbsDatabase* fp2ChunkDB, 
    int indexType) { 
    serverChannel_ = serverChannel;
    fp2ChunkDB_ = fp2ChunkDB;
    indexType_ = indexType;

    // init the upload
    dataWriterObj_ = new DataWriter();
    storageCoreObj_ = new StorageCore();
    absIndexObj_ = new PlainIndex(fp2ChunkDB_);
    absIndexObj_->SetStorageCoreObj(storageCoreObj_);
    dataReceiverObj_ = new DataReceiver(absIndexObj_, serverChannel_);
    dataReceiverObj_->SetStorageCoreObj(storageCoreObj_);

    // init download recipe
    recipeSenderObj_ = new RecipeSender(serverChannel_);

    // init download chunk
    recvDecoderObj_ = new RecvDecoder(absIndexObj_, serverChannel_);

    // for log file
    if (!tool::FileExist(logFileName_)) {
        // if the log file not exist, add the header
        logFile_.open(logFileName_, ios_base::out);
        logFile_ <<  "logical data size (B), " << "logical chunk num, "
            << "unique data size (B), " << "unique chunk num, "
            << "compressed data size (B), " << "total time (s)" << endl;
    } else {
        // the log file exists
        logFile_.open(logFileName_, ios_base::app | ios_base::out);
    }

    //tool::Logging(myName_.c_str(), "init the ServerOptThread.\n");
}

/**
 * @brief Destroy the Server Opt Thread object
 * 
 */
ServerOptThread::~ServerOptThread() {
    delete dataWriterObj_;
    delete storageCoreObj_;
    delete absIndexObj_;
    delete dataReceiverObj_;
    delete recvDecoderObj_;
    delete recipeSenderObj_;

    for (auto it : clientLockIndex_) {
        delete it.second;
    }

    logFile_.close();

    // fprintf(stderr, "========ServerOptThread Info========\n");
    // fprintf(stderr, "total recv upload requests: %lu\n", totalUploadReqNum_);
    // fprintf(stderr, "total recv download requests: %lu\n", totalRestoreReqNum_);
    // fprintf(stderr, "====================================\n");
}

/**
 * @brief the main thread 
 * 
 * @param clientSSL the client ssl
 */
void ServerOptThread::Run(SSL* clientSSL) {
    SendMsgBuffer_t recvBuf;
    recvBuf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + CHUNK_HASH_SIZE * 2 + sizeof(FileRecipeHead_t));
    recvBuf.header = (NetworkHead_t*) recvBuf.sendBuffer;
    recvBuf.header->dataSize = 0;
    recvBuf.dataBuffer = recvBuf.sendBuffer + sizeof(NetworkHead_t);
    uint32_t recvSize = 0;

    //tool::Logging(myName_.c_str(), "the main thread is running.\n");

    // start to receive the data type
    if (!serverChannel_->ReceiveData(clientSSL, recvBuf.sendBuffer, 
        recvSize)) {
        tool::Logging(myName_.c_str(), "recv the login message error.\n");
        exit(EXIT_FAILURE);
    }

    // check the client lock here (ensure exist only one client with the same client ID)
    uint32_t clientID = recvBuf.header->clientID;
    boost::mutex* tmpLock;
    {
        // ensure only one client ID can enter the process
        lock_guard<mutex> lock(clientLockSetLock_);
        auto clientLockRes = clientLockIndex_.find(clientID);
        if (clientLockRes != clientLockIndex_.end()) {
            // try to lock this mutex
            tmpLock = clientLockRes->second;
            tmpLock->lock();
        } else {
            // add a new lock to the current index
            tmpLock = new boost::mutex();
            clientLockIndex_[clientID] = tmpLock;
            tmpLock->lock();
        }
    }

    // ------------------------
    // 判断请求类型，如果是upload和download recipe则初始化文件名
    // ------------------------
    int optType = 0;
    //tool::Logging(myName_.c_str(), "message type is%d\n", recvBuf.header->messageType);
    switch (recvBuf.header->messageType) {
        case EDGE_MIGRATE_LOGIN: {
            this->EdgeUploadThread(recvBuf, clientSSL);
            break;
        }
        case EDGE_DOWNLOAD_RECIPE_LOGIN: {
            this->EdgeDownloadRecipeThread(recvBuf, clientSSL);
            break;
        }
        case EDGE_DOWNLOAD_CHUNK_LOGIN: {
            this->EdgeDownloadChunkThread(recvBuf, clientSSL);
            break;
        }
        default: {
            tool::Logging(myName_.c_str(), "wrong client login type.\n");
            exit(EXIT_FAILURE);
        }
    }

    // clean up client variables
    free(recvBuf.sendBuffer);
    tmpLock->unlock();
    //tool::Logging(myName_.c_str(), "server thread end\n");
    return ;
}

/**
 * @brief check the file status
 * 
 * @param fullRecipePath the full recipe path
 * @param optType the operation type
 * @return true success
 * @return false fail
 */
bool ServerOptThread::CheckFileStatus(string& fullRecipePath, int optType) {
    if (tool::FileExist(fullRecipePath)) {
        // the file exists
        switch (optType) {
            case UPLOAD_OPT: {
                tool::Logging(myName_.c_str(), "%s exists, overwrite it.\n",
                    fullRecipePath.c_str());
                break;
            }
            case DOWNLOAD_RECIPE_OPT: {
                tool::Logging(myName_.c_str(), "%s exists, access it.\n",
                    fullRecipePath.c_str());
                break;
            }
        }
    } else {
        switch (optType) {
            case UPLOAD_OPT: {
                tool::Logging(myName_.c_str(), "%s not exists, create it.\n",
                    fullRecipePath.c_str());
                break;
            }
            case DOWNLOAD_RECIPE_OPT: {
                tool::Logging(myName_.c_str(), "%s not exists, restore reject.\n",
                    fullRecipePath.c_str());
                return false;
            }
        }
    }
    return true;
}

void ServerOptThread::EdgeUploadThread(SendMsgBuffer_t &recvBuf, SSL* clientSSL) {
    uint32_t clientID = recvBuf.header->clientID;
    vector<boost::thread*> thList;
    boost::thread* thTmp;
    boost::thread_attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    EnclaveInfo_t enclaveInfo;
    ClientVar* curClient;

    string fileName;
    fileName.assign((char*) recvBuf.dataBuffer, CHUNK_HASH_SIZE * 2); 
    string recipePath = config.GetRecipeRootPath() +
        fileName + config.GetRecipeSuffix();
    string secureRecipePath = config.GetRecipeRootPath() +
        fileName + config.GetSecureRecipeSuffix();
    string keyRecipePath = config.GetRecipeRootPath() +
        fileName + config.GetKeyRecipeSuffix();
    
    FileRecipeHead_t* tmpRecipeHead = (FileRecipeHead_t*)(recvBuf.dataBuffer + CHUNK_HASH_SIZE * 2);
    
    curClient = new ClientVar(clientID, clientSSL, UPLOAD_OPT, recipePath, 
        secureRecipePath, keyRecipePath, tmpRecipeHead->fileSize, 
        tmpRecipeHead->totalChunkNum);
    
    thTmp = new boost::thread(attrs, boost::bind(&DataReceiver::Run, dataReceiverObj_,
        curClient, &enclaveInfo));
    thList.push_back(thTmp); 
    thTmp = new boost::thread(attrs, boost::bind(&DataWriter::Run, dataWriterObj_,
        curClient->_inputMQ));
    thList.push_back(thTmp);

    // send the upload-response to the client
    recvBuf.header->messageType = EDGE_LOGIN_RESPONSE;
    if (!serverChannel_->SendData(clientSSL, recvBuf.sendBuffer, 
        sizeof(NetworkHead_t))) {
        tool::Logging(myName_.c_str(), "send the upload-login response error.\n");
        exit(EXIT_FAILURE);
    }

    for (auto it : thList) {
        it->join();
    }

    // clean up
    for (auto it : thList) {
        delete it;
    }  
    thList.clear();
    delete curClient;

    return ;
}

void ServerOptThread::EdgeDownloadRecipeThread(SendMsgBuffer_t &recvBuf, SSL* clientSSL) {
    uint32_t clientID = recvBuf.header->clientID;
    vector<boost::thread*> thList;
    boost::thread* thTmp;
    boost::thread_attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    EnclaveInfo_t enclaveInfo;
    ClientVar* curClient;
    
    string fileName;
    fileName.assign((char*) recvBuf.dataBuffer, CHUNK_HASH_SIZE * 2); 
    string recipePath = config.GetRecipeRootPath() +
        fileName + config.GetRecipeSuffix();
    string secureRecipePath = config.GetRecipeRootPath() +
        fileName + config.GetSecureRecipeSuffix();
    string keyRecipePath = config.GetRecipeRootPath() +
        fileName + config.GetKeyRecipeSuffix();

    if (tool::FileExist(recipePath) && tool::FileExist(secureRecipePath) 
        && tool::FileExist(keyRecipePath)) {
        tool::Logging(myName_.c_str(), "recv the download recipe request from client: %u\n",
            clientID);
        curClient = new ClientVar(clientID, clientSSL, DOWNLOAD_RECIPE_OPT, recipePath,
            secureRecipePath, keyRecipePath, 0, 0);

        thTmp = new boost::thread(attrs, boost::bind(&RecipeSender::Run, recipeSenderObj_,
            curClient));
        thList.push_back(thTmp);

        // send the restore-reponse to the client (include the file recipe header)
        recvBuf.header->messageType = EDGE_LOGIN_RESPONSE;
        curClient->_recipeReadHandler.read((char*)recvBuf.dataBuffer,
            sizeof(FileRecipeHead_t));
        tool::Logging(myName_.c_str(), "send login response\n");
        if (!serverChannel_->SendData(clientSSL, recvBuf.sendBuffer, 
            sizeof(NetworkHead_t) + sizeof(FileRecipeHead_t))) {
            tool::Logging(myName_.c_str(), "send the restore-login response error.\n");
            exit(EXIT_FAILURE);
        }
    }
    else {
        recvBuf.header->messageType = CLOUD_FILE_NON_EXIST;
        if (!serverChannel_->SendData(clientSSL, recvBuf.sendBuffer, 
            sizeof(NetworkHead_t) + sizeof(FileRecipeHead_t))) {
            tool::Logging(myName_.c_str(), "send the restore-login response error.\n");
            exit(EXIT_FAILURE);
        }
    }

    for (auto it : thList) {
        it->join();
    }

    // clean up
    for (auto it : thList) {
        delete it;
    }
    thList.clear();
    delete curClient;
    tool::Logging(myName_.c_str(), "download recipes done\n");
}

void ServerOptThread::EdgeDownloadChunkThread(SendMsgBuffer_t &recvBuf, SSL* clientSSL) {
    uint32_t clientID = recvBuf.header->clientID;
    vector<boost::thread*> thList;
    boost::thread* thTmp;
    boost::thread_attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    EnclaveInfo_t enclaveInfo;
    ClientVar* curClient;

    tool::Logging(myName_.c_str(), "recv the download chunk request from client: %u\n",
        clientID);
    string virtualStr = "";
    curClient = new ClientVar(clientID, clientSSL, DOWNLOAD_CHUNK_OPT, virtualStr,
        virtualStr, virtualStr, 0, 0);
    
    thTmp = new boost::thread(attrs, boost::bind(&RecvDecoder::Run, recvDecoderObj_,
        curClient));
    thList.push_back(thTmp);

    // send the restore-reponse to the client (include the file recipe header)
    recvBuf.header->messageType = EDGE_LOGIN_RESPONSE;
    if (!serverChannel_->SendData(clientSSL, recvBuf.sendBuffer, 
        sizeof(NetworkHead_t))) {
        tool::Logging(myName_.c_str(), "send the restore-login response error.\n");
        exit(EXIT_FAILURE);
    }

    for (auto it : thList) {
        it->join();
    }

    // clean up
    for (auto it : thList) {
        delete it;
    }
    thList.clear();
    delete curClient;
    //tool::Logging(myName_.c_str(), "111\n");
    return ;
}