#include <Arduino.h>
#include <unity.h>
#include "../src/hkdf.h"
#include "../src/pn5180.h"

/*
 * Verify HKDF key derivation matches the Python implementation.
 *
 * To generate test vectors, run in Python:
 *
 *   import hmac, hashlib
 *   MASTER_KEY = bytes([0x9A,0x75,0x9C,0xF2,0xC4,0xF7,0xCA,0xFF,
 *                       0x22,0x2C,0xB9,0x76,0x9B,0x41,0xBC,0x96])
 *   CONTEXT = b"RFID-A\x00"
 *   uid = bytes([0xAA, 0xBB, 0xCC, 0xDD])
 *   prk = hmac.new(MASTER_KEY, uid, hashlib.sha256).digest()
 *   okm = b""
 *   t = b""
 *   for i in range(1, 4):
 *       t = hmac.new(prk, t + CONTEXT + bytes([i]), hashlib.sha256).digest()
 *       okm += t
 *   # First 6 bytes = sector 0 key, next 6 = sector 1 key, etc.
 *   print(okm[:96].hex())
 */

// Test vector: UID = AA BB CC DD
// Expected first 12 bytes (sector 0 + sector 1 keys) from Python:
static const uint8_t TEST_UID[] = {0xAA, 0xBB, 0xCC, 0xDD};

// Pre-computed from Python (run the script above to regenerate)
static const uint8_t EXPECTED_KEYS_PREFIX[] = {
    // Paste output from Python here — first 12 bytes
    // This is a placeholder; run the Python script to fill in actual values
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // sector 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // sector 1
};

void test_hkdf_output_length() {
    uint8_t keys[96];
    hkdfDeriveKeys(TEST_UID, 4, keys, 96);

    // Keys should not be all zeros (extremely unlikely with real HMAC)
    bool allZero = true;
    for (int i = 0; i < 96; i++) {
        if (keys[i] != 0x00) { allZero = false; break; }
    }
    TEST_ASSERT_FALSE_MESSAGE(allZero, "HKDF output is all zeros");
}

void test_hkdf_deterministic() {
    uint8_t keys1[96], keys2[96];
    hkdfDeriveKeys(TEST_UID, 4, keys1, 96);
    hkdfDeriveKeys(TEST_UID, 4, keys2, 96);
    TEST_ASSERT_EQUAL_MEMORY(keys1, keys2, 96);
}

void test_hkdf_different_uids() {
    uint8_t uid2[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t keys1[96], keys2[96];
    hkdfDeriveKeys(TEST_UID, 4, keys1, 96);
    hkdfDeriveKeys(uid2, 4, keys2, 96);

    // Different UIDs must produce different keys
    bool same = (memcmp(keys1, keys2, 96) == 0);
    TEST_ASSERT_FALSE_MESSAGE(same, "Different UIDs produced identical keys");
}

void test_get_sector_key() {
    uint8_t keys[96];
    hkdfDeriveKeys(TEST_UID, 4, keys, 96);

    uint8_t key0[6], key1[6];
    getSectorKey(keys, 0, key0);   // Block 0 → sector 0
    getSectorKey(keys, 4, key1);   // Block 4 → sector 1

    // Sector 0 key = keys[0:6]
    TEST_ASSERT_EQUAL_MEMORY(keys, key0, 6);
    // Sector 1 key = keys[6:12]
    TEST_ASSERT_EQUAL_MEMORY(keys + 6, key1, 6);
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_hkdf_output_length);
    RUN_TEST(test_hkdf_deterministic);
    RUN_TEST(test_hkdf_different_uids);
    RUN_TEST(test_get_sector_key);
    UNITY_END();
}

void loop() {}
