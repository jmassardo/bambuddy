#include "pn5180.h"
#include "pins.h"
#include "hkdf.h"

const uint8_t BAMBU_MASTER_KEY[16] = {
    0x9A, 0x75, 0x9C, 0xF2, 0xC4, 0xF7, 0xCA, 0xFF,
    0x22, 0x2C, 0xB9, 0x76, 0x9B, 0x41, 0xBC, 0x96
};
const uint8_t BAMBU_CONTEXT[7] = {'R', 'F', 'I', 'D', '-', 'A', '\0'};
const uint8_t BAMBU_BLOCKS[] = {1, 2, 4, 5};
const size_t BAMBU_BLOCKS_COUNT = 4;

// --- Init / Close ---

bool PN5180::begin() {
    pinMode(PIN_NFC_NSS, OUTPUT);
    pinMode(PIN_NFC_RST, OUTPUT);
    pinMode(PIN_NFC_BUSY, INPUT);

    digitalWrite(PIN_NFC_NSS, HIGH);
    digitalWrite(PIN_NFC_RST, HIGH);

    _spi = new SPIClass(HSPI);
    _spi->begin(PIN_NFC_SCK, PIN_NFC_MISO, PIN_NFC_MOSI, -1);  // -1 = no HW CS

    reset();
    loadRfConfig(0x00, 0x80);
    delay(10);
    rfOn();
    delay(30);
    setTransceiveMode();

    // Verify communication by reading product version
    uint8_t ver[2];
    if (getProductVersion(ver, 2)) {
        log_i("PN5180 initialized (product v%d.%d)", ver[0], ver[1]);
        _ok = true;
    } else {
        log_e("PN5180 communication failed");
        _ok = false;
    }

    return _ok;
}

void PN5180::close() {
    rfOff();
    if (_spi) {
        _spi->end();
        delete _spi;
        _spi = nullptr;
    }
    _ok = false;
}

// --- Low-level SPI ---

void PN5180::_csLow() {
    digitalWrite(PIN_NFC_NSS, LOW);
    delayMicroseconds(5);  // 5µs setup time
}

void PN5180::_csHigh() {
    digitalWrite(PIN_NFC_NSS, HIGH);
    delayMicroseconds(100);  // 100µs post-CS delay
}

void PN5180::_waitBusy(uint32_t timeoutMs) {
    // Wait for BUSY HIGH (processing started)
    uint32_t deadline = millis() + 10;
    while (digitalRead(PIN_NFC_BUSY) == LOW) {
        if (millis() > deadline) break;
        delayMicroseconds(10);
    }
    // Wait for BUSY LOW (processing complete)
    deadline = millis() + timeoutMs;
    while (digitalRead(PIN_NFC_BUSY) == HIGH) {
        if (millis() > deadline) {
            log_e("PN5180 BUSY timeout");
            return;
        }
        delayMicroseconds(100);
    }
}

void PN5180::_cmd(const uint8_t* data, size_t len) {
    _spi->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    _csLow();
    _spi->transferBytes(data, nullptr, len);
    _csHigh();
    _spi->endTransaction();
    _waitBusy();
}

void PN5180::_readResponse(uint8_t* buf, size_t len) {
    _spi->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    _csLow();
    memset(buf, 0xFF, len);
    _spi->transferBytes(buf, buf, len);
    _csHigh();
    _spi->endTransaction();
}

// --- Register Operations ---

void PN5180::_writeReg(uint8_t reg, uint32_t val) {
    uint8_t cmd[6] = {0x00, reg,
        (uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF),
        (uint8_t)((val >> 16) & 0xFF), (uint8_t)((val >> 24) & 0xFF)};
    _cmd(cmd, 6);
}

void PN5180::_writeRegOr(uint8_t reg, uint32_t mask) {
    uint8_t cmd[6] = {0x01, reg,
        (uint8_t)(mask & 0xFF), (uint8_t)((mask >> 8) & 0xFF),
        (uint8_t)((mask >> 16) & 0xFF), (uint8_t)((mask >> 24) & 0xFF)};
    _cmd(cmd, 6);
}

void PN5180::_writeRegAnd(uint8_t reg, uint32_t mask) {
    uint8_t cmd[6] = {0x02, reg,
        (uint8_t)(mask & 0xFF), (uint8_t)((mask >> 8) & 0xFF),
        (uint8_t)((mask >> 16) & 0xFF), (uint8_t)((mask >> 24) & 0xFF)};
    _cmd(cmd, 6);
}

