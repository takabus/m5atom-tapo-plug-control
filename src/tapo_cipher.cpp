#include "tapo_cipher.h"
#include <cstring>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>

void TapoCipher::sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    mbedtls_sha1(data, len, out);
}

void TapoCipher::sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    mbedtls_sha256(data, len, out, 0);
}

void TapoCipher::sha256_multi(const uint8_t* parts[], const size_t lengths[], size_t count, uint8_t out[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    for (size_t i = 0; i < count; i++) {
        mbedtls_sha256_update(&ctx, parts[i], lengths[i]);
    }
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

void TapoCipher::computeAuthHash(const char* email, const char* password, uint8_t out[32]) {
    uint8_t emailSha1[20];
    uint8_t passSha1[20];
    sha1((const uint8_t*)email, strlen(email), emailSha1);
    sha1((const uint8_t*)password, strlen(password), passSha1);

    // auth_hash = SHA256(SHA1(email) || SHA1(password))
    uint8_t combined[40];
    memcpy(combined, emailSha1, 20);
    memcpy(combined + 20, passSha1, 20);
    sha256(combined, 40, out);
}

void TapoCipher::int32ToBE(int32_t val, uint8_t out[4]) {
    out[0] = (val >> 24) & 0xFF;
    out[1] = (val >> 16) & 0xFF;
    out[2] = (val >> 8) & 0xFF;
    out[3] = val & 0xFF;
}

void TapoCipher::buildIV(int32_t seq, uint8_t iv[16]) {
    memcpy(iv, ivSeed_, 12);
    int32ToBE(seq, iv + 12);
}

void TapoCipher::deriveKeys(const uint8_t localSeed[16], const uint8_t remoteSeed[16], const uint8_t authHash[32]) {
    // key = SHA256("lsk" + local_seed + remote_seed + auth_hash)[0:16]
    {
        const char* prefix = "lsk";
        const uint8_t* parts[] = {(const uint8_t*)prefix, localSeed, remoteSeed, authHash};
        const size_t lengths[] = {3, 16, 16, 32};
        uint8_t hash[32];
        sha256_multi(parts, lengths, 4, hash);
        memcpy(key_, hash, 16);
    }

    // iv_seed = SHA256("iv" + local_seed + remote_seed + auth_hash)[0:12]
    // seq = SHA256("iv" + ...)[12:16] as int32 big-endian
    {
        const char* prefix = "iv";
        const uint8_t* parts[] = {(const uint8_t*)prefix, localSeed, remoteSeed, authHash};
        const size_t lengths[] = {2, 16, 16, 32};
        uint8_t hash[32];
        sha256_multi(parts, lengths, 4, hash);
        memcpy(ivSeed_, hash, 12);
        seq_ = ((int32_t)hash[12] << 24) | ((int32_t)hash[13] << 16) |
               ((int32_t)hash[14] << 8) | (int32_t)hash[15];
    }

    // sig = SHA256("ldk" + local_seed + remote_seed + auth_hash)[0:28]
    {
        const char* prefix = "ldk";
        const uint8_t* parts[] = {(const uint8_t*)prefix, localSeed, remoteSeed, authHash};
        const size_t lengths[] = {3, 16, 16, 32};
        uint8_t hash[32];
        sha256_multi(parts, lengths, 4, hash);
        memcpy(sig_, hash, 28);
    }
}

std::vector<uint8_t> TapoCipher::encrypt(const char* plaintext) {
    seq_++;

    size_t ptLen = strlen(plaintext);
    // PKCS7 padding
    size_t padLen = 16 - (ptLen % 16);
    size_t paddedLen = ptLen + padLen;
    std::vector<uint8_t> padded(paddedLen);
    memcpy(padded.data(), plaintext, ptLen);
    memset(padded.data() + ptLen, (uint8_t)padLen, padLen);

    // Build IV
    uint8_t iv[16];
    buildIV(seq_, iv);

    // AES-128-CBC encrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key_, 128);
    std::vector<uint8_t> encrypted(paddedLen);
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, ivCopy, padded.data(), encrypted.data());
    mbedtls_aes_free(&aes);

    // Signature = SHA256(sig_key + seq_bytes + encrypted_data)
    uint8_t seqBytes[4];
    int32ToBE(seq_, seqBytes);

    const uint8_t* sigParts[] = {sig_, seqBytes, encrypted.data()};
    const size_t sigLengths[] = {28, 4, paddedLen};
    uint8_t signature[32];
    sha256_multi(sigParts, sigLengths, 3, signature);

    // Output: signature(32) + encrypted_data
    std::vector<uint8_t> result(32 + paddedLen);
    memcpy(result.data(), signature, 32);
    memcpy(result.data() + 32, encrypted.data(), paddedLen);
    return result;
}

std::vector<uint8_t> TapoCipher::decrypt(const uint8_t* data, size_t len) {
    if (len < 32) return {};

    const uint8_t* encrypted = data + 32;
    size_t encLen = len - 32;

    // Build IV with current seq
    uint8_t iv[16];
    buildIV(seq_, iv);

    // AES-128-CBC decrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key_, 128);
    std::vector<uint8_t> decrypted(encLen);
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encLen, ivCopy, encrypted, decrypted.data());
    mbedtls_aes_free(&aes);

    // Remove PKCS7 padding
    if (encLen > 0) {
        uint8_t padVal = decrypted[encLen - 1];
        if (padVal > 0 && padVal <= 16 && padVal <= encLen) {
            decrypted.resize(encLen - padVal);
        }
    }

    return decrypted;
}
