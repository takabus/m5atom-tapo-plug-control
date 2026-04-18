#include "tapo_cipher.h"
#include <cstring>

#ifdef ESP8266
// ESP8266: use BearSSL (built into ESP8266 Arduino core)
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_block.h>

void TapoCipher::sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    br_sha1_context ctx;
    br_sha1_init(&ctx);
    br_sha1_update(&ctx, data, len);
    br_sha1_out(&ctx, out);
}

void TapoCipher::sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, data, len);
    br_sha256_out(&ctx, out);
}

void TapoCipher::sha256_multi(const uint8_t* parts[], const size_t lengths[], size_t count, uint8_t out[32]) {
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    for (size_t i = 0; i < count; i++) {
        br_sha256_update(&ctx, parts[i], lengths[i]);
    }
    br_sha256_out(&ctx, out);
}

#else
// ESP32: use mbedtls (built into ESP-IDF)
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

#endif // ESP8266

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
    std::vector<uint8_t> encrypted(paddedLen);
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);

#ifdef ESP8266
    // BearSSL AES-CBC: encrypts in-place, so copy plaintext to output buffer first
    memcpy(encrypted.data(), padded.data(), paddedLen);
    br_aes_ct_cbcenc_keys aesCtx;
    br_aes_ct_cbcenc_init(&aesCtx, key_, 16);
    br_aes_ct_cbcenc_run(&aesCtx, ivCopy, encrypted.data(), paddedLen);
#else
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key_, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, ivCopy, padded.data(), encrypted.data());
    mbedtls_aes_free(&aes);
#endif

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
    std::vector<uint8_t> decrypted(encLen);
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, 16);

#ifdef ESP8266
    // BearSSL AES-CBC: decrypts in-place, so copy ciphertext to output buffer first
    memcpy(decrypted.data(), encrypted, encLen);
    br_aes_ct_cbcdec_keys aesCtx;
    br_aes_ct_cbcdec_init(&aesCtx, key_, 16);
    br_aes_ct_cbcdec_run(&aesCtx, ivCopy, decrypted.data(), encLen);
#else
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key_, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encLen, ivCopy, encrypted, decrypted.data());
    mbedtls_aes_free(&aes);
#endif

    // Remove PKCS7 padding
    if (encLen > 0) {
        uint8_t padVal = decrypted[encLen - 1];
        if (padVal > 0 && padVal <= 16 && padVal <= encLen) {
            decrypted.resize(encLen - padVal);
        }
    }

    return decrypted;
}
