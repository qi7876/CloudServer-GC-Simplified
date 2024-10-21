/**
 * @file constVar.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define the const variables
 * @version 0.1
 * @date 2020-12-10
 *
 * @copyright Copyright (c) 2020
 *
 */

#ifndef CONST_VAR_H
#define CONST_VAR_H

// for enclave lib path
static const char ENCLAVE_PATH[] = "../lib/storeEnclave.signed.so";
// #include <bits/stdint-uintn.h>

// For out-enclave breakdown
#define CHUNKING_BREAKDOWN 0
#define DATAWRITER_BREAKDOWN 0
#define RESTORE_WRITER_BREAKDOWN 0

// For SGX_Breakdown
#define SGX_BREAKDOWN 0
#define IMPACT_OF_TOP_K 0
#define MULTI_CLIENT 1

// the length of the hash
enum ENCRYPT_SET { AES_256_GCM = 0,
    AES_128_GCM = 1,
    AES_256_CFB = 2,
    AES_128_CFB = 3 };
enum HASH_SET { SHA_256 = 0,
    MD5 = 1,
    SHA_1 = 2 };
static const uint32_t CRYPTO_BLOCK_SIZE = 16;
static const uint32_t CHUNK_HASH_SIZE = 32;
static const uint32_t MLE_KEY_SIZE = 32;
static const uint32_t CHUNK_ENCRYPT_KEY_SIZE = 32;
static const uint32_t HASH_TYPE = SHA_256;
static const uint32_t CIPHER_TYPE = AES_256_GCM;

// the size of chunk
static const uint32_t MIN_CHUNK_SIZE = 4096;
static const uint32_t AVG_CHUNK_SIZE = 8192;
static const uint32_t MAX_CHUNK_SIZE = 16384;

// the size of segment
static const uint32_t AVG_SEGMENT_SIZE = 1024 * 1024 * 10; // 10MB default
static const uint32_t MIN_SEGMENT_SIZE = AVG_SEGMENT_SIZE / 2; // 5MB default
static const uint32_t MAX_SEGMENT_SIZE = AVG_SEGMENT_SIZE * 2; // 20MB default
static const uint32_t DIVISOR = ((AVG_SEGMENT_SIZE - MIN_SEGMENT_SIZE) / AVG_CHUNK_SIZE);
static const uint32_t PATTERN = 1;

// the type of chunker
enum CHUNKER_TYPE { FIXED_SIZE_CHUNKING = 0,
    FAST_CDC,
    FSL_TRACE,
    UBC_TRACE };

// the setting of the container
static const uint32_t MAX_CONTAINER_SIZE = 1 << 22; // container size: 4MB
static const uint32_t CONTAINER_ID_LENGTH = 8;
static const uint32_t SEGMENT_ID_LENGTH = 16;

// define the data type of the MQ
enum DATA_TYPE_SET { DATA_CHUNK = 0,
    RECIPE_END,
    DATA_SEGMENT_END_FLAG };

// configure for sparse index
static const uint32_t SPARSE_SAMPLE_RATE = 6;
static const uint32_t SPARSE_CHAMPION_NUM = 10;
static const uint32_t SPARSE_MANIFIEST_CAP_NUM = 10;

enum INDEX_TYPE_SET { OUT_ENCLAVE = 0,
    IN_ENCLAVE,
    EXTREME_BIN,
    SPARSE_INDEX,
    FREQ_INDEX };

enum SSL_CONNECTION_TYPE { IN_SERVERSIDE = 0,
    IN_CLIENTSIDE };

// for SSL connection
static const char SERVER_CERT[] = "../key/server/server.crt";
static const char SERVER_KEY[] = "../key/server/server.key";
static const char CLIENT_CERT[] = "../key/client/client.crt";
static const char CLIENT_KEY[] = "../key/client/client.key";
static const char CA_CERT[] = "../key/ca/ca.crt";

static const char KEYMANGER_PRIVATE_KEY[] = "../key/RSAkey/server.key";
static const char KEYMANGER_PUBLIC_KEY_FILE[] = "../key/RSAkey/serverpub.key";
static const uint32_t RSA_KEY_SIZE = 128;
static const uint32_t SHORT_HASH_SIZE = 16;

// for network message protocol code
enum PROTCOL_CODE_SET {
    EDGE_UPLOAD_CHUNK = 0,
    EDGE_UPLOAD_CHUNK_END,
    CLOUD_QUERY_RETURN,
    CLIENT_RESTORE_READY,
    SERVER_RESTORE_CHUNK,
    SERVER_RESTORE_RECIPE,
    SERVER_RESTORE_FINAL,
    CLIENT_KEY_GEN,
    KEY_MANAGER_KEY_GEN_REPLY
};

enum EDGE_PROTCOL_CODE_SET {
    // for both
    EDGE_LOGIN_RESPONSE = 0,
    CLOUD_FILE_NON_EXIST,

    // for upload
    EDGE_MIGRATE_LOGIN,
    EDGE_UPLOAD_SEC_RECIPE,
    EDGE_UPLOAD_SEC_RECIPE_END,
    EDGE_UPLOAD_RECIPE,
    EDGE_UPLOAD_RECIPE_END,
    EDGE_UPLOAD_KEY_RECIPE,
    EDGE_UPLOAD_KEY_RECIPE_END,
    EDGE_MIGRATION_CHUNK,
    EDGE_MIGRATION_CHUNK_FINAL,
    EDGE_MIGRATION_NEWFILE,

    // for download recipe
    EDGE_DOWNLOAD_RECIPE_LOGIN,
    EDGE_DOWNLOAD_RECIPE_READY,
    CLOUD_SEND_RECIPE,
    CLOUD_SEND_SECURE_RECIPE,
    CLOUD_SEND_KEY_RECIPE,
    EDGE_RECEIVE_READY,
    CLOUD_SEND_RECIPE_END,

    // for download chunk
    EDGE_DOWNLOAD_CHUNK_LOGIN,
    EDGE_DOWNLOAD_CHUNK_READY,
    CLOUD_SEND_CHUNK,
    CLOUD_SEND_CHUNK_END
};

static const uint32_t CHUNK_QUEUE_SIZE = 8192;
static const uint32_t CONTAINER_QUEUE_SIZE = 32;
static const uint32_t CONTAINER_CAPPING_VALUE = 16;

static const uint32_t SGX_PERSISTENCE_BUFFER_SIZE = 2 * 1024 * 1024;

enum TWO_PATH_STATUS { UNIQUE = 0,
    TMP_UNIQUE = 1,
    DUPLICATE = 2,
    TMP_DUPLICATE = 3 };

enum ENCLAVE_TRUST_STATUS { ENCLAVE_TRUSTED = 0,
    ENCLAVE_UNTRUSTED = 1 };

static const uint32_t MAX_SGX_MESSAGE_SIZE = 4 * 1024;

#define ENABLE_SGX_RA 0
#define TEST_IN_CSE 0

static const uint32_t THREAD_STACK_SIZE = 8 * 1024 * 1024;
static const uint32_t SESSION_KEY_BUFFER_SIZE = 65;

enum OPT_TYPE { UPLOAD_OPT = 0,
    DOWNLOAD_RECIPE_OPT,
    DOWNLOAD_CHUNK_OPT };

enum LOCK_TYPE { SESSION_LCK_WRITE = 0,
    SESSION_LCK_READ,
    TOP_K_LCK_WRITE,
    TOP_K_LCK_READ };

enum DAE_TYPE { PLAIN_DAE = 0,
    CE_DAE,
    TED_DAE,
    DUPLESS_DAE };

#endif