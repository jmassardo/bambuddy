#pragma once

#include <Arduino.h>

enum class NFCState {
    IDLE,
    TAG_PRESENT
};

struct TagEvent {
    enum Type { NONE, TAG_DETECTED, TAG_REMOVED };

    Type type = NONE;
    char uid[15];    // Hex string (4 bytes = 8 chars + null)
    uint8_t sak = 0;
    char tagType[16];  // "mifare_classic", "ntag", "unknown"
    char trayUuid[33]; // 32 hex chars + null
};

class NFCReader {
public:
    bool begin();
    void close();

    TagEvent poll();

    bool writeNtag(const uint8_t* data, size_t dataLen, String& message);

    bool ok() const { return _ok; }
    NFCState state() const { return _state; }
    const char* currentUid() const { return _currentUid; }
    uint8_t currentSak() const { return _currentSak; }
    const char* readerType() const { return "PN5180"; }
    const char* connection() const { return "SPI"; }

private:
    bool _ok = false;
    NFCState _state = NFCState::IDLE;
    char _currentUid[15] = {0};
    uint8_t _currentSak = 0;
    uint8_t _missCount = 0;
    uint16_t _errorCount = 0;
    uint32_t _pollCount = 0;
    uint32_t _lastStatusLog = 0;

    static const uint8_t MISS_THRESHOLD = 3;
    static const uint16_t ERROR_RECOVERY_THRESHOLD = 10;

    void _initRf();
    bool _fullReset();
    String _extractTrayUuid(uint8_t blocks[][16], size_t count);
};
