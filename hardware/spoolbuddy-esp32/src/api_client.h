#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct ApiResponse {
    bool success = false;
    JsonDocument doc;

    // Heartbeat response fields
    String pendingCommand() const;
    int32_t tareOffset(int32_t fallback) const;
    float calibrationFactor(float fallback) const;
    String sshPublicKey() const;
    String firmwareUrl() const;
    String firmwareVersion() const;
    int writeSpool() const;
    String writeNdefHex() const;
};

class ApiClient {
public:
    void begin(const String& backendUrl, const String& apiKey);

    ApiResponse registerDevice(
        const String& deviceId,
        const String& hostname,
        const String& ipAddress,
        const String& firmwareVersion,
        bool hasNfc,
        bool hasScale,
        int32_t tareOffset,
        float calibrationFactor,
        const String& nfcReaderType,
        const String& nfcConnection,
        const String& backendUrl
    );

    ApiResponse heartbeat(
        const String& deviceId,
        bool nfcOk,
        bool scaleOk,
        uint32_t uptimeS,
        const String& ipAddress,
        const String& firmwareVersion,
        const JsonObject& systemStats
    );

    ApiResponse tagScanned(
        const String& deviceId,
        const String& tagUid,
        const String& trayUuid,
        uint8_t sak,
        const String& tagType
    );

    ApiResponse tagRemoved(const String& deviceId, const String& tagUid);

    ApiResponse scaleReading(
        const String& deviceId,
        float weightGrams,
        bool stable,
        int32_t rawAdc
    );

    ApiResponse writeTagResult(
        const String& deviceId,
        int spoolId,
        const String& tagUid,
        bool success,
        const String& message
    );

    ApiResponse diagnosticResult(
        const String& deviceId,
        const String& diagnostic,
        bool success,
        const String& output
    );

    ApiResponse systemCommandResult(
        const String& deviceId,
        const String& command,
        bool success,
        const String& message
    );

    ApiResponse updateTare(const String& deviceId, int32_t tareOffset);

    bool isConnected() const { return _connected; }

private:
    String _baseUrl;
    String _apiKey;
    bool _connected = false;
    float _backoff = 1.0f;

    // Offline buffer
    struct BufferedRequest {
        String path;
        String body;
    };
    static const size_t BUFFER_MAX = 50;
    BufferedRequest _buffer[BUFFER_MAX];
    size_t _bufferHead = 0;
    size_t _bufferTail = 0;
    size_t _bufferCount = 0;

    ApiResponse _post(const String& path, const JsonDocument& doc);
    void _bufferRequest(const String& path, const String& body);
    void _flushBuffer();
};