uint32_t PN5180::_readReg(uint8_t reg) {
    uint8_t cmd[2] = {0x04, reg};
    _cmd(cmd, 2);
    delayMicroseconds(100);
    uint8_t buf[4];
    _readResponse(buf, 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

void PN5180::_readEeprom(uint8_t addr, uint8_t len, uint8_t* buf) {
    uint8_t cmd[3] = {0x07, addr, len};
    _cmd(cmd, 3);
    delayMicroseconds(100);
    _readResponse(buf, len);
}

// --- Commands ---

void PN5180::reset() {
    digitalWrite(PIN_NFC_RST, LOW);
    delay(50);
    digitalWrite(PIN_NFC_RST, HIGH);
    delay(100);
    _waitBusy(2000);
    delay(50);
}

void PN5180::loadRfConfig(uint8_t tx, uint8_t rx) {
    _writeReg(0x03, 0xFFFFFFFF);  // Clear IRQs
    delayMicroseconds(100);
    uint8_t cmd[3] = {0x11, tx, rx};
    _cmd(cmd, 3);
    delay(10);
}

void PN5180::rfOn() {
    uint8_t cmd[2] = {0x16, 0x00};
    _cmd(cmd, 2);
    delay(10);
}

void PN5180::rfOff() {
    uint8_t cmd[2] = {0x17, 0x00};
    _cmd(cmd, 2);
    delay(5);
}

void PN5180::setTransceiveMode() {
    uint32_t sysCfg = _readReg(0x00);
    sysCfg = (sysCfg & 0xFFFFFFF8) | 0x03;
    _writeReg(0x00, sysCfg);
}

void PN5180::_sendData(const uint8_t* data, size_t len, uint8_t validBits) {
    uint8_t* cmd = (uint8_t*)malloc(len + 2);
    cmd[0] = 0x09;
    cmd[1] = validBits;
    memcpy(cmd + 2, data, len);

    _spi->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    _csLow();
    _spi->transferBytes(cmd, nullptr, len + 2);
    _csHigh();
    _spi->endTransaction();

    free(cmd);
    delayMicroseconds(100);
    _waitBusy();
}

void PN5180::_readData(uint8_t* buf, size_t len) {
    uint8_t cmd[2] = {0x0A, 0x00};
    _cmd(cmd, 2);
    _readResponse(buf, len);
}

// --- ISO 14443A ---

bool PN5180::activateTypeA(CardInfo& card) {
    // Crypto off, CRC off
    _writeRegAnd(0x00, 0xFFFFFFBF);
    _writeRegAnd(0x12, 0xFFFFFFFE);
    _writeRegAnd(0x19, 0xFFFFFFFE);
    _writeReg(0x03, 0xFFFFFFFF);

    // Reset to IDLE then TRANSCEIVE
    uint32_t sysCfg = _readReg(0x00);
    _writeReg(0x00, sysCfg & 0xFFFFFFF8);  // IDLE
    delay(1);
    _writeReg(0x00, (sysCfg & 0xFFFFFFF8) | 0x03);  // TRANSCEIVE
    delay(2);

    // WUPA (7-bit)
    uint8_t wupa = 0x52;
    _sendData(&wupa, 1, 0x07);
    delay(5);

    uint32_t rxStatus = _readReg(0x13);
    uint16_t rxLen = rxStatus & 0x1FF;

    if (rxLen < 2 || rxLen == 511) {
        // Try REQA
        _writeReg(0x03, 0xFFFFFFFF);
        delay(2);
        setTransceiveMode();
        delay(2);
        uint8_t reqa = 0x26;
        _sendData(&reqa, 1, 0x07);
        delay(5);
        rxStatus = _readReg(0x13);
        rxLen = rxStatus & 0x1FF;
        if (rxLen < 2 || rxLen == 511) return false;
    }

    uint8_t atqa[2];
    _readData(atqa, 2);
    if (atqa[0] == 0xFF || atqa[0] == 0x00) return false;

    // Anti-collision Level 1
    _writeReg(0x03, 0xFFFFFFFF);
    setTransceiveMode();
    delay(2);

    uint8_t anticol[2] = {0x93, 0x20};
    _sendData(anticol, 2);
    delay(10);

    rxStatus = _readReg(0x13);
    rxLen = rxStatus & 0x1FF;
    if (rxLen < 5 || rxLen > 64) return false;

    uint8_t uidBuf[5];
    _readData(uidBuf, 5);

    uint8_t bcc = uidBuf[0] ^ uidBuf[1] ^ uidBuf[2] ^ uidBuf[3];
    if (bcc != uidBuf[4]) return false;

    // SELECT
    _writeReg(0x03, 0xFFFFFFFF);
    setTransceiveMode();
    delay(2);

    // Enable CRC for SELECT
    _writeRegOr(0x19, 0x01);
    _writeRegOr(0x12, 0x01);

    uint8_t sel[7] = {0x93, 0x70, uidBuf[0], uidBuf[1], uidBuf[2], uidBuf[3], uidBuf[4]};
    _sendData(sel, 7);
    delay(10);

    rxStatus = _readReg(0x13);
    rxLen = rxStatus & 0x1FF;
    if (rxLen < 1) return false;

    uint8_t sakBuf[3];
    _readData(sakBuf, min((uint16_t)3, rxLen));

    // Fill card info
    memcpy(card.uid, uidBuf, 4);
    card.uidLen = 4;
    card.sak = sakBuf[0];

    return true;
}

bool PN5180::reactivateCard(CardInfo& card) {
    rfOff();
    delay(10);
    _writeReg(0x03, 0xFFFFFFFF);
    loadRfConfig(0x00, 0x80);
    delay(5);
    rfOn();
    delay(20);
    return activateTypeA(card);
}

// --- MIFARE Classic ---

bool PN5180::mfcAuthenticate(uint8_t block, const uint8_t* key, const uint8_t* uid) {
    // Wait for BUSY LOW
    uint32_t deadline = millis() + 100;
    while (digitalRead(PIN_NFC_BUSY) == HIGH) {
        if (millis() > deadline) return false;
        delay(1);
    }

    // MFC_AUTHENTICATE: [0x0C][key 6B][keyType 0x60][blockNo][uid 4B] = 13 bytes
    uint8_t cmd[13];
    cmd[0] = 0x0C;
    memcpy(cmd + 1, key, 6);
    cmd[7] = 0x60;  // Key A
    cmd[8] = block;
    memcpy(cmd + 9, uid, 4);

    _spi->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    _csLow();
    _spi->transferBytes(cmd, nullptr, 13);
    _csHigh();
    _spi->endTransaction();

    _waitBusy(1000);

    // Read 1-byte response: 0x00 = success
    uint8_t response;
    _spi->beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));
    _csLow();
    response = _spi->transfer(0xFF);
    _csHigh();
    _spi->endTransaction();

    return response == 0x00;
}

