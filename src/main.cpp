#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <vector>

#include "config.h"
#include "storage.h"
#include "dmx_controller.h"
#include "artnet_receiver.h"
#include "web_ui.h"

namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String g_apSsid;

volatile bool restartPending = false;
uint32_t restartAtMs = 0;
volatile bool otaUnauthorized = false;

void scheduleRestart(uint32_t delayMs) {
  restartAtMs = millis() + delayMs;
  restartPending = true;
}

// ---------------------------------------------------------------------------
// WebSocket — realtime channel control
// ---------------------------------------------------------------------------
String buildFullStateJson() {
  uint8_t values[DMX_UNIVERSE_SIZE];
  DmxController::getAllChannels(values);

  String s;
  s.reserve(DMX_UNIVERSE_SIZE * 4 + 10);
  s += "{\"d\":[";
  for (int i = 0; i < DMX_UNIVERSE_SIZE; i++) {
    s += values[i];
    if (i < DMX_UNIVERSE_SIZE - 1) s += ',';
  }
  s += "]}";
  return s;
}

void handleWsMessage(uint8_t *data, size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
    return;
  }

  if (doc["c"].is<int>()) {
    int channel = doc["c"];
    int value = doc["v"] | 0;
    value = constrain(value, 0, 255);
    DmxController::setChannel(channel, static_cast<uint8_t>(value));

    char buf[40];
    int n = snprintf(buf, sizeof(buf), "{\"c\":%d,\"v\":%d}", channel, value);
    ws.textAll(buf, n);
  } else if (doc["a"].is<const char *>()) {
    String action = doc["a"].as<String>();
    if (action == "bo") {
      Serial.println("[WS] Blackout received");
      DmxController::blackout();
      ws.textAll("{\"a\":\"bo\"}");
    } else if (action == "fo") {
      Serial.println("[WS] Full ON received");
      DmxController::fullOn();
      ws.textAll("{\"a\":\"fo\"}");
    }
  }
}

void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    client->text(buildFullStateJson());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      handleWsMessage(data, len);
    } else {
      Serial.printf("[WS] Ignored non-simple frame (final=%d index=%u len=%u opcode=%d)\n",
                    info->final, (unsigned)info->index, (unsigned)len, (int)info->opcode);
    }
  }
}

