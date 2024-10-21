#include "../../include/recipeSender.h"

RecipeSender::RecipeSender(SSLConnection* serverChannel) {
    serverChannel_ = serverChannel;
    sendRecipeBatchSize_ = config.GetSendRecipeBatchSize();
}

RecipeSender::~RecipeSender() {

}

void RecipeSender::Run(ClientVar *curClient) {
    SendMsgBuffer_t* sendRecipeBuffer = &curClient->_sendRecipeBuf;
    SSL* clientSSL = curClient->_clientSSL;
    uint32_t recvSize = 0;

    // ------------------------
    // 等待edge回应
    // ------------------------
    if (!serverChannel_->ReceiveData(clientSSL, sendRecipeBuffer->sendBuffer,
        recvSize)) {
        tool::Logging(myName_.c_str(), "recv the client ready error.\n");
        exit(EXIT_FAILURE);
    } else {
        if (sendRecipeBuffer->header->messageType != EDGE_RECEIVE_READY) {
            tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
            exit(EXIT_FAILURE);
        }
    }

    uint8_t* tmpBuf = (uint8_t*)malloc(sizeof(FileRecipeHead_t));

    // ------------------------
    // 1.读取并发送plain recipe
    // ------------------------
    tool::Logging(myName_.c_str(), "start to read the file recipe.\n");
    bool end = false;
    while (!end) {
        // read a batch of the recipe entries from the recipe file
        curClient->_recipeReadHandler.read((char*)sendRecipeBuffer->dataBuffer, 
            sizeof(RecipeEntry_t) * sendRecipeBatchSize_);
        size_t readCnt = curClient->_recipeReadHandler.gcount();
        end = curClient->_recipeReadHandler.eof();
        size_t recipeEntryNum = readCnt / sizeof(RecipeEntry_t);
        if (readCnt == 0) {
            break;
        }
        sendRecipeBuffer->header->currentItemNum = recipeEntryNum;
        sendRecipeBuffer->header->dataSize = readCnt;
        sendRecipeBuffer->header->messageType = CLOUD_SEND_RECIPE;

        //tool::PrintBinaryArray(sendRecipeBuffer->dataBuffer, 6);
        if (!serverChannel_->SendData(clientSSL, sendRecipeBuffer->sendBuffer, 
        sizeof(NetworkHead_t) + sendRecipeBuffer->header->dataSize)) {
            tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
            exit(EXIT_FAILURE);
        }

        if (!serverChannel_->ReceiveData(clientSSL, sendRecipeBuffer->sendBuffer,
            recvSize)) {
            tool::Logging(myName_.c_str(), "recv the client ready error.\n");
            exit(EXIT_FAILURE);
        } else {
            if (sendRecipeBuffer->header->messageType != EDGE_RECEIVE_READY) {
                tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // ------------------------
    // 2.读取并发送secure recipe
    // ------------------------
    tool::Logging(myName_.c_str(), "start to read the sec file recipe.\n");
    end = false;
    FileRecipeHead_t* tmpRecipeHead = (FileRecipeHead_t*) malloc(sizeof(FileRecipeHead_t));
    curClient->_secureRecipeReadHandler.read((char*)tmpRecipeHead, sizeof(FileRecipeHead_t));
    while (!end) {
        // read a batch of the recipe entries from the recipe file
        curClient->_secureRecipeReadHandler.read((char*)sendRecipeBuffer->dataBuffer, 
            sizeof(RecipeEntry_t) * sendRecipeBatchSize_);
        size_t readCnt = curClient->_secureRecipeReadHandler.gcount();
        end = curClient->_secureRecipeReadHandler.eof();
        size_t recipeEntryNum = readCnt / sizeof(RecipeEntry_t);
        if (readCnt == 0) {
            break;
        }
        sendRecipeBuffer->header->currentItemNum = recipeEntryNum;
        sendRecipeBuffer->header->dataSize = readCnt;
        sendRecipeBuffer->header->messageType = CLOUD_SEND_SECURE_RECIPE;

        //tool::PrintBinaryArray(sendRecipeBuffer->dataBuffer, 6);
        if (!serverChannel_->SendData(clientSSL, sendRecipeBuffer->sendBuffer, 
        sizeof(NetworkHead_t) + sendRecipeBuffer->header->dataSize)) {
            tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
            exit(EXIT_FAILURE);
        }

        if (!serverChannel_->ReceiveData(clientSSL, sendRecipeBuffer->sendBuffer,
            recvSize)) {
            tool::Logging(myName_.c_str(), "recv the client ready error.\n");
            exit(EXIT_FAILURE);
        } else {
            if (sendRecipeBuffer->header->messageType != EDGE_RECEIVE_READY) {
                tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // ------------------------
    // 3.读取并发送key recipe
    // ------------------------
    curClient->_keyRecipeReadHandler.read((char*)tmpRecipeHead, sizeof(FileRecipeHead_t));
    free(tmpRecipeHead);
    tool::Logging(myName_.c_str(), "start to read the key file recipe.\n");
    end = false;
    while (!end) {
        // read a batch of the recipe entries from the recipe file
        curClient->_keyRecipeReadHandler.read((char*)sendRecipeBuffer->dataBuffer, 
            sizeof(RecipeEntry_t) * sendRecipeBatchSize_);
        size_t readCnt = curClient->_keyRecipeReadHandler.gcount();
        end = curClient->_keyRecipeReadHandler.eof();
        size_t recipeEntryNum = readCnt / sizeof(RecipeEntry_t);
        if (readCnt == 0) {
            break;
        }
        sendRecipeBuffer->header->currentItemNum = recipeEntryNum;
        sendRecipeBuffer->header->dataSize = readCnt;
        sendRecipeBuffer->header->messageType = CLOUD_SEND_KEY_RECIPE;

        //tool::PrintBinaryArray(sendRecipeBuffer->dataBuffer, 6);
        if (!serverChannel_->SendData(clientSSL, sendRecipeBuffer->sendBuffer, 
        sizeof(NetworkHead_t) + sendRecipeBuffer->header->dataSize)) {
            tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
            exit(EXIT_FAILURE);
        }

        if (!serverChannel_->ReceiveData(clientSSL, sendRecipeBuffer->sendBuffer,
            recvSize)) {
            tool::Logging(myName_.c_str(), "recv the client ready error.\n");
            exit(EXIT_FAILURE);
        } else {
            if (sendRecipeBuffer->header->messageType != EDGE_RECEIVE_READY) {
                tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    sendRecipeBuffer->header->currentItemNum = 0;
    sendRecipeBuffer->header->dataSize = 0;
    sendRecipeBuffer->header->messageType = CLOUD_SEND_RECIPE_END;
    if (!serverChannel_->SendData(clientSSL, sendRecipeBuffer->sendBuffer, sizeof(NetworkHead_t))) {
        tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
        exit(EXIT_FAILURE);
    }

    string clientIP;
    if(!serverChannel_->ReceiveData(clientSSL, sendRecipeBuffer->sendBuffer, recvSize))
    {
        tool::Logging(myName_.c_str(), "send recipes done.\n");
        serverChannel_->GetClientIp(clientIP, clientSSL);
        serverChannel_->ClearAcceptedClientSd(clientSSL);
    }

    return ;
}