/**
 * @file recvDecoder.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the recv decoder
 * @version 0.1
 * @date 2021-09-06
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/recvDecoder.h"

/**
 * @brief Construct a new Recv Decoder object
 * 
 * @param serverChannel the ssl connection pointer
 */
RecvDecoder::RecvDecoder(AbsIndex* absIndexObj, SSLConnection* serverChannel) : AbsRecvDecoder(serverChannel) {
    absIndexObj_ = absIndexObj;
    //tool::Logging(myName_.c_str(), "init the RecvDecoder.\n");
}

/**
 * @brief Destroy the Recv Decoder object
 * 
 */
RecvDecoder::~RecvDecoder() {
    // fprintf(stderr, "========RecvDecoder Info========\n");
    // fprintf(stderr, "read container from file num: %lu\n", readFromContainerFileNum_);
    // fprintf(stderr, "=======================================\n");
}

/**
 * @brief the main process
 * 
 * @param curClient the current client ptr
 */
void RecvDecoder::Run(ClientVar* curClient) {
    //tool::Logging(myName_.c_str(), "the main thread is running.\n");
    SSL* clientSSL = curClient->_clientSSL;
    SendMsgBuffer_t* sendChunkBuf = &curClient->_sendChunkBuf;
    uint32_t recvSize = 0;
    string clientIP;

    bool endFlag = false;

    // ------------------------
    // 等待edge回应
    // ------------------------
    if (!serverChannel_->ReceiveData(clientSSL, sendChunkBuf->sendBuffer,
        recvSize)) {
        tool::Logging(myName_.c_str(), "recv the client ready error.\n");
        exit(EXIT_FAILURE);
    } else {
        tool::Logging(myName_.c_str(), "01 Message type is %d\n", sendChunkBuf->header->messageType);
        if (sendChunkBuf->header->messageType != EDGE_RECEIVE_READY) {
            tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
            exit(EXIT_FAILURE);
        }
    }
    tool::Logging(myName_.c_str(), "ready to recieve data \n");
    recvSize = 0;
    while (true) {
        if (!serverChannel_->ReceiveData(clientSSL, sendChunkBuf->sendBuffer,
            recvSize)) {
            tool::Logging(myName_.c_str(), "download chunk finish.\n");
            serverChannel_->GetClientIp(clientIP, clientSSL);
            serverChannel_->ClearAcceptedClientSd(clientSSL);
            break ;
        } else {
            tool::Logging(myName_.c_str(), "02 Message type is %d\n", sendChunkBuf->header->messageType);
            if (sendChunkBuf->header->messageType != EDGE_DOWNLOAD_CHUNK_READY) {
                tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
                tool::Logging(myName_.c_str(), "data size is %d\n", sendChunkBuf->header->dataSize);
                exit(EXIT_FAILURE);
            }
            else {
                uint32_t recipeNum = sendChunkBuf->header->currentItemNum;
                sendChunkBuf->header->currentItemNum = 0;
                sendChunkBuf->header->dataSize = 0;
                this->ProcessRecipeBatch(sendChunkBuf->dataBuffer, recipeNum, curClient);       
            } 
        }
    }
    return ;
}

/**
 * @brief recover a chunk
 * 
 * @param chunkBuffer the chunk buffer
 * @param chunkSize the chunk size
 * @param restoreChunkBuf the restore chunk buffer
 */
void RecvDecoder::RecoverOneChunk(DownloadChunkEntry_t* entry, uint8_t* chunkBuffer,
    SendMsgBuffer_t* sendChunkBuf) {
    uint8_t* outputBuffer = sendChunkBuf->dataBuffer + 
        sendChunkBuf->header->dataSize;

    memcpy(outputBuffer, &entry->chunkSize, sizeof(uint32_t));
    memcpy(outputBuffer + sizeof(uint32_t), chunkBuffer + entry->chunkOffset, 
        entry->chunkSize);
    sendChunkBuf->header->dataSize += (sizeof(uint32_t) + entry->chunkSize);
    sendChunkBuf->header->currentItemNum++;

    return ;
}