// ---------------------------------------------------------------------------
// REST — status, fixtures, Wi-Fi config
// ---------------------------------------------------------------------------
void handleStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["apSsid"] = g_apSsid;
  doc["apIp"] = WiFi.softAPIP().toString();
  bool staConnected = WiFi.status() == WL_CONNECTED;
  doc["staConnected"] = staConnected;
  doc["staSsid"] = Storage::staSsid();
  doc["staIp"] = staConnected ? WiFi.localIP().toString() : "";

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleGetProfiles(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonArray arr = doc["profiles"].to<JsonArray>();
  for (const FixtureProfile &p : Storage::profiles()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id;
    o["name"] = p.name;
    JsonArray channels = o["channels"].to<JsonArray>();
    for (const String &ch : p.channelNames) channels.add(ch);
  }
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleGetFixtures(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonArray arr = doc["fixtures"].to<JsonArray>();
  for (const Fixture &fx : Storage::fixtures()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = fx.id;
    o["name"] = fx.name;
    o["address"] = fx.address;
    o["profileId"] = fx.profileId;
    JsonArray channels = o["channels"].to<JsonArray>();
    FixtureProfile *profile = Storage::findProfile(fx.profileId);
    if (profile) {
      o["profileName"] = profile->name;
      for (const String &ch : profile->channelNames) channels.add(ch);
    } else {
      o["profileName"] = "";
    }
  }
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void handleGetPresets(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonArray arr = doc["presets"].to<JsonArray>();
  for (const Preset &p : Storage::presets()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id;
    o["name"] = p.name;
  }
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

void sendJsonError(AsyncWebServerRequest *request, int code, const String &error) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  String out;
  serializeJson(doc, out);
  request->send(code, "application/json", out);
}

void sendJsonOk(AsyncWebServerRequest *request, uint16_t id = 0) {
  JsonDocument doc;
  doc["ok"] = true;
  if (id) doc["id"] = id;
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

// ESPAsyncWebServer's default matcher for a plain string is "backward
// compatible" (exact match OR prefix + "/..."), so a handler registered at
// "/api/presets" would also swallow POSTs to "/api/presets/recall" etc.
// Force exact matching so each JSON route only handles its own exact URL.
AsyncCallbackJsonWebHandler *jsonHandler(const char *url, ArJsonRequestHandlerFunction fn) {
  auto *h = new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact(url), fn);
  h->setMethod(HTTP_POST);
  return h;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, handleStatus);

  server.on("/api/profiles", HTTP_GET, handleGetProfiles);

  server.addHandler(jsonHandler("/api/profiles", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    String name = o["name"] | "";
    std::vector<String> channels;
    for (JsonVariant v : o["channels"].as<JsonArray>()) channels.push_back(v.as<String>());
    String err;
    uint16_t id = Storage::addProfile(name, channels, err);
    if (id == 0) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request, id);
  }));

  server.addHandler(jsonHandler("/api/profiles/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    String name = o["name"] | "";
    std::vector<String> channels;
    for (JsonVariant v : o["channels"].as<JsonArray>()) channels.push_back(v.as<String>());
    String err;
    if (!Storage::updateProfile(id, name, channels, err)) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request);
  }));

  server.addHandler(jsonHandler("/api/profiles/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    String err;
    if (!Storage::removeProfile(id, err)) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request);
  }));

  server.on("/api/fixtures", HTTP_GET, handleGetFixtures);

  server.addHandler(jsonHandler("/api/fixtures", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    String name = o["name"] | "";
    uint16_t address = o["address"] | 0;
    uint16_t profileId = o["profileId"] | 0;
    String err;
    uint16_t id = Storage::addFixture(name, address, profileId, err);
    if (id == 0) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request, id);
  }));

  server.addHandler(jsonHandler("/api/fixtures/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    String name = o["name"] | "";
    uint16_t address = o["address"] | 0;
    uint16_t profileId = o["profileId"] | 0;
    String err;
    if (!Storage::updateFixture(id, name, address, profileId, err)) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request);
  }));

  server.addHandler(jsonHandler("/api/fixtures/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    if (!Storage::removeFixture(id)) {
      sendJsonError(request, 404, "Fixture not found");
      return;
    }
    sendJsonOk(request);
  }));

  server.on("/api/presets", HTTP_GET, handleGetPresets);

  server.addHandler(jsonHandler("/api/presets", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    String name = o["name"] | "";
    uint8_t values[DMX_UNIVERSE_SIZE];
    DmxController::getAllChannels(values);
    String err;
    uint16_t id = Storage::addPreset(name, values, err);
    if (id == 0) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request, id);
  }));

  server.addHandler(jsonHandler("/api/presets/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    String name = o["name"] | "";
    uint8_t values[DMX_UNIVERSE_SIZE];
    DmxController::getAllChannels(values);
    String err;
    if (!Storage::updatePreset(id, name, values, err)) {
      sendJsonError(request, 400, err);
      return;
    }
    sendJsonOk(request);
  }));

  server.addHandler(jsonHandler("/api/presets/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    if (!Storage::removePreset(id)) {
      sendJsonError(request, 404, "Preset not found");
      return;
    }
    sendJsonOk(request);
  }));

  server.addHandler(jsonHandler("/api/presets/recall", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    uint16_t id = o["id"] | 0;
    Preset *p = Storage::findPreset(id);
    if (!p) {
      sendJsonError(request, 404, "Preset not found");
      return;
    }
    DmxController::setChannels(1, p->values, DMX_UNIVERSE_SIZE);
    ws.textAll(buildFullStateJson());
    sendJsonOk(request);
  }));

  server.addHandler(jsonHandler("/api/wifi", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    String ssid = o["ssid"] | "";
    String password = o["password"] | "";
    Storage::setStaCredentials(ssid, password);
    sendJsonOk(request);
    scheduleRestart(800);
  }));

  server.on("/api/artnet", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["enabled"] = ArtnetReceiver::enabled();
    doc["universe"] = ArtnetReceiver::universe();
    uint32_t last = ArtnetReceiver::lastPacketMs();
    doc["msSinceLastPacket"] = last ? static_cast<int32_t>(millis() - last) : -1;
    doc["senderIp"] = ArtnetReceiver::lastSenderIp();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.addHandler(jsonHandler("/api/artnet", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject o = json.as<JsonObject>();
    bool enabled = o["enabled"] | false;
    uint16_t universe = o["universe"] | 0;
    ArtnetReceiver::setEnabled(enabled);
    ArtnetReceiver::setUniverse(universe);
    sendJsonOk(request);
  }));

  // ---- OTA firmware upload (multipart form: fields "password" + "firmware") --
  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (otaUnauthorized) {
          otaUnauthorized = false;
          request->send(401, "text/plain", "Wrong OTA password");
          return;
        }
        bool ok = !Update.hasError();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Update failed");
        if (ok) scheduleRestart(600);
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          String pass = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
          otaUnauthorized = (pass != OTA_PASSWORD);
          if (otaUnauthorized) {
            Serial.println("[OTA] Rejected: wrong password");
            return;
          }
          Serial.printf("[OTA] Web update start: %s\n", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
        }
        if (otaUnauthorized) return;
        if (!Update.hasError() && Update.write(data, len) != len) Update.printError(Serial);
        if (final) {
          if (Update.end(true)) Serial.printf("[OTA] Web update success: %uB\n", index + len);
          else Update.printError(Serial);
        }
      });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

