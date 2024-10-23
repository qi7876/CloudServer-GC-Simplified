/**
 * @file clientVar.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of client var
 * @version 0.1
 * @date 2021-04-24
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/clientVar.h"

/**
 * @brief Construct a new Client Var object
 * 
 * @param clientID the client ID
 * @param clientSSL the client SSL
 * @param optType the operation type (upload / download)
 * @param recipePath the file recipe path
 */
ClientVar::ClientVar(uint32_t clientID, SSL* clientSSL, 
    int optType, string& recipePath, string& secureRecipePath, 
    string& keyRecipePath, uint64_t fileSize, uint64_t totalChunkNum) {
    // basic info
    _clientID = clientID;
    _clientSSL = clientSSL;
    optType_ = optType;
    recipePath_ = recipePath;
    secureRecipePath_ = secureRecipePath;
    keyRecipePath_ = keyRecipePath;
    myName_ = myName_ + "-" + to_string(_clientID);
    _fileSize = fileSize;
    _totalChunkNum = totalChunkNum;

    // config
    sendChunkBatchSize_ = config.GetSendChunkBatchSize();
    sendRecipeBatchSize_ = config.GetSendRecipeBatchSize();

    switch (optType_) {
        case UPLOAD_OPT: {
            this->InitUploadBuffer();
            break;
        }
        case DOWNLOAD_RECIPE_OPT: {
            this->InitDownloadRecipeBuffer();
            break;
        }
        case DOWNLOAD_CHUNK_OPT: {
            this->InitDownloadChunkBuffer();
            break;
        }
        default: {
            tool::Logging(myName_.c_str(), "wrong client opt type.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * @brief Destroy the Client Var object
 * 
 */
ClientVar::~ClientVar() {
    switch (optType_) {
        case UPLOAD_OPT: {
            this->DestoryUploadBuffer();
            break;
        }
        case DOWNLOAD_RECIPE_OPT: {
            this->DestoryDownloadRecipeBuffer();
            break;
        }
        case DOWNLOAD_CHUNK_OPT: {
            this->DestoryDownloadChunkBuffer();
            break;
        }
    }
}

/**
 * @brief init the upload buffer
 * 
 */
void ClientVar::InitUploadBuffer() {
    // assign a random id to the container
    tool::CreateUUID(_curContainer.containerID, CONTAINER_ID_LENGTH);
    _curContainer.currentBodySize = 0;
    _curContainer.currentHeaderSize = 0;

    // init the recv buffer
    _recvChunkBuf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sendChunkBatchSize_ * (sizeof(uint32_t) + MAX_CHUNK_SIZE));
    _recvChunkBuf.header = (NetworkHead_t*) _recvChunkBuf.sendBuffer;
    _recvChunkBuf.header->clientID = _clientID;
    _recvChunkBuf.header->currentItemNum = 0;
    _recvChunkBuf.header->dataSize = 0;
    _recvChunkBuf.dataBuffer = _recvChunkBuf.sendBuffer + sizeof(NetworkHead_t);

    // prepare the input MQ
    _inputMQ = new MessageQueue<Container_t>(CONTAINER_QUEUE_SIZE);

    // prepare the crypto
    _mdCtx = EVP_MD_CTX_new();
    _cipherCtx = EVP_CIPHER_CTX_new();

    // init the file recipe
    _recipeWriteHandler.open(recipePath_, ios_base::trunc | ios_base::binary);
    if (!_recipeWriteHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init recipe file: %s\n",
            recipePath_.c_str());
        exit(EXIT_FAILURE);
    }
    FileRecipeHead_t virtualRecipeEnd;
    virtualRecipeEnd.fileSize = _fileSize;
    virtualRecipeEnd.totalChunkNum = _totalChunkNum;
    _recipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));

    // for key recipe
    _keyRecipeWriteHandler.open(keyRecipePath_, ios_base::trunc | ios_base::binary);
    if (!_keyRecipeWriteHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init key recipe file: %s\n",
            keyRecipePath_.c_str());
        exit(EXIT_FAILURE);
    }
    _keyRecipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));

    // for secure recipe
    _secureRecipeWriteHandler.open(secureRecipePath_, ios_base::trunc | ios_base::binary);
    if (!_secureRecipeWriteHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init sec recipe file: %s\n",
            secureRecipePath_.c_str());
        exit(EXIT_FAILURE);
    }
    _secureRecipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));

    return ;
}