/**
 * @brief process a batch of recipe
 * 
 * @param recipeBuffer the read recipe buffer
 * @param recipeNum the read recipe num
 * @param curClient the current client var
 */
void RecvDecoder::ProcessRecipeBatch(uint8_t* recipeBuffer, size_t recipeNum, 
    ClientVar* curClient) {
    ReqContainer_t* reqContainer = (ReqContainer_t*)&curClient->_reqContainer;
    uint8_t* idBuffer = reqContainer->idBuffer;
    uint8_t** containerArray = reqContainer->containerArray;
    SendMsgBuffer_t* sendChunkBuf = &curClient->_sendChunkBuf;

    unordered_map<string, uint32_t> tmpContainerMap;
    tmpContainerMap.reserve(CONTAINER_CAPPING_VALUE);

    unordered_map<string, RestoreIndexEntry_t> restoreIndex;
    
    DownloadChunkEntry_t* downloadChunkBase = curClient->_downloadChunkBase;
    DownloadChunkEntry_t* downloadChunkEntry = downloadChunkBase;

    uint32_t offset = 0;
    string tmpContainerNameStr;
    string tmpHashStr;
    tmpContainerNameStr.resize(CONTAINER_ID_LENGTH, 0);
    tmpHashStr.resize(CHUNK_HASH_SIZE, 0);
    tool::Logging(myName_.c_str(), "read index store first \n");
    for (size_t i = 0; i < recipeNum; i++) {
        memcpy(downloadChunkEntry->chunkHash, recipeBuffer + offset, CHUNK_HASH_SIZE);
        tmpHashStr.assign((char*)downloadChunkEntry->chunkHash, CHUNK_HASH_SIZE);
        auto result = absIndexObj_->ReadIndexStore(tmpHashStr, tmpContainerNameStr);
        if(!result)
        {
            tool::Logging(myName_.c_str(), "no find\n");
        }
        memcpy(downloadChunkEntry->containerName, tmpContainerNameStr.c_str(), 
            CONTAINER_ID_LENGTH);
        downloadChunkEntry++;
        offset += sizeof(RecipeEntry_t);
    }

    downloadChunkEntry = downloadChunkBase;
    DownloadChunkEntry_t* startEntry = downloadChunkBase;
    DownloadChunkEntry_t* endEntry = startEntry;
    //tool::Logging(myName_.c_str(), "start send chunk recipe num is %d\n", recipeNum);
    for (size_t i = 0; i < recipeNum; i++) {
        tmpContainerNameStr.assign((char*)downloadChunkEntry->containerName, CONTAINER_ID_LENGTH);
        auto findResult = tmpContainerMap.find(tmpContainerNameStr);
        if (findResult == tmpContainerMap.end()) {
           // tool::Logging(myName_.c_str(),"enter\n");
            downloadChunkEntry->containerID = reqContainer->idNum;
            tmpContainerMap[tmpContainerNameStr] = reqContainer->idNum;
            memcpy(idBuffer + reqContainer->idNum * CONTAINER_ID_LENGTH, 
                tmpContainerNameStr.c_str(), CONTAINER_ID_LENGTH);
            reqContainer->idNum++;
        }
        else {
            downloadChunkEntry->containerID = findResult->second;
        }
        downloadChunkEntry++;
        // 如果container到达上限，开始处理每个chunk

        if (reqContainer->idNum == CONTAINER_CAPPING_VALUE || i == (recipeNum - 1)) {
            tool::Logging(myName_.c_str(), "start send chunk \n");
            endEntry = downloadChunkEntry;
            this->GetReqContainers(curClient);
            this->IndexConstruct(restoreIndex, containerArray, reqContainer->idNum);

            while (startEntry != endEntry) {
                string tmpHashStr;
                tmpHashStr.assign((char*)startEntry->chunkHash, CHUNK_HASH_SIZE);
                auto findResult = restoreIndex.find(tmpHashStr);
                startEntry->chunkOffset = findResult->second.offset;
                startEntry->chunkSize = findResult->second.length;
                uint8_t* containerContent = containerArray[startEntry->containerID];

                this->RecoverOneChunk(startEntry, containerContent, sendChunkBuf);

                startEntry++;

                // chunk buf已满，准备传送
                //tool::Logging(myName_.c_str(), "current item num is %d\n", sendChunkBuf->header->currentItemNum);
                if (sendChunkBuf->header->currentItemNum % sendChunkBatchSize_ == 0) {
                    
                    // copy the header to the send buffer
                    sendChunkBuf->header->messageType = CLOUD_SEND_CHUNK;
                    //tool::Logging(myName_.c_str(), "send batch chunk \n");
                    this->SendBatchChunks(sendChunkBuf, curClient->_clientSSL);

                    sendChunkBuf->header->dataSize = 0;
                    sendChunkBuf->header->currentItemNum = 0;
                }
            }

            if(i == (recipeNum - 1) && sendChunkBuf->header->currentItemNum % sendChunkBatchSize_  !=  0)
            {
                sendChunkBuf->header->messageType = CLOUD_SEND_CHUNK;
                //tool::Logging(myName_.c_str(), "send last batch chunk \n");
                this->SendBatchChunks(sendChunkBuf, curClient->_clientSSL);
                
                sendChunkBuf->header->dataSize = 0;
                sendChunkBuf->header->currentItemNum = 0;
            }

            // 重置
            reqContainer->idNum = 0;
            tmpContainerMap.clear();
            restoreIndex.clear();
        }
    }

    this->ProcessRecipeTailBatch(curClient);

    return ;
}

