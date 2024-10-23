/**
 * @file plainIndex.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement plain index
 * @version 0.1
 * @date 2021-05-14
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/plainIndex.h"

/**
 * @brief Construct a new Plain Index object
 * 
 * @param indexStore the reference to the index store
 */
PlainIndex::PlainIndex(AbsDatabase* indexStore) : AbsIndex (indexStore) {
    //tool::Logging(myName_.c_str(), "init the PlainIndex.\n");
}

/**
 * @brief Destroy the Plain Index object destore the plain index
 * 
 */
PlainIndex::~PlainIndex() {
    fprintf(stderr, "========CloudServer Info========\n");
    // fprintf(stderr, "recv data size: %lu\n", totalRecvDataSize_);
    // fprintf(stderr, "recv batch num: %lu\n", totalBatchNum_);
    //fprintf(stderr, "logical chunk num: %lu\n", _logicalChunkNum);
    fprintf(stderr, "total logical data size: %lu MiB\n", _logicalDataSize / (1024*1024));
    //fprintf(stderr, "physical chunk num: %lu\n", _uniqueChunkNum);
    fprintf(stderr, "total write data size: %lu MiB\n", _uniqueDataSize / (1024*1024));
    fprintf(stderr, "===============================\n");
}

/**
 * @brief process one batch 
 * 
 * @param recvChunkBuf the recv chunk buffer
 * @param curClient the current client var
 */
void PlainIndex::ProcessOneBatch(SendMsgBuffer_t* recvChunkBuf, ClientVar* curClient) {
    // update statistic
    totalRecvDataSize_ += recvChunkBuf->header->dataSize;
    totalBatchNum_++;

    // the client info
    EVP_MD_CTX* mdCtx = curClient->_mdCtx;

    // get the chunk num
    uint32_t chunkNum = recvChunkBuf->header->currentItemNum;
    //tool::Logging(myName_.c_str(), "chunk num is %d\n", chunkNum);

    // start to process each chunk
    string containerNameStr;
    containerNameStr.resize(CONTAINER_ID_LENGTH, 0);
    size_t currentOffset = 0;
    uint32_t tmpChunkSize = 0;
    string tmpHashStr;
    tmpHashStr.resize(CHUNK_HASH_SIZE, 0);
    bool status;

    for (size_t i = 0; i < chunkNum; i++) {
        // compute the hash over the ciphertext chunk
        memcpy(&tmpChunkSize, recvChunkBuf->dataBuffer + currentOffset, sizeof(tmpChunkSize));
        currentOffset += sizeof(tmpChunkSize);
        
        cryptoObj_->GenerateHash(mdCtx, recvChunkBuf->dataBuffer + currentOffset, 
            tmpChunkSize, (uint8_t*)&tmpHashStr[0]);

#if (MULTI_CLIENT == 1)
    pthread_rwlock_wrlock(&outIdxLck_);
#endif
   
        status = this->ReadIndexStore(tmpHashStr, containerNameStr);
        if(!status){ 
            storageCoreObj_->SaveChunk((char*)recvChunkBuf->dataBuffer + currentOffset, tmpChunkSize, 
                tmpHashStr, containerNameStr, curClient);
            
            this->UpdateIndexStore(tmpHashStr, containerNameStr);
            _uniqueChunkNum++;
            _uniqueDataSize += tmpChunkSize;
        }
#if (MULTI_CLIENT == 1)
    pthread_rwlock_unlock(&outIdxLck_);
#endif  
        currentOffset += tmpChunkSize;
        // update the statistic
        // _logicalDataSize += tmpChunkSize;
        // _logicalChunkNum++;
    }

    // reset
    memset(recvChunkBuf->dataBuffer, 0, sendChunkBatchSize_ * sizeof(Chunk_t));
    return ;
}

void PlainIndex::ProcessRecipeBatch(SendMsgBuffer_t* recvChunkBuf, ClientVar* curClient) {
    // get the chunk num
    uint32_t entryNum = recvChunkBuf->header->currentItemNum;
    //tool::Logging("sec recipe", "entry num is %d\n", entryNum);
    uint8_t* entryBase = recvChunkBuf->dataBuffer;

    string tmpHashStr;
    string tmpContainerNameStr;
    tmpContainerNameStr.resize(CONTAINER_ID_LENGTH, 0);
    bool status;
    uint8_t* statusList;

    statusList = (uint8_t*) malloc(sizeof(uint8_t) * entryNum); 
  

    // 首先写recipe
    curClient->_secureRecipeWriteHandler.write((char*)recvChunkBuf->dataBuffer,
        recvChunkBuf->header->dataSize);
    //tool::PrintBinaryArray(recvChunkBuf->sendBuffer, CHUNK_HASH_SIZE);
    // 查询index
    for (size_t i = 0; i < entryNum; i++) {
        tmpHashStr.assign((char*)(entryBase), CHUNK_HASH_SIZE);
        entryBase += CHUNK_HASH_SIZE;
        status = this->ReadIndexStore(tmpHashStr, tmpContainerNameStr);
        //std::cout << "secFP" << std::endl;
        //tool::PrintBinaryArray((uint8_t*)&tmpHashStr[0], CHUNK_HASH_SIZE);
        if (status == true) 
        {
            statusList[i] = 0;
        }
        else
        {
            // string emptyContainerName = " ";
            // this->UpdateIndexStore(tmpHashStr, emptyContainerName);
            statusList[i] = 1;
        }
    }

    //tool::PrintBinaryArray(statusList, sizeof(uint8_t) * entryNum);
    memcpy(recvChunkBuf->dataBuffer, statusList, entryNum);
    recvChunkBuf->header->messageType = CLOUD_QUERY_RETURN;
    recvChunkBuf->header->dataSize = entryNum;

    return ;
}