/**
 * @brief destory the upload buffer
 * 
 */
void ClientVar::DestoryUploadBuffer() {
    if (_recipeWriteHandler.is_open()) {
        _recipeWriteHandler.close();
    }
    if (_keyRecipeWriteHandler.is_open()) {
        _keyRecipeWriteHandler.close();
    }
    if (_secureRecipeWriteHandler.is_open()) {
        _keyRecipeWriteHandler.close();
    }
    free(_recvChunkBuf.sendBuffer);
    delete _inputMQ;
    EVP_MD_CTX_free(_mdCtx);
    EVP_CIPHER_CTX_free(_cipherCtx);
    return ;
}

/**
 * @brief init the restore buffer
 * 
 */
void ClientVar::InitDownloadRecipeBuffer() {
    // ---------------------------
    // 打开文件
    // ---------------------------
    _recipeReadHandler.open(recipePath_, ios_base::in | ios_base::binary);
    if (!_recipeReadHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
            recipePath_.c_str());
        exit(EXIT_FAILURE);
    }
    _secureRecipeReadHandler.open(secureRecipePath_, ios_base::in | ios_base::binary);
    if (!_secureRecipeReadHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
            secureRecipePath_.c_str());
        exit(EXIT_FAILURE);
    }
    _keyRecipeReadHandler.open(keyRecipePath_, ios_base::in | ios_base::binary);
    if (!_keyRecipeReadHandler.is_open()) {
        tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
            keyRecipePath_.c_str());
        exit(EXIT_FAILURE);
    }

    // ---------------------------
    // 初始化send buffer
    // ---------------------------
    if (sizeof(RecipeEntry_t) > sizeof(KeyRecipeEntry_t)) {
        _sendRecipeBuf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
            sendRecipeBatchSize_ * sizeof(RecipeEntry_t));
    }
    else {
        _sendRecipeBuf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
            sendRecipeBatchSize_ * sizeof(KeyRecipeEntry_t));
    }
    _sendRecipeBuf.header = (NetworkHead_t*) _sendRecipeBuf.sendBuffer;
    _sendRecipeBuf.header->clientID = _clientID;
    _sendRecipeBuf.header->currentItemNum = 0;
    _sendRecipeBuf.header->dataSize = 0;
    _sendRecipeBuf.dataBuffer = _sendRecipeBuf.sendBuffer + sizeof(NetworkHead_t);
    return ;
}

/**
 * @brief destory the restore buffer
 * 
 */
void ClientVar::DestoryDownloadRecipeBuffer() {
    if (_recipeReadHandler.is_open()) {
        _recipeReadHandler.close();
    }
    if (_keyRecipeReadHandler.is_open()) {
        _keyRecipeReadHandler.close();
    }
    if (_secureRecipeReadHandler.is_open()) {
        _secureRecipeReadHandler.close();
    }
    return ;
}

/**
 * @brief init the restore buffer
 * 
 */
void ClientVar::InitDownloadChunkBuffer() {
    // init buffer
    _reqContainer.idBuffer = (uint8_t*) malloc(CONTAINER_CAPPING_VALUE *
        CONTAINER_ID_LENGTH);
    _reqContainer.containerArray = (uint8_t**) malloc(CONTAINER_CAPPING_VALUE *
        sizeof(uint8_t*));
    _reqContainer.idNum = 0;
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        _reqContainer.containerArray[i] = (uint8_t*) malloc(sizeof(uint8_t) *
            MAX_CONTAINER_SIZE);
    }

    // init the send chunk buffer
    _sendChunkBuf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sendChunkBatchSize_ * (sizeof(uint32_t) + MAX_CHUNK_SIZE));
    _sendChunkBuf.header = (NetworkHead_t*) _sendChunkBuf.sendBuffer;
    _sendChunkBuf.header->clientID = _clientID;
    _sendChunkBuf.header->currentItemNum = 0;
    _sendChunkBuf.header->dataSize = 0;
    _sendChunkBuf.dataBuffer = _sendChunkBuf.sendBuffer + sizeof(NetworkHead_t);

    // init the container cache
    _containerCache = new ReadCache();

    _downloadChunkBase = (DownloadChunkEntry_t*) malloc(sizeof(DownloadChunkEntry_t) 
        * sendChunkBatchSize_);    
    return ;
}