bool PN5180::mfcReadBlock(uint8_t block, uint8_t* data) {
    _writeReg(0x03, 0xFFFFFFFF);
    setTransceiveMode();
    delay(1);

    // Enable TX and RX CRC
    _writeRegOr(0x19, 0x01);
    _writeRegOr(0x12, 0x01);

    // MIFARE READ: 0x30 + block
    uint8_t readCmd[2] = {0x30, block};
    _sendData(readCmd, 2);
    delay(10);

    uint32_t rxStatus = _readReg(0x13);
    uint16_t rxLen = rxStatus & 0x1FF;
    if (rxLen != 16) return false;

    _readData(data, 16);
    return true;
}

bool PN5180::readBambuTag(const uint8_t* uid, uint8_t uidLen,
                          uint8_t blocks[][16], size_t* blocksRead) {
    *blocksRead = 0;

    // Derive per-sector keys
    uint8_t keys[96];
    hkdfDeriveKeys(uid, uidLen, keys, 96);

    // Clear Crypto1 and IRQs
    _writeRegAnd(0x00, 0xFFFFFFBF);
    _writeReg(0x03, 0xFFFFFFFF);

    // Reactivate card
    CardInfo card;
    if (!reactivateCard(card)) {
        log_d("Failed to reactivate card for Bambu read");
        return false;
    }

    if (memcmp(card.uid, uid, 4) != 0) {
        log_d("UID mismatch after reactivation");
        return false;
    }

    int8_t currentSector = -1;

    for (size_t i = 0; i < BAMBU_BLOCKS_COUNT; i++) {
        uint8_t block = BAMBU_BLOCKS[i];
        uint8_t sector = block / 4;

        if (sector != currentSector) {
            // Get key for this sector
            uint8_t sectorKey[6];
            memcpy(sectorKey, keys + sector * 6, 6);

            if (!mfcAuthenticate(block, sectorKey, uid)) {
                log_d("Auth failed for block %d (sector %d)", block, sector);
                return false;
            }
            currentSector = sector;
        }

        if (!mfcReadBlock(block, blocks[i])) {
            log_d("Read failed for block %d", block);
            return false;
        }
        (*blocksRead)++;
    }

    return true;
}