/**
 * @brief process the tail batch of the recipe
 * 
 * @param curClient the current client var
 */
void RecvDecoder::ProcessRecipeTailBatch(ClientVar* curClient) {
    SendMsgBuffer_t* sendChunkBuf = &curClient->_sendChunkBuf;
    sendChunkBuf->header->messageType = CLOUD_SEND_CHUNK_END;
    this->SendBatchChunks(sendChunkBuf, curClient->_clientSSL);
    return ;
}

/**
 * @brief Get the Required Containers object 
 * 
 * @param curClient the current client ptr
 */
void RecvDecoder::GetReqContainers(ClientVar* curClient) {
    ReqContainer_t* reqContainer = &curClient->_reqContainer;
    uint8_t* idBuffer = reqContainer->idBuffer; 
    //tool::PrintBinaryArray(idBuffer, CONTAINER_ID_LENGTH);
    uint8_t** containerArray = reqContainer->containerArray;
    ReadCache* containerCache = curClient->_containerCache;
    uint32_t idNum = reqContainer->idNum; 

    // retrieve each container
    string containerNameStr;
    for (size_t i = 0; i < idNum; i++) {
        containerNameStr.assign((char*) (idBuffer + i * CONTAINER_ID_LENGTH), 
            CONTAINER_ID_LENGTH);
        // step-1: check the container cache'
        //tool::Logging(myName_.c_str(), "container name is %s\n", containerNameStr.c_str());
        bool cacheHitStatus = containerCache->ExistsInCache(containerNameStr);
        if (cacheHitStatus) {
            // step-2: exist in the container cache, read from the cache, directly copy the data from the cache
            memcpy(containerArray[i], containerCache->ReadFromCache(containerNameStr), 
                MAX_CONTAINER_SIZE);
            continue ;
        } 

        // step-3: not exist in the contain cache, read from disk
        ifstream containerIn;
        string readFileNameStr = containerNamePrefix_ + containerNameStr + containerNameTail_;
        containerIn.open(readFileNameStr, ifstream::in | ifstream::binary);
        //tool::Logging(myName_.c_str(), "read file name is %s\n", readFileNameStr.c_str());
        if (!containerIn.is_open()) {
            tool::Logging(myName_.c_str(), "cannot open the container: %s\n", readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        }

        // get the data section size (total chunk size - metadata section)
        containerIn.seekg(0, ios_base::end);
        int readSize = containerIn.tellg();
        containerIn.seekg(0, ios_base::beg);

        // read the metadata section
        int containerSize = 0;
        containerSize = readSize;
        // read compression data
        containerIn.read((char*)containerArray[i], containerSize);

        if (containerIn.gcount() != containerSize) {
            tool::Logging(myName_.c_str(), "read size %lu cannot match expected size: %d for container %s.\n",
                containerIn.gcount(), containerSize, readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        } 

        // close the container file
        containerIn.close();
        readFromContainerFileNum_++;
        containerCache->InsertToCache(containerNameStr, containerArray[i], containerSize);
    }
    
    tool::Logging(myName_.c_str(), "get req container done\n");
    return ;
}

/**
 * @brief send the restore chunk to the client
 * 
 * @param sendChunkBuf the send chunk buffer
 * @param clientSSL the ssl connection
 */
void RecvDecoder::SendBatchChunks(SendMsgBuffer_t* sendChunkBuf, 
    SSL* clientSSL) {
    if (!serverChannel_->SendData(clientSSL, sendChunkBuf->sendBuffer, 
        sizeof(NetworkHead_t) + sendChunkBuf->header->dataSize)) {
        tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
        exit(EXIT_FAILURE);
    }
    return ;
}

void RecvDecoder::IndexConstruct(unordered_map<string, RestoreIndexEntry_t>& restoreIndex, 
    uint8_t** containerArray, uint32_t idNum) {
    tool::Logging(myName_.c_str(), "index construct.....\n");
    for (size_t i = 0; i < idNum; i++) {
        size_t chunkNum = (containerArray[i][3]) + (containerArray[i][2] << 8) + 
            (containerArray[i][1] << 16) + (containerArray[i][0] << 24);
        //tool::Logging(myName_.c_str(), "chunk num is %d\n", chunkNum);
        //tool::PrintBinaryArray(containerArray[i], CHUNK_HASH_SIZE);
        //memcpy((containerArray[i] + 4), containerArray[i] + 4, chunkNum * (CHUNK_HASH_SIZE+8));
       // tool::Logging(myName_.c_str(), "memcpy done\n");
        for (size_t j = 0; j < chunkNum; j++) {
            string tmpChunkHash;
            //tool::Logging(myName_.c_str(), "j is %d\n", j);
            tmpChunkHash.assign((char*)(containerArray[i] + 4) + (j * (CHUNK_HASH_SIZE + 8)), CHUNK_HASH_SIZE);
            //tool::PrintBinaryArray(containerArray[i] + (j * (CHUNK_HASH_SIZE + 8)), CHUNK_HASH_SIZE);
            RestoreIndexEntry_t tmpRestoreEntry;
            tmpRestoreEntry.offset = 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 3]) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 2] << 8) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 1] << 16) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 0] << 24) +
                chunkNum * (CHUNK_HASH_SIZE + 8) + 4;
           //tool::Logging(myName_.c_str(), "offset  is %d\n", tmpRestoreEntry.offset);
            tmpRestoreEntry.length = 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 7]) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 6] << 8) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 5] << 16) + 
                ((containerArray[i] + 4)[j * (CHUNK_HASH_SIZE + 8) + CHUNK_HASH_SIZE + 4] << 24);
            restoreIndex[tmpChunkHash] = tmpRestoreEntry;
           //tool::Logging(myName_.c_str(), "len is %d\n", tmpRestoreEntry.length);
        }
    }
    tool::Logging(myName_.c_str(), "index construct done\n");
    return ;

}