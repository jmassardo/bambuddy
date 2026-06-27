#pragma once

#include <Arduino.h>
#include <SPI.h>

// Bambu MIFARE Classic key derivation constants
extern const uint8_t BAMBU_MASTER_KEY[16];
extern const uint8_t BAMBU_CONTEXT[7];  // "RFID-A\0"

// Blocks to read from Bambu tags
extern const uint8_t BAMBU_BLOCKS[];
extern const size_t BAMBU_BLOCKS_COUNT;

class PN5180 {
public:
    bool begin();
    void close();

    // RF control
    void reset();
    void loadRfConfig(uint8_t tx, uint8_t rx);
    void rfOn();
    void rfOff();
    void setTransceiveMode();

    // ISO 14443A
    struct CardInfo {
        uint8_t uid[7];
        uint8_t uidLen;
        uint8_t sak;
    };
    bool activateTypeA(CardInfo& card);
    bool reactivateCard(CardInfo& card);

    // MIFARE Classic
    bool mfcAuthenticate(uint8_t block, const uint8_t* key, const uint8_t* uid);
    bool mfcReadBlock(uint8_t block, uint8_t* data);  // data must be 16 bytes

    // Bambu tag
    bool readBambuTag(const uint8_t* uid, uint8_t uidLen,
                      uint8_t blocks[][16], size_t* blocksRead);

    // NTAG
    bool ntagReadPages(uint8_t startPage, uint8_t numPages, uint8_t* data, size_t* dataLen);
    bool ntagWritePage(uint8_t page, const uint8_t* data);  // data must be 4 bytes
    bool ntagWritePages(uint8_t startPage, const uint8_t* data, size_t dataLen);

    // Diagnostics
    bool getProductVersion(uint8_t* version, size_t len);
    bool getFirmwareVersion(uint8_t* version, size_t len);

    bool ok() const { return _ok; }

private:
    SPIClass* _spi = nullptr;
    bool _ok = false;

    // Low-level SPI
    void _csLow();
    void _csHigh();
    void _waitBusy(uint32_t timeoutMs = 1000);
    void _cmd(const uint8_t* data, size_t len);
    void _readResponse(uint8_t* buf, size_t len);

    // Register operations
    void _writeReg(uint8_t reg, uint32_t val);
    void _writeRegOr(uint8_t reg, uint32_t mask);
    void _writeRegAnd(uint8_t reg, uint32_t mask);
    uint32_t _readReg(uint8_t reg);
    void _readEeprom(uint8_t addr, uint8_t len, uint8_t* buf);

    // Data transfer
    void _sendData(const uint8_t* data, size_t len, uint8_t validBits = 0x00);
    void _readData(uint8_t* buf, size_t len);
};
