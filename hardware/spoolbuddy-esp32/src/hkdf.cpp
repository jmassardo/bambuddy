#include "hkdf.h"
#include "pn5180.h"  // For BAMBU_MASTER_KEY, BAMBU_CONTEXT
#include <mbedtls/md.h>

void hkdfDeriveKeys(const uint8_t* uid, size_t uidLen, uint8_t* output, size_t outputLen) {
    // HKDF-Extract: PRK = HMAC-SHA256(salt=BAMBU_MASTER_KEY, IKM=uid)
    uint8_t prk[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mdInfo, 1);  // 1 = HMAC
    mbedtls_md_hmac_starts(&ctx, BAMBU_MASTER_KEY, 16);
    mbedtls_md_hmac_update(&ctx, uid, uidLen);
    mbedtls_md_hmac_finish(&ctx, prk);

    // HKDF-Expand: generate outputLen bytes using context "RFID-A\0"
    size_t generated = 0;
    uint8_t t[32] = {0};
    size_t tLen = 0;
    uint8_t counter = 1;

    while (generated < outputLen) {
        mbedtls_md_hmac_starts(&ctx, prk, 32);
        if (tLen > 0) {
            mbedtls_md_hmac_update(&ctx, t, tLen);
        }
        mbedtls_md_hmac_update(&ctx, BAMBU_CONTEXT, 7);
        mbedtls_md_hmac_update(&ctx, &counter, 1);
        mbedtls_md_hmac_finish(&ctx, t);
        tLen = 32;

        size_t toCopy = min((size_t)32, outputLen - generated);
        memcpy(output + generated, t, toCopy);
        generated += toCopy;
        counter++;
    }

    mbedtls_md_free(&ctx);
}

void getSectorKey(const uint8_t* keys, uint8_t block, uint8_t* sectorKey) {
    uint8_t sector = block / 4;
    memcpy(sectorKey, keys + sector * 6, 6);
}