// ---------------------------------------------------------------------------
// Wi-Fi + OTA (network) bring-up
// ---------------------------------------------------------------------------
// Builds a per-device unique name ("ESP32-DMX-9F92") from the last 2 bytes
// of the AP MAC address, so multiple units on the same network never share
// an AP SSID, mDNS hostname, or OTA hostname.
String uniqueDeviceName() {
  String mac = WiFi.softAPmacAddress();
  mac.replace(":", "");
  return String(AP_SSID_DEFAULT) + "-" + mac.substring(mac.length() - 4);
}

void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  g_apSsid = uniqueDeviceName();

  // Cheap ESP32-C3 "Super Mini" boards have an undersized PA/regulator and
  // are widely reported to run dangerously hot (sometimes brownout-resetting)
  // at full WiFi TX power. This is a known hardware limitation of the board,
  // not a firmware bug — capping power here is the same fix used in ESPHome
  // (`wifi.output_power: 8.5dB`/`10dB`). Range is still plenty for typical
  // indoor/venue use; raise it back towards WIFI_POWER_19_5dBm only if your
  // specific board runs cool and you actually need more range.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  WiFi.softAP(g_apSsid.c_str(), AP_PASS_DEFAULT);
  Serial.printf("[WiFi] Access point \"%s\" up at %s\n", g_apSsid.c_str(), WiFi.softAPIP().toString().c_str());

  if (Storage::hasStaCredentials()) {
    Serial.printf("[WiFi] Joining \"%s\"...\n", Storage::staSsid().c_str());
    WiFi.begin(Storage::staSsid().c_str(), Storage::staPassword().c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_STA_TIMEOUT_MS) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected, IP %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WiFi] Could not join the configured network, staying on AP only");
    }
  }

  if (MDNS.begin(g_apSsid.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WiFi] mDNS ready: http://%s.local\n", g_apSsid.c_str());
  }
}

void setupOta() {
  ArduinoOTA.setHostname(g_apSsid.c_str());
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() { Serial.println("[OTA] Network update start"); });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] Network update done"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("[OTA] Error [%u]\n", error); });

  ArduinoOTA.begin();
}

void setupStatusLed() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_LOW ? HIGH : LOW); // off
}

void updateStatusLed() {
  static uint32_t lastToggle = 0;
  static bool ledState = false;
  bool staConnected = WiFi.status() == WL_CONNECTED;

  // Solid on when joined to a Wi-Fi network, slow blink on AP-only mode.
  if (staConnected) {
    digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_LOW ? LOW : HIGH);
    return;
  }
  if (millis() - lastToggle > 700) {
    lastToggle = millis();
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState == STATUS_LED_ACTIVE_LOW ? HIGH : LOW);
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  setupStatusLed();

  Storage::begin();
  DmxController::begin();
  setupWifi();
  setupWebServer();
  setupOta();
  ArtnetReceiver::begin();

  Serial.println("[Main] Ready");
}

void loop() {
  ArduinoOTA.handle();
  updateStatusLed();
  ArtnetReceiver::poll();

  static uint32_t lastCleanup = 0;
  if (millis() - lastCleanup > WS_CLEANUP_MS) {
    lastCleanup = millis();
    ws.cleanupClients();
  }

  // Throttled: broadcast the new state at most every 200ms while Art-Net
  // data keeps arriving, instead of on every single incoming packet.
  static uint32_t lastArtnetBroadcast = 0;
  if (millis() - lastArtnetBroadcast > 200) {
    lastArtnetBroadcast = millis();
    if (ArtnetReceiver::consumeDirty()) {
      ws.textAll(buildFullStateJson());
    }
  }

  if (restartPending && millis() >= restartAtMs) {
    Serial.println("[Main] Restarting...");
    delay(50);
    ESP.restart();
  }
}
