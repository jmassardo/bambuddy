#pragma once

#include <Arduino.h>

// HKDF-SHA256 key derivation for Bambu MIFARE Classic tags.
// Uses mbedTLS (built into ESP-IDF).

// Derive 96 bytes of MIFARE key material (16 sectors × 6 bytes).
// salt = BAMBU_MASTER_KEY, ikm = tag UID, context = "RFID-A\0"
void hkdfDeriveKeys(const uint8_t* uid, size_t uidLen, uint8_t* output, size_t outputLen);

// Get 6-byte sector key for the sector containing the given block
void getSectorKey(const uint8_t* keys, uint8_t block, uint8_t* sectorKey);
