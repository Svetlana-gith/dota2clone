/*
 * Simple bcrypt implementation using Windows BCrypt API
 * This is a simplified version for the authentication system
 */

#include "bcrypt_hash.h"
#include <windows.h>
#include <bcrypt.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "bcrypt.lib")

#define BCRYPT_HASHSIZE 24
#define BCRYPT_SALTSIZE 16
#define BCRYPT_HASHSTR_SIZE 61

// Base64 encoding table for bcrypt
static const char bcrypt_base64[] = 
    "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

static void encode_base64(char *dst, const unsigned char *src, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned int c1 = src[i++];
        unsigned int c2 = (i < len) ? src[i++] : 0;
        unsigned int c3 = (i < len) ? src[i++] : 0;

        dst[0] = bcrypt_base64[c1 >> 2];
        dst[1] = bcrypt_base64[((c1 & 0x03) << 4) | (c2 >> 4)];
        dst[2] = bcrypt_base64[((c2 & 0x0f) << 2) | (c3 >> 6)];
        dst[3] = bcrypt_base64[c3 & 0x3f];
        dst += 4;
    }
}

static int decode_base64(unsigned char *dst, const char *src, size_t len) {
    size_t i, j;
    unsigned char decode_table[256];
    
    memset(decode_table, 0xFF, sizeof(decode_table));
    for (i = 0; i < sizeof(bcrypt_base64) - 1; i++) {
        decode_table[(unsigned char)bcrypt_base64[i]] = (unsigned char)i;
    }

    for (i = 0, j = 0; i < len; i += 4, j += 3) {
        unsigned char c1 = decode_table[(unsigned char)src[i]];
        unsigned char c2 = decode_table[(unsigned char)src[i + 1]];
        unsigned char c3 = decode_table[(unsigned char)src[i + 2]];
        unsigned char c4 = decode_table[(unsigned char)src[i + 3]];

        if (c1 == 0xFF || c2 == 0xFF || c3 == 0xFF || c4 == 0xFF)
            return -1;

        dst[j] = (c1 << 2) | (c2 >> 4);
        if (j + 1 < len)
            dst[j + 1] = (c2 << 4) | (c3 >> 2);
        if (j + 2 < len)
            dst[j + 2] = (c3 << 6) | c4;
    }

    return 0;
}

// Simple PBKDF2-like key derivation using Windows BCrypt
static int derive_key(const char *password, const unsigned char *salt, 
                     int cost, unsigned char *output, size_t output_len) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    NTSTATUS status;
    
    // Open SHA256 algorithm
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status))
        return -1;

    // Perform iterations (2^cost)
    unsigned int iterations = 1 << cost;
    unsigned char buffer[32]; // SHA256 output
    
    // Initial hash: password + salt
    BCRYPT_HASH_HANDLE hHash = NULL;
    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    BCryptHashData(hHash, (PUCHAR)password, (ULONG)strlen(password), 0);
    BCryptHashData(hHash, (PUCHAR)salt, BCRYPT_SALTSIZE, 0);
    
    DWORD hashLen = sizeof(buffer);
    BCryptFinishHash(hHash, buffer, hashLen, 0);
    BCryptDestroyHash(hHash);

    // Iterate
    for (unsigned int i = 1; i < iterations; i++) {
        status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
        if (!BCRYPT_SUCCESS(status)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return -1;
        }

        BCryptHashData(hHash, buffer, hashLen, 0);
        BCryptFinishHash(hHash, buffer, hashLen, 0);
        BCryptDestroyHash(hHash);
    }

    // Copy output
    memcpy(output, buffer, (output_len < hashLen) ? output_len : hashLen);

    BCryptCloseAlgorithmProvider(hAlg, 0);
    return 0;
}

int bcrypt_hashpw(const char *password, int cost, char *hash) {
    if (!password || !hash || cost < 4 || cost > 31)
        return -1;

    // Generate random salt
    unsigned char salt[BCRYPT_SALTSIZE];
    NTSTATUS status = BCryptGenRandom(NULL, salt, BCRYPT_SALTSIZE, 
                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status))
        return -1;

    // Derive key
    unsigned char derived[BCRYPT_HASHSIZE];
    if (derive_key(password, salt, cost, derived, BCRYPT_HASHSIZE) != 0)
        return -1;

    // Format: $2b$<cost>$<salt><hash>
    sprintf(hash, "$2b$%02d$", cost);
    
    // Encode salt (22 chars)
    char *p = hash + 7;
    encode_base64(p, salt, BCRYPT_SALTSIZE);
    p[22] = '\0';
    
    // Encode hash (31 chars)
    p = hash + 29;
    encode_base64(p, derived, BCRYPT_HASHSIZE);
    p[31] = '\0';

    return 0;
}

int bcrypt_checkpw(const char *password, const char *hash) {
    if (!password || !hash)
        return -1;

    // Parse hash format: $2b$<cost>$<salt><hash>
    if (strncmp(hash, "$2b$", 4) != 0)
        return -1;

    int cost;
    if (sscanf(hash + 4, "%02d$", &cost) != 1)
        return -1;

    if (cost < 4 || cost > 31)
        return -1;

    // Extract salt (22 base64 chars -> 16 bytes, but decode needs more space)
    const char *salt_str = hash + 7;
    unsigned char salt[BCRYPT_SALTSIZE + 4]; // Extra space for decode_base64
    memset(salt, 0, sizeof(salt));
    if (decode_base64(salt, salt_str, 22) != 0)
        return -1;

    // Derive key with same salt
    unsigned char derived[BCRYPT_HASHSIZE];
    if (derive_key(password, salt, cost, derived, BCRYPT_HASHSIZE) != 0)
        return -1;

    // Encode derived hash (24 bytes -> 32 base64 chars)
    char derived_str[36]; // Extra space for safety
    memset(derived_str, 0, sizeof(derived_str));
    encode_base64(derived_str, derived, BCRYPT_HASHSIZE);
    derived_str[31] = '\0';

    // Compare with stored hash
    const char *stored_hash = hash + 29;
    if (strncmp(derived_str, stored_hash, 31) == 0)
        return 0;

    return -1;
}
