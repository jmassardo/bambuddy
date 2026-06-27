#include "nfc_reader.h"
#include "pn5180.h"

static PN5180 pn5180;

bool NFCReader::begin() {
    _ok = pn5180.begin();
    if (_ok) {
        log_i("NFC reader initialized");
    } else {
        log_w("NFC reader not available");
    }
    return _ok;
}

void NFCReader::close() {
    pn5180.close();
    _ok = false;
}

void NFCReader::_initRf() {
    pn5180.reset();
    pn5180.loadRfConfig(0x00, 0x80);
    delay(10);
    pn5180.rfOn();
    delay(30);
    pn5180.setTransceiveMode();
}

bool NFCReader::_fullReset() {
    _initRf();
    _errorCount = 0;
    log_i("NFC reader recovered after full reset");
    return true;
}

TagEvent NFCReader::poll() {
    TagEvent event;
    event.type = TagEvent::NONE;
    _pollCount++;

    // Periodic status log (every 60s)
    uint32_t now = millis();
    if (now - _lastStatusLog >= 60000) {
        log_i("NFC: state=%d, polls=%d, errors=%d, ok=%d",
               (int)_state, _pollCount, _errorCount, _ok);
        _lastStatusLog = now;
    }

    if (_state == NFCState::IDLE) {
        // Full reset before each idle poll (PN5180 quirk)
        _initRf();
    } else {
        // Tag present: light RF cycle
        pn5180.rfOff();
        delay(3);
        pn5180.rfOn();
        delay(10);
    }

    PN5180::CardInfo card;
    bool found = pn5180.activateTypeA(card);

    if (!found) {
        // Check if this is an error condition
        if (_state == NFCState::TAG_PRESENT) {
            _missCount++;
            if (_missCount >= MISS_THRESHOLD) {
                event.type = TagEvent::TAG_REMOVED;
                strncpy(event.uid, _currentUid, sizeof(event.uid));

                _state = NFCState::IDLE;
                _currentUid[0] = '\0';
                _currentSak = 0;
                _missCount = 0;
                log_i("Tag removed: %s", event.uid);
            }
        }
        return event;
    }

    // Card found — clear errors
    if (_errorCount > 0) {
        log_i("NFC recovered after %d errors", _errorCount);
    }
    _errorCount = 0;
    _ok = true;
    _missCount = 0;

    // Format UID as hex
    char uidHex[15];
    snprintf(uidHex, sizeof(uidHex), "%02X%02X%02X%02X",
             card.uid[0], card.uid[1], card.uid[2], card.uid[3]);

    if (_state == NFCState::IDLE) {
        _state = NFCState::TAG_PRESENT;
        strncpy(_currentUid, uidHex, sizeof(_currentUid));
        _currentSak = card.sak;

        event.type = TagEvent::TAG_DETECTED;
        strncpy(event.uid, uidHex, sizeof(event.uid));
        event.sak = card.sak;

        // Determine tag type
        if (card.sak == 0x08 || card.sak == 0x18) {
            strncpy(event.tagType, "mifare_classic", sizeof(event.tagType));
        } else if (card.sak == 0x00 || card.sak == 0x04) {
            strncpy(event.tagType, "ntag", sizeof(event.tagType));
        } else {
            strncpy(event.tagType, "unknown", sizeof(event.tagType));
        }

        // Try reading Bambu tag data
        event.trayUuid[0] = '\0';
        if (card.sak == 0x08 || card.sak == 0x18) {
            uint8_t blocks[4][16];
            size_t blocksRead = 0;
            if (pn5180.readBambuTag(card.uid, card.uidLen, blocks, &blocksRead)) {
                String uuid = _extractTrayUuid(blocks, blocksRead);
                if (uuid.length() > 0) {
                    strncpy(event.trayUuid, uuid.c_str(), sizeof(event.trayUuid));
                }
            }
        }

        log_i("Tag detected: %s (SAK=0x%02X, type=%s)", uidHex, card.sak, event.tagType);
    }

    return event;
}

bool NFCReader::writeNtag(const uint8_t* data, size_t dataLen, String& message) {
    if (_state != NFCState::TAG_PRESENT) {
        message = "No tag present";
        return false;
    }
    if (_currentSak != 0x00 && _currentSak != 0x04) {
        message = "Not an NTAG (SAK=0x" + String(_currentSak, HEX) + ")";
        return false;
    }

    // Reactivate card
    PN5180::CardInfo card;
    if (!pn5180.reactivateCard(card)) {
        message = "Failed to reactivate card for write";
        return false;
    }

    // Verify UID hasn't changed
    char uidHex[15];
    snprintf(uidHex, sizeof(uidHex), "%02X%02X%02X%02X",
             card.uid[0], card.uid[1], card.uid[2], card.uid[3]);
    if (strcmp(uidHex, _currentUid) != 0) {
        message = "Tag UID changed during write";
        return false;
    }

    if (pn5180.ntagWritePages(4, data, dataLen)) {
        message = "Write successful";
        log_i("NTAG write successful: %d bytes to %s", dataLen, _currentUid);
        return true;
    } else {
        message = "Write or verification failed";
        return false;
    }
}

String NFCReader::_extractTrayUuid(uint8_t blocks[][16], size_t count) {
    if (count < 4) return "";

    // Blocks index 2 and 3 in our array correspond to Bambu blocks 4 and 5
    // (BAMBU_BLOCKS = {1, 2, 4, 5}, so index 2 = block 4, index 3 = block 5)
    uint8_t raw[32];
    memcpy(raw, blocks[2], 16);
    memcpy(raw + 16, blocks[3], 16);

    // Try ASCII hex decode
    char hexChars[33];
    size_t hexLen = 0;
    for (size_t i = 0; i < 32 && hexLen < 32; i++) {
        char c = (char)raw[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            hexChars[hexLen++] = toupper(c);
        }
    }
    hexChars[hexLen] = '\0';

    if (hexLen >= 32) {
        hexChars[32] = '\0';
        // Check not all zeros
        bool allZero = true;
        for (int i = 0; i < 32; i++) {
            if (hexChars[i] != '0') { allZero = false; break; }
        }
        if (!allZero) return String(hexChars);
    }

    // Fallback: raw hex of first 16 bytes
    char fallback[33];
    for (int i = 0; i < 16; i++) {
        sprintf(fallback + i * 2, "%02X", raw[i]);
    }
    fallback[32] = '\0';

    // Check not all zeros
    bool allZero = true;
    for (int i = 0; i < 32; i++) {
        if (fallback[i] != '0') { allZero = false; break; }
    }
    return allZero ? "" : String(fallback);
}