// --- NTAG ---

bool PN5180::ntagReadPages(uint8_t startPage, uint8_t numPages, uint8_t* data, size_t* dataLen) {
    *dataLen = 0;

    // Crypto1 off, TX CRC on, RX CRC off
    _writeRegAnd(0x00, 0xFFFFFFBF);
    _writeRegOr(0x19, 0x01);
    _writeRegAnd(0x12, 0xFFFFFFFE);
    _writeReg(0x03, 0xFFFFFFFF);

    uint32_t sysCfg = _readReg(0x00);
    _writeReg(0x00, sysCfg & 0xFFFFFFF8);  // IDLE
    delay(1);
    _writeReg(0x00, (sysCfg & 0xFFFFFFF8) | 0x03);  // TRANSCEIVE
    delay(2);

    uint8_t pagesRead = 0;
    while (pagesRead < numPages) {
        if (pagesRead > 0) {
            _writeReg(0x03, 0xFFFFFFFF);
            setTransceiveMode();
            delay(1);
        }

        // READ: 0x30 + page -> returns 16 bytes (4 pages)
        uint8_t readCmd[2] = {0x30, (uint8_t)(startPage + pagesRead)};
        _sendData(readCmd, 2);
        delay(10);

        uint32_t rxStatus = _readReg(0x13);
        uint16_t rxLen = rxStatus & 0x1FF;
        if (rxLen < 16) {
            log_w("NTAG read page %d: rxLen=%d", startPage + pagesRead, rxLen);
            return false;
        }

        uint8_t buf[16];
        _readData(buf, 16);

        uint8_t pagesToCopy = min((uint8_t)4, (uint8_t)(numPages - pagesRead));
        memcpy(data + *dataLen, buf, pagesToCopy * 4);
        *dataLen += pagesToCopy * 4;
        pagesRead += 4;
    }

    return true;
}

bool PN5180::ntagWritePage(uint8_t page, const uint8_t* data) {
    // Crypto1 off, TX CRC on, RX CRC off
    _writeRegAnd(0x00, 0xFFFFFFBF);
    _writeRegOr(0x19, 0x01);
    _writeRegAnd(0x12, 0xFFFFFFFE);
    _writeReg(0x03, 0xFFFFFFFF);

    uint32_t sysCfg = _readReg(0x00);
    _writeReg(0x00, sysCfg & 0xFFFFFFF8);  // IDLE
    delay(1);
    _writeReg(0x00, (sysCfg & 0xFFFFFFF8) | 0x03);  // TRANSCEIVE
    delay(2);

    // WRITE: 0xA2 + page + 4 bytes
    uint8_t cmd[6] = {0xA2, page, data[0], data[1], data[2], data[3]};
    _sendData(cmd, 6);
    delay(10);

    return true;  // ACK is 4-bit, can't be captured by PN5180
}

bool PN5180::ntagWritePages(uint8_t startPage, const uint8_t* data, size_t dataLen) {
    // Pad to 4-byte boundary
    size_t paddedLen = ((dataLen + 3) / 4) * 4;
    uint8_t* padded = (uint8_t*)calloc(paddedLen, 1);
    memcpy(padded, data, dataLen);

    size_t numPages = paddedLen / 4;
    for (size_t i = 0; i < paddedLen; i += 4) {
        if (!ntagWritePage(startPage + (i / 4), padded + i)) {
            free(padded);
            log_w("NTAG write failed at page %d", startPage + (i / 4));
            return false;
        }
        delay(2);
    }

    free(padded);
    log_i("NTAG write complete (%d pages)", numPages);
    return true;
}

// --- Diagnostics ---

bool PN5180::getProductVersion(uint8_t* version, size_t len) {
    if (len < 2) return false;
    _readEeprom(0x10, 2, version);
    return (version[0] != 0xFF && version[0] != 0x00);
}

bool PN5180::getFirmwareVersion(uint8_t* version, size_t len) {
    if (len < 2) return false;
    _readEeprom(0x12, 2, version);
    return true;
}