/**
 * @brief destory the restore buffer
 * 
 */
void ClientVar::DestoryDownloadChunkBuffer() {
    free(_sendChunkBuf.sendBuffer);
    free(_reqContainer.idBuffer);
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        free(_reqContainer.containerArray[i]);
    }
    free(_reqContainer.containerArray);
    free(_downloadChunkBase);
    delete _containerCache;
    return ;
}

void ClientVar::ChangeFile(string newFileName, uint64_t fileSize, uint64_t totalChunkNum) {
    recipePath_ = config.GetRecipeRootPath() +
        newFileName + config.GetRecipeSuffix();
    secureRecipePath_ = config.GetRecipeRootPath() +
        newFileName + config.GetSecureRecipeSuffix();
    keyRecipePath_ = config.GetRecipeRootPath() +
        newFileName + config.GetKeyRecipeSuffix();

    _fileSize = fileSize;
    _totalChunkNum = totalChunkNum;
    
    switch (optType_) {
        case UPLOAD_OPT: {
            if (_recipeWriteHandler.is_open()) {
                _recipeWriteHandler.close();
            }
            if (_keyRecipeWriteHandler.is_open()) {
                _keyRecipeWriteHandler.close();
            }
            if (_secureRecipeWriteHandler.is_open()) {
                _secureRecipeWriteHandler.close();
            }
        
            // init the file recipe
            _recipeWriteHandler.open(recipePath_, ios_base::trunc | ios_base::binary);
            if (!_recipeWriteHandler.is_open()) {
                tool::Logging(myName_.c_str(), "cannot init recipe file: %s\n",
                    recipePath_.c_str());
                exit(EXIT_FAILURE);
            }
            FileRecipeHead_t virtualRecipeEnd;
            virtualRecipeEnd.fileSize = _fileSize;
            virtualRecipeEnd.totalChunkNum = _totalChunkNum;
            _recipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));
        
            // for key recipe
            _keyRecipeWriteHandler.open(keyRecipePath_, ios_base::trunc | ios_base::binary);
            if (!_keyRecipeWriteHandler.is_open()) {
                tool::Logging(myName_.c_str(), "cannot init key recipe file: %s\n",
                    keyRecipePath_.c_str());
                exit(EXIT_FAILURE);
            }
            _keyRecipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));
        
            // for secure recipe
            _secureRecipeWriteHandler.open(secureRecipePath_, ios_base::trunc | ios_base::binary);
            if (!_secureRecipeWriteHandler.is_open()) {
                tool::Logging(myName_.c_str(), "cannot init sec recipe file: %s\n",
                    secureRecipePath_.c_str());
                exit(EXIT_FAILURE);
            }
            _secureRecipeWriteHandler.write((char*)&virtualRecipeEnd, sizeof(FileRecipeHead_t));
            break;
        }
        case DOWNLOAD_RECIPE_OPT: {
            if (_recipeReadHandler.is_open()) {
                _recipeReadHandler.close();
            }
            if (_keyRecipeReadHandler.is_open()) {
                _keyRecipeReadHandler.close();
            }
            if (_secureRecipeReadHandler.is_open()) {
                _secureRecipeReadHandler.close();
            }

            _recipeReadHandler.open(recipePath_, ios_base::in | ios_base::binary);
            if (!_recipeReadHandler.is_open()) {
                tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
                    recipePath_.c_str());
                exit(EXIT_FAILURE);
            }
            _secureRecipeReadHandler.open(secureRecipePath_, ios_base::in | ios_base::binary);
            if (!_secureRecipeReadHandler.is_open()) {
                tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
                    secureRecipePath_.c_str());
                exit(EXIT_FAILURE);
            }
            _keyRecipeReadHandler.open(keyRecipePath_, ios_base::in | ios_base::binary);
            if (!_keyRecipeReadHandler.is_open()) {
            tool::Logging(myName_.c_str(), "cannot init the file recipe: %s.\n",
                keyRecipePath_.c_str());
            exit(EXIT_FAILURE);
            }
            break;
        }
    }
    //tool::Logging(myName_.c_str(), "change file start \n");
    return ;
}