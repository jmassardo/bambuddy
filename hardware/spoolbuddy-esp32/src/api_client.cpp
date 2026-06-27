#include "api_client.h"
#include <HTTPClient.h>
#include <WiFi.h>

void ApiClient::begin(const String& backendUrl, const String& apiKey) {
    _baseUrl = backendUrl;
    if (_baseUrl.endsWith("/")) _baseUrl.remove(_baseUrl.length() - 1);
    _baseUrl += "/api/v1/spoolbuddy";
    _apiKey = apiKey;
}

ApiResponse ApiClient::_post(const String& path, const JsonDocument& doc) {
    ApiResponse resp;

    if (WiFi.status() != WL_CONNECTED) {
        String body;
        serializeJson(doc, body);
        _bufferRequest(path, body);
        return resp;
    }

    HTTPClient http;
    String url = _baseUrl + path;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    if (!_apiKey.isEmpty()) {
        http.addHeader("X-API-Key", _apiKey);
    }
    http.setTimeout(10000);

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    if (code >= 200 && code < 300) {
        resp.success = true;
        _connected = true;
        _backoff = 1.0f;

        String payload = http.getString();
        if (payload.length() > 0) {
            deserializeJson(resp.doc, payload);
        }
    } else {
        if (_connected) {
            log_w("Backend connection lost: HTTP %d on %s", code, path.c_str());
            _connected = false;
        }
        _bufferRequest(path, body);
    }

    http.end();
    return resp;
}

void ApiClient::_bufferRequest(const String& path, const String& body) {
    if (_bufferCount >= BUFFER_MAX) {
        // Drop oldest
        _bufferTail = (_bufferTail + 1) % BUFFER_MAX;
        _bufferCount--;
    }
    _buffer[_bufferHead] = {path, body};
    _bufferHead = (_bufferHead + 1) % BUFFER_MAX;
    _bufferCount++;
}

void ApiClient::_flushBuffer() {
    while (_bufferCount > 0) {
        auto& item = _buffer[_bufferTail];

        HTTPClient http;
        http.begin(_baseUrl + item.path);
        http.addHeader("Content-Type", "application/json");
        if (!_apiKey.isEmpty()) {
            http.addHeader("X-API-Key", _apiKey);
        }
        http.setTimeout(5000);

        int code = http.POST(item.body);
        http.end();

        if (code >= 200 && code < 300) {
            _bufferTail = (_bufferTail + 1) % BUFFER_MAX;
            _bufferCount--;
        } else {
            break;  // Stop flushing on first failure
        }
    }
}

// --- API Methods ---

ApiResponse ApiClient::registerDevice(
    const String& deviceId, const String& hostname, const String& ipAddress,
    const String& firmwareVersion, bool hasNfc, bool hasScale,
    int32_t tareOffset, float calibrationFactor,
    const String& nfcReaderType, const String& nfcConnection,
    const String& backendUrl
) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["hostname"] = hostname;
    doc["ip_address"] = ipAddress;
    doc["firmware_version"] = firmwareVersion;
    doc["has_nfc"] = hasNfc;
    doc["has_scale"] = hasScale;
    doc["tare_offset"] = tareOffset;
    doc["calibration_factor"] = calibrationFactor;
    doc["nfc_reader_type"] = nfcReaderType;
    doc["nfc_connection"] = nfcConnection;
    doc["backend_url"] = backendUrl;
    doc["has_backlight"] = false;

    return _post("/devices/register", doc);
}

ApiResponse ApiClient::heartbeat(
    const String& deviceId, bool nfcOk, bool scaleOk,
    uint32_t uptimeS, const String& ipAddress,
    const String& firmwareVersion, const JsonObject& systemStats
) {
    JsonDocument doc;
    doc["nfc_ok"] = nfcOk;
    doc["scale_ok"] = scaleOk;
    doc["uptime_s"] = uptimeS;
    doc["ip_address"] = ipAddress;
    doc["firmware_version"] = firmwareVersion;
    doc["nfc_reader_type"] = "PN5180";
    doc["nfc_connection"] = "SPI";

    if (!systemStats.isNull()) {
        doc["system_stats"] = systemStats;
    }

    ApiResponse resp = _post("/devices/" + deviceId + "/heartbeat", doc);

    if (resp.success && _bufferCount > 0) {
        _flushBuffer();
    }

    return resp;
}

ApiResponse ApiClient::tagScanned(
    const String& deviceId, const String& tagUid,
    const String& trayUuid, uint8_t sak, const String& tagType
) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["tag_uid"] = tagUid;
    if (!trayUuid.isEmpty()) doc["tray_uuid"] = trayUuid;
    doc["sak"] = sak;
    doc["tag_type"] = tagType;

    return _post("/nfc/tag-scanned", doc);
}

ApiResponse ApiClient::tagRemoved(const String& deviceId, const String& tagUid) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["tag_uid"] = tagUid;
    return _post("/nfc/tag-removed", doc);
}

ApiResponse ApiClient::scaleReading(
    const String& deviceId, float weightGrams, bool stable, int32_t rawAdc
) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["weight_grams"] = weightGrams;
    doc["stable"] = stable;
    doc["raw_adc"] = rawAdc;
    return _post("/scale/reading", doc);
}

ApiResponse ApiClient::writeTagResult(
    const String& deviceId, int spoolId, const String& tagUid,
    bool success, const String& message
) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    doc["spool_id"] = spoolId;
    doc["tag_uid"] = tagUid;
    doc["success"] = success;
    doc["message"] = message;
    return _post("/nfc/write-result", doc);
}

ApiResponse ApiClient::diagnosticResult(
    const String& deviceId, const String& diagnostic,
    bool success, const String& output
) {
    JsonDocument doc;
    doc["diagnostic"] = diagnostic;
    doc["success"] = success;
    doc["output"] = output;
    doc["exit_code"] = success ? 0 : 1;
    return _post("/diagnostics/" + deviceId + "/result", doc);
}

ApiResponse ApiClient::systemCommandResult(
    const String& deviceId, const String& command,
    bool success, const String& message
) {
    JsonDocument doc;
    doc["command"] = command;
    doc["success"] = success;
    doc["message"] = message;
    return _post("/devices/" + deviceId + "/system/command-result", doc);
}

ApiResponse ApiClient::updateTare(const String& deviceId, int32_t tareOffset) {
    JsonDocument doc;
    doc["tare_offset"] = tareOffset;
    return _post("/devices/" + deviceId + "/calibration/set-tare", doc);
}

// --- ApiResponse helpers ---

String ApiResponse::pendingCommand() const {
    return doc["pending_command"] | "";
}

int32_t ApiResponse::tareOffset(int32_t fallback) const {
    return doc["tare_offset"] | fallback;
}

float ApiResponse::calibrationFactor(float fallback) const {
    return doc["calibration_factor"] | fallback;
}

String ApiResponse::sshPublicKey() const {
    return doc["ssh_public_key"] | "";
}

String ApiResponse::firmwareUrl() const {
    return doc["firmware_url"] | "";
}

String ApiResponse::firmwareVersion() const {
    return doc["firmware_version"] | "";
}

int ApiResponse::writeSpool() const {
    JsonObjectConst payload = doc["pending_write_payload"];
    return payload["spool_id"] | 0;
}

String ApiResponse::writeNdefHex() const {
    JsonObjectConst payload = doc["pending_write_payload"];
    return payload["ndef_data_hex"] | "";
}
