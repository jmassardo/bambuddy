#include "provision.h"
#include "config.h"
#include "pins.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

static Preferences prefs;
static WebServer* portalServer = nullptr;
static DNSServer* dnsServer = nullptr;
static bool portalDone = false;
static DeviceConfig pendingConfig = {};

// Minimal HTML for captive portal (fits in flash easily)
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BamBuddy Panel Setup</title>
<style>
body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;background:#1a1a2e;color:#eee}
h1{color:#00AE42;font-size:1.4em}
label{display:block;margin:12px 0 4px;font-size:0.9em;color:#aaa}
input,select{width:100%;padding:8px;border:1px solid #333;border-radius:4px;background:#16213e;color:#eee;box-sizing:border-box}
button{width:100%;padding:12px;margin-top:20px;background:#00AE42;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer}
button:hover{background:#009938}
.note{font-size:0.8em;color:#666;margin-top:8px}
</style></head><body>
<h1>&#x1F3ED; BamBuddy Panel Setup</h1>
<form method="POST" action="/save">
<label>WiFi Network</label>
<input name="ssid" placeholder="WiFi SSID" required>
<label>WiFi Password</label>
<input name="pass" type="password" placeholder="Password">
<label>BamBuddy Server</label>
<input name="host" placeholder="192.168.1.100" required>
<label>Server Port</label>
<input name="port" type="number" value="8000">
<label>API Key</label>
<input name="key" placeholder="Your API key" required>
<label>Device ID</label>
<input name="devid" placeholder="panel-001" required>
<label>Printer ID (number)</label>
<input name="printer" type="number" value="-1">
<p class="note">Set printer ID to -1 to assign later from the web UI.</p>
<button type="submit">Save &amp; Connect</button>
</form>
</body></html>
)rawliteral";

static const char SAVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved!</title>
<style>body{font-family:sans-serif;max-width:400px;margin:60px auto;padding:0 16px;background:#1a1a2e;color:#eee;text-align:center}h1{color:#00AE42}</style>
</head><body>
<h1>&#x2705; Configuration Saved</h1>
<p>Device will restart and connect to your network.</p>
<p style="color:#666;font-size:0.9em">This page will close automatically.</p>
</body></html>
)rawliteral";

void Provisioning::begin() {
    // Nothing to do at init
}

bool Provisioning::loadConfig(DeviceConfig& cfg) {
    prefs.begin(NVS_NAMESPACE, true);

    cfg.provisioned = prefs.getBool(NVS_KEY_PROVISIONED, false);
    if (!cfg.provisioned) {
        prefs.end();
        return false;
    }

    prefs.getString(NVS_KEY_WIFI_SSID, cfg.wifiSsid, sizeof(cfg.wifiSsid));
    prefs.getString(NVS_KEY_WIFI_PASS, cfg.wifiPass, sizeof(cfg.wifiPass));
    prefs.getString(NVS_KEY_SERVER_HOST, cfg.serverHost, sizeof(cfg.serverHost));
    cfg.serverPort = prefs.getUShort(NVS_KEY_SERVER_PORT, 8000);
    prefs.getString(NVS_KEY_API_KEY, cfg.apiKey, sizeof(cfg.apiKey));
    prefs.getString(NVS_KEY_DEVICE_ID, cfg.deviceId, sizeof(cfg.deviceId));
    cfg.printerId = prefs.getInt(NVS_KEY_PRINTER_ID, -1);

    prefs.end();
    return true;
}

void Provisioning::saveConfig(const DeviceConfig& cfg) {
    prefs.begin(NVS_NAMESPACE, false);

    prefs.putString(NVS_KEY_WIFI_SSID, cfg.wifiSsid);
    prefs.putString(NVS_KEY_WIFI_PASS, cfg.wifiPass);
    prefs.putString(NVS_KEY_SERVER_HOST, cfg.serverHost);
    prefs.putUShort(NVS_KEY_SERVER_PORT, cfg.serverPort);
    prefs.putString(NVS_KEY_API_KEY, cfg.apiKey);
    prefs.putString(NVS_KEY_DEVICE_ID, cfg.deviceId);
    prefs.putInt(NVS_KEY_PRINTER_ID, cfg.printerId);
    prefs.putBool(NVS_KEY_PROVISIONED, true);

    prefs.end();
}

void Provisioning::factoryReset() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    ESP.restart();
}

void Provisioning::startCaptivePortal() {
    Serial.println("[Provision] Starting captive portal AP...");

    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP("BamBuddy-Panel", "");  // Open network
    delay(500);
    Serial.printf("[Provision] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // DNS server — redirect all domains to us
    dnsServer = new DNSServer();
    dnsServer->start(53, "*", WiFi.softAPIP());

    // Web server
    portalServer = new WebServer(80);
    portalServer->on("/", HTTP_GET, [this]() { _handleRoot(); });
    portalServer->on("/save", HTTP_POST, [this]() { _handleSave(); });
    portalServer->onNotFound([this]() { _handleRoot(); });  // Captive portal redirect
    portalServer->begin();

    Serial.println("[Provision] Portal running. Waiting for config...");

    // Blink LED to indicate setup mode
    portalDone = false;
    while (!portalDone) {
        dnsServer->processNextRequest();
        portalServer->handleClient();
        // Blink LED
        digitalWrite(PIN_LED, (millis() / 500) % 2 == 0 ? LOW : HIGH);
        delay(1);
    }

    // Clean up
    portalServer->stop();
    dnsServer->stop();
    delete portalServer;
    delete dnsServer;
    portalServer = nullptr;
    dnsServer = nullptr;

    // Save and reboot
    saveConfig(pendingConfig);
    Serial.println("[Provision] Config saved. Rebooting in 2s...");
    delay(2000);
    ESP.restart();
}

void Provisioning::_handleRoot() {
    portalServer->send(200, "text/html", FPSTR(PORTAL_HTML));
}

void Provisioning::_handleSave() {
    // Parse form data
    String ssid = portalServer->arg("ssid");
    String pass = portalServer->arg("pass");
    String host = portalServer->arg("host");
    String port = portalServer->arg("port");
    String key = portalServer->arg("key");
    String devid = portalServer->arg("devid");
    String printer = portalServer->arg("printer");

    ssid.toCharArray(pendingConfig.wifiSsid, sizeof(pendingConfig.wifiSsid));
    pass.toCharArray(pendingConfig.wifiPass, sizeof(pendingConfig.wifiPass));
    host.toCharArray(pendingConfig.serverHost, sizeof(pendingConfig.serverHost));
    pendingConfig.serverPort = port.toInt();
    key.toCharArray(pendingConfig.apiKey, sizeof(pendingConfig.apiKey));
    devid.toCharArray(pendingConfig.deviceId, sizeof(pendingConfig.deviceId));
    pendingConfig.printerId = printer.toInt();
    pendingConfig.provisioned = true;

    Serial.printf("[Provision] Saving: SSID=%s Host=%s:%d DevID=%s\n",
                  pendingConfig.wifiSsid, pendingConfig.serverHost,
                  pendingConfig.serverPort, pendingConfig.deviceId);

    portalServer->send(200, "text/html", FPSTR(SAVE_HTML));
    portalDone = true;
}

void Provisioning::_handleScan() {
    // Optional: scan for WiFi networks (future enhancement)
}
