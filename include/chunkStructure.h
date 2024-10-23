/**
 * @file chunkStructure.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define the necessary data structure in deduplication
 * @version 0.1
 * @date 2019-12-19
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#ifndef BASICDEDUP_CHUNK_h
#define BASICDEDUP_CHUNK_h

#include "constVar.h"
#include <stdint.h>

typedef struct {
    uint32_t chunkSize;
    uint8_t data[MAX_CHUNK_SIZE];
} Chunk_t;

typedef struct {
    uint64_t fileSize;
    uint64_t totalChunkNum;
} FileRecipeHead_t;

typedef struct {
    union {
        Chunk_t chunk;
        FileRecipeHead_t recipeHead;
    };
    int dataType;
} Data_t;

typedef struct {
    uint8_t chunkHash[CHUNK_HASH_SIZE]; 
} RecipeEntry_t;

typedef struct {
    uint8_t key[MLE_KEY_SIZE];
} KeyRecipeEntry_t;

typedef struct {
    uint64_t sendChunkBatchSize;
    uint64_t sendRecipeBatchSize;
    uint64_t topKParam;
} EnclaveConfig_t;

typedef struct {
    uint64_t uniqueChunkNum;
    uint64_t uniqueDataSize;
    uint64_t logicalChunkNum;
    uint64_t logicalDataSize;
    uint64_t compressedSize;
    double enclaveProcessTime;
#if (SGX_BREAKDOWN == 1)
    double dataTranTime;
    double fpTime;
    double freqTime;
    double firstDedupTime;
    double secondDedupTime;
    double compTime;
    double encTime;
#endif
} EnclaveInfo_t;

typedef struct {
    int messageType;
    uint32_t clientID;
    uint32_t dataSize;
    uint32_t currentItemNum;
} NetworkHead_t;

typedef struct {
    NetworkHead_t* header;
    uint8_t* sendBuffer;
    uint8_t* dataBuffer;
} SendMsgBuffer_t;

typedef struct {
    uint32_t recipeNum;
    uint8_t* entryList;
} Recipe_t;

typedef struct {
    char containerID[CONTAINER_ID_LENGTH];
    uint8_t body[MAX_CONTAINER_SIZE]; 
    uint32_t currentSize;
} Container_t;

typedef struct {
    char containerID[CONTAINER_ID_LENGTH];
    uint32_t chunkNum;
    uint8_t body[MAX_CONTAINER_SIZE]; 
    uint32_t currentBodySize;
    uint8_t header[MAX_CONTAINER_SIZE]; 
    uint32_t currentHeaderSize;
} InmemoryContainer_t;

typedef struct {
    uint8_t* idBuffer;
    uint8_t** containerArray;
    uint32_t idNum;
} ReqContainer_t;

typedef struct {
    uint8_t chunkHash[CHUNK_HASH_SIZE];
    uint8_t containerName[CONTAINER_ID_LENGTH]; 
    uint32_t chunkOffset; 
    uint32_t chunkSize;
    uint32_t containerID;
} DownloadChunkEntry_t;

typedef struct {
    uint32_t offset;
    uint32_t length;
} RestoreIndexEntry_t;

#endif //BASICDEDUP_CHUNK_h