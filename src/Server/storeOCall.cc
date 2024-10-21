/**
 * @file storeOCall.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2022-04-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "../../include/storeOCall.h"

namespace OutEnclave {
    string myName_ = "OCall";
};

using namespace OutEnclave;

/**
 * @brief dump the inside container to the outside buffer
 * 
 * @param outClient the out-enclave client ptr
 */
void Ocall_WriteContainer(void* outClient) {
    ClientVar* curClient = (ClientVar*)outClient;
    InmemoryContainer_t* curContainer = &curClient->_curContainer;
    Container_t writeContainer;
    uint8_t curNumChar[4];
    curNumChar[0] = curContainer->chunkNum >> 24;
    curNumChar[1] = curContainer->chunkNum >> 16;
    curNumChar[2] = curContainer->chunkNum >> 8;
    curNumChar[3] = curContainer->chunkNum;

    memcpy(writeContainer.containerID, curContainer->containerID, CONTAINER_ID_LENGTH);
    uint32_t writeOffset = 0;   
    memcpy(writeContainer.body + writeOffset, curNumChar, 4);
    writeOffset += 4;
    memcpy(writeContainer.body + writeOffset, &curClient->_curContainer.header, 
        curClient->_curContainer.currentHeaderSize);
    writeOffset += curClient->_curContainer.currentHeaderSize;
    memcpy(writeContainer.body + writeOffset, &curClient->_curContainer.body, 
        curClient->_curContainer.currentBodySize);
    writeOffset += curClient->_curContainer.currentBodySize;
    // claim cuurentSize 
    writeContainer.currentSize = writeOffset;
    curClient->_inputMQ->Push(writeContainer);
    
    return ;
}