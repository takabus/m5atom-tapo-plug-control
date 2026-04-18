#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

class TapoCipher {
public:
    // Hash utilities
    static void sha1(const uint8_t* data, size_t len, uint8_t out[20]);
    static void sha256(const uint8_t* data, size_t len, uint8_t out[32]);
    static void sha256_multi(const uint8_t* parts[], const size_t lengths[], size_t count, uint8_t out[32]);

    // Compute auth_hash = SHA256(SHA1(email) || SHA1(password))
    static void computeAuthHash(const char* email, const char* password, uint8_t out[32]);

    // Initialize cipher from handshake seeds
    void deriveKeys(const uint8_t localSeed[16], const uint8_t remoteSeed[16], const uint8_t authHash[32]);

    // Encrypt plaintext JSON, returns: signature(32) + encrypted_data
    // seq is incremented internally
    std::vector<uint8_t> encrypt(const char* plaintext);

    // Decrypt response (after removing HTTP framing)
    // Input: signature(32) + encrypted_data
    std::vector<uint8_t> decrypt(const uint8_t* data, size_t len);

    int32_t getSeq() const { return seq_; }

private:
    uint8_t key_[16];    // AES-128 key
    uint8_t ivSeed_[12]; // IV prefix (12 bytes)
    uint8_t sig_[28];    // Signature prefix (28 bytes)
    int32_t seq_;        // Sequence number

    void buildIV(int32_t seq, uint8_t iv[16]);
    static void int32ToBE(int32_t val, uint8_t out[4]);
};
