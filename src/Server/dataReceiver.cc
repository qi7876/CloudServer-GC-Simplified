/**
 * @file dataReceiver.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of data receivers 
 * @version 0.1
 * @date 2021-01-27
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/dataReceiver.h"

/**
 * @brief Construct a new DataReceiver object
 * 
 * @param absIndexObj the pointer to the index obj
 * @param serverChannel the pointer to the security channel
 */
DataReceiver::DataReceiver(AbsIndex* absIndexObj, SSLConnection* serverChannel) {
    // set up the connection and interface
    serverChannel_ = serverChannel;
    absIndexObj_ = absIndexObj;
    //tool::Logging(myName_.c_str(), "init the DataReceiver.\n");
}

/**
 * @brief Destroy the DataReceiver object
 * 
 */
DataReceiver::~DataReceiver() {
    // fprintf(stderr, "========DataReceiver Info========\n");
    // fprintf(stderr, "total recv batch num: %lu\n", batchNum_);
    // fprintf(stderr, "total recv recipe end num: %lu\n", recipeEndNum_);
    // fprintf(stderr, "=================================\n");
}

/**
 * @brief the main process to handle new client upload-request connection
 * 
 * @param curClient the ptr to the current client
 * @param enclaveInfo the ptr to the enclave info
 */
void DataReceiver::Run(ClientVar* curClient, EnclaveInfo_t* enclaveInfo) {
    uint32_t recvSize = 0;
    string clientIP;
    SendMsgBuffer_t* recvChunkBuf = &curClient->_recvChunkBuf;
    InmemoryContainer_t* curContainer = &curClient->_curContainer;
    SSL* clientSSL = curClient->_clientSSL;
    absIndexObj_->_logicalDataSize += curClient->_fileSize;
    absIndexObj_->_logicalChunkNum += curClient->_totalChunkNum;
    tool::Logging(myName_.c_str(), "Start Migration...\n");
    bool isEnd = false;
    while (!isEnd) {
        // receive data
        if (!serverChannel_->ReceiveData(clientSSL, recvChunkBuf->sendBuffer, 
            recvSize)) {
            tool::Logging(myName_.c_str(), "Migration Done!\n");
            serverChannel_->GetClientIp(clientIP, clientSSL);
            serverChannel_->ClearAcceptedClientSd(clientSSL);
            isEnd = true;
            break;
        } else {
            switch (recvChunkBuf->header->messageType) {
                case EDGE_MIGRATION_CHUNK: {
                    absIndexObj_->ProcessOneBatch(recvChunkBuf, curClient);
                    batchNum_++;
                    break;
                }
                case EDGE_MIGRATION_CHUNK_FINAL: {
                    //tool::Logging(myName_.c_str(), "migrate chunk done\n");
                    //isEnd = true;
                    //tool::Logging(myName_.c_str(), "EdgeServer-%d Upload Chunk Done...\n", curClient->_clientID);
                    break;
                }
                case EDGE_UPLOAD_SEC_RECIPE: {
                    absIndexObj_->ProcessRecipeBatch(recvChunkBuf, curClient);
                    serverChannel_->SendData(clientSSL, recvChunkBuf->sendBuffer, 
                        sizeof(NetworkHead_t) + recvChunkBuf->header->dataSize);
                    recipeBatchNum_++;
                    break;
                }
                case EDGE_UPLOAD_SEC_RECIPE_END: {
                    //tool::Logging(myName_.c_str(), "Upload Secure Recipe Done...\n");
                    break;
                }
                case EDGE_MIGRATION_NEWFILE: {
                    string fileName;
                    fileName.assign((char*) recvChunkBuf->dataBuffer, CHUNK_HASH_SIZE * 2); 
                    FileRecipeHead_t* tmpRecipeHead = 
                        (FileRecipeHead_t*)(recvChunkBuf->dataBuffer + CHUNK_HASH_SIZE * 2);
                    curClient->ChangeFile(fileName, tmpRecipeHead->fileSize, tmpRecipeHead->totalChunkNum);
                    break;
                }
                case EDGE_UPLOAD_RECIPE: {
                    curClient->_recipeWriteHandler.write((char*)recvChunkBuf->dataBuffer, 
                        recvChunkBuf->header->currentItemNum * CHUNK_HASH_SIZE);
                    break;
                }
                case EDGE_UPLOAD_KEY_RECIPE: {
                    curClient->_keyRecipeWriteHandler.write((char*)recvChunkBuf->dataBuffer, 
                        recvChunkBuf->header->currentItemNum * CHUNK_HASH_SIZE);
                    break;
                }
                case EDGE_UPLOAD_RECIPE_END: {
                    // finalize the file recipe
                    recipeEndNum_++;

                    // update the upload data size
                    FileRecipeHead_t* tmpRecipeHead = (FileRecipeHead_t*)recvChunkBuf->dataBuffer;
                    curClient->_uploadDataSize = tmpRecipeHead->fileSize;
                    break;
                }
                default: {
                    // receive teh wrong message type
                    tool::Logging(myName_.c_str(), "wrong received message type.\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // process the last container
    if (curContainer->currentHeaderSize != 0) {
        //tool::Logging(myName_.c_str(), "process last container\n");
        Ocall_WriteContainer(curClient);
        //tool::Logging(myName_.c_str(), "process last container done\n");
    }
    curClient->_inputMQ->done_ = true;

    enclaveInfo->logicalDataSize = absIndexObj_->_logicalDataSize;
    enclaveInfo->logicalChunkNum = absIndexObj_->_logicalChunkNum;
    enclaveInfo->uniqueDataSize = absIndexObj_->_uniqueDataSize;
    enclaveInfo->uniqueChunkNum = absIndexObj_->_uniqueChunkNum;
    enclaveInfo->compressedSize = absIndexObj_->_compressedDataSize;
    return ; 
}