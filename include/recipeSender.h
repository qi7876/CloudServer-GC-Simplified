#ifndef RECIPE_SENDER_H
#define RECIPE_SENDER_H

#include "configure.h"
#include "clientVar.h"
#include "sslConnection.h"

class RecipeSender {
private:
    string myName_ = "RecipeSender";
    SSLConnection* serverChannel_;
    uint64_t sendRecipeBatchSize_;

public:
    RecipeSender(SSLConnection* serverChannel);

    ~RecipeSender();

    void Run(ClientVar* curClient);
};

#endif