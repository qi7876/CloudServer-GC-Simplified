/**
 * @file storageCore.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interfaces defined in the storage core.
 * @version 0.1
 * @date 2019-12-27
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "../../include/storageCore.h"

extern Configure config;

/**
 * @brief Construct a new Storage Core object
 * 
 */
StorageCore::StorageCore() {
    //tool::Logging(myName_.c_str(), "Init the StorageCore\n");
}

/**
 * @brief Destroy the Storage Core:: Storage Core object
 * 
 */
StorageCore::~StorageCore() {
    // fprintf(stderr, "========StorageCore Info========\n");
    // fprintf(stderr, "write the data size: %lu\n", writtenDataSize_);
    // fprintf(stderr, "write chunk num: %lu\n", writtenChunkNum_);
    // fprintf(stderr, "================================\n");

}

/**
 * @brief write the data to a container according to the given metadata
 * 
 * @param key chunk metadata
 * @param data content
 * @param curContainer the pointer to the current container
 * @param curClient the ptr to the current client
 */
void StorageCore::WriteContainer(string& containerName, char* data, uint32_t dataSize,
    string& chunkHash, InmemoryContainer_t* curContainer, ClientVar* curClient) {
    
    uint32_t currentBodyOffset = curContainer->currentBodySize;
    uint32_t writeBodyOffset = currentBodyOffset;
    uint32_t currentHeaderOffset = curContainer->currentHeaderSize;
    uint32_t writeHeaderOffset = currentHeaderOffset;

    uint32_t writeSize = dataSize + CHUNK_HASH_SIZE + sizeof(uint32_t) * 2;
 
    if (writeSize + currentBodyOffset + currentHeaderOffset + 4 >= MAX_CONTAINER_SIZE) {
        // write container
        Ocall_WriteContainer(curClient);

        // reset current container
        tool::CreateUUID(curClient->_curContainer.containerID, 
            CONTAINER_ID_LENGTH);
        curClient->_curContainer.currentBodySize = 0;
        curClient->_curContainer.currentHeaderSize = 0;
        curClient->_curContainer.chunkNum = 0;
       
        currentBodyOffset = curContainer->currentBodySize;
        writeBodyOffset = currentBodyOffset;
        currentHeaderOffset = curContainer->currentHeaderSize;
        writeHeaderOffset = currentHeaderOffset;
    }
    // 把int转换为4*char
    uint8_t offsetChar[4];
    // 写offset
    offsetChar[0] = writeBodyOffset >> 24;
    offsetChar[1] = writeBodyOffset >> 16;
    offsetChar[2] = writeBodyOffset >> 8;
    offsetChar[3] = writeBodyOffset;
    uint8_t lengthChar[4];
    lengthChar[0] = dataSize >> 24;
    lengthChar[1] = dataSize >> 16;
    lengthChar[2] = dataSize >> 8;
    lengthChar[3] = dataSize;
    // save chunk
    memcpy(curContainer->body + writeBodyOffset, data, dataSize);
    memcpy(curContainer->header + writeHeaderOffset, chunkHash.c_str(), CHUNK_HASH_SIZE);
    writeHeaderOffset += CHUNK_HASH_SIZE;
    memcpy(curContainer->header + writeHeaderOffset, offsetChar, sizeof(uint32_t));
    writeHeaderOffset += sizeof(uint32_t);
    memcpy(curContainer->header + writeHeaderOffset, lengthChar, sizeof(uint32_t));
    // update the metadata of the container
    curContainer->currentHeaderSize += (CHUNK_HASH_SIZE + sizeof(uint32_t) * 2);
    curContainer->currentBodySize += dataSize;
    curContainer->chunkNum ++;

    containerName.assign(curContainer->containerID, CONTAINER_ID_LENGTH);
    return ;
}

/**
 * @brief save the chunk to the storage server
 * 
 * @param chunkData the chunk data buffer
 * @param chunkSize the chunk size
 * @param chunkAddr the chunk address (return)
 * @param curClient the prt to current client
 */
void StorageCore::SaveChunk(char* chunkData, uint32_t chunkSize, string& chunkHash,
    string& containerName, ClientVar* curClient) {
    
    InmemoryContainer_t* curContainer = &curClient->_curContainer;
        
    // write to the container
    this->WriteContainer(containerName, chunkData, chunkSize, 
        chunkHash, curContainer, curClient);
    writtenDataSize_ += chunkSize;
    writtenChunkNum_++;

    return ;
}

/**
 * @brief update the file recipe to the disk
 * 
 * @param recipeBuffer the pointer to the recipe buffer
 * @param recipeEntryNum the number of recipe entries
 * @param fileRecipeHandler the recipe file handler
 */
void StorageCore::UpdateRecipeToFile(const uint8_t* recipeBuffer, size_t recipeEntryNum, 
    ofstream& fileRecipeHandler) {
    if (!fileRecipeHandler.is_open()) {
        tool::Logging(myName_.c_str(), "recipe file does not open.\n");
        exit(EXIT_FAILURE);
    }
    size_t recipeBufferSize = recipeEntryNum * sizeof(RecipeEntry_t);
    fileRecipeHandler.write((char*)recipeBuffer, recipeBufferSize);
    return ;
}

/**
 * @brief finalize the file recipe
 * 
 * @param recipeHead the recipe header
 * @param fileRecipeHandler the recipe file handler
 */
void StorageCore::FinalizeRecipe(FileRecipeHead_t* recipeHead, 
    ofstream& fileRecipeHandler) {
    if (!fileRecipeHandler.is_open()) {
        tool::Logging(myName_.c_str(), "recipe file does not open.\n");
        exit(EXIT_FAILURE);
    }
    fileRecipeHandler.seekp(0, ios_base::beg);
    fileRecipeHandler.write((const char*)recipeHead, sizeof(FileRecipeHead_t));

    fileRecipeHandler.close();    
    return ; 
}