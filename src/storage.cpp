#include "storage.h"
#include "config.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

namespace Storage {

namespace {

std::vector<FixtureProfile> g_profiles;
uint16_t g_nextProfileId = 1;

std::vector<Fixture> g_fixtures;
uint16_t g_nextId = 1;

std::vector<Preset> g_presets;
uint16_t g_nextPresetId = 1;

String g_staSsid;
String g_staPass;

bool g_artnetEnabled = ARTNET_ENABLED_DEFAULT;
uint16_t g_artnetUniverse = ARTNET_UNIVERSE_DEFAULT;

void saveProfiles() {
  JsonDocument doc;
  JsonArray arr = doc["profiles"].to<JsonArray>();
  for (const FixtureProfile &p : g_profiles) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id;
    o["name"] = p.name;
    JsonArray channels = o["channels"].to<JsonArray>();
    for (const String &ch : p.channelNames) channels.add(ch);
  }

  File f = LittleFS.open(PROFILES_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

// Ships a small, generic library out of the box (same idea as the built-in
// generic fixtures every real console library includes), so there's
// something to patch immediately on first boot.
void seedDefaultProfiles() {
  g_profiles.clear();
  g_nextProfileId = 1;

  auto seed = [](const char *name, std::vector<String> channels) {
    FixtureProfile p;
    p.id = g_nextProfileId++;
    p.name = name;
    p.channelNames = std::move(channels);
    g_profiles.push_back(p);
  };

  seed("Generic Dimmer", {"Dimmer"});
  seed("Generic RGB", {"Red", "Green", "Blue"});
  seed("Generic RGBW", {"Red", "Green", "Blue", "White"});

  saveProfiles();
}

void loadProfiles() {
  g_profiles.clear();
  g_nextProfileId = 1;

  if (!LittleFS.exists(PROFILES_FILE)) {
    seedDefaultProfiles();
    return;
  }

  File f = LittleFS.open(PROFILES_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  for (JsonObject o : doc["profiles"].as<JsonArray>()) {
    FixtureProfile p;
    p.id = o["id"] | 0;
    p.name = o["name"] | "";
    if (p.id == 0) continue;

    for (JsonVariant v : o["channels"].as<JsonArray>()) {
      if (p.channelNames.size() >= MAX_CHANNELS_PER_FIXTURE) break;
      p.channelNames.push_back(v.as<String>());
    }
    if (p.channelNames.empty()) continue;

    g_profiles.push_back(p);
    if (p.id >= g_nextProfileId) g_nextProfileId = p.id + 1;
  }
}

void loadFixtures() {
  g_fixtures.clear();
  g_nextId = 1;

  if (!LittleFS.exists(FIXTURES_FILE)) return;

  File f = LittleFS.open(FIXTURES_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  for (JsonObject o : doc["fixtures"].as<JsonArray>()) {
    Fixture fx;
    fx.id = o["id"] | 0;
    fx.name = o["name"] | "";
    fx.address = o["address"] | 1;
    fx.profileId = o["profileId"] | 0;
    if (fx.id == 0 || fx.profileId == 0) continue;
    g_fixtures.push_back(fx);
    if (fx.id >= g_nextId) g_nextId = fx.id + 1;
  }
}

void saveFixtures() {
  JsonDocument doc;
  JsonArray arr = doc["fixtures"].to<JsonArray>();
  for (const Fixture &fx : g_fixtures) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = fx.id;
    o["name"] = fx.name;
    o["address"] = fx.address;
    o["profileId"] = fx.profileId;
  }

  File f = LittleFS.open(FIXTURES_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void loadPresets() {
  g_presets.clear();
  g_nextPresetId = 1;

  if (!LittleFS.exists(PRESETS_FILE)) return;

  File f = LittleFS.open(PRESETS_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  for (JsonObject o : doc["presets"].as<JsonArray>()) {
    Preset p;
    p.id = o["id"] | 0;
    p.name = o["name"] | "";
    if (p.id == 0) continue;

    JsonArray values = o["values"].as<JsonArray>();
    size_t i = 0;
    for (JsonVariant v : values) {
      if (i >= DMX_UNIVERSE_SIZE) break;
      p.values[i++] = v.as<uint8_t>();
    }
    while (i < DMX_UNIVERSE_SIZE) p.values[i++] = 0;

    g_presets.push_back(p);
    if (p.id >= g_nextPresetId) g_nextPresetId = p.id + 1;
  }
}

void savePresets() {
  JsonDocument doc;
  JsonArray arr = doc["presets"].to<JsonArray>();
  for (const Preset &p : g_presets) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id;
    o["name"] = p.name;
    JsonArray values = o["values"].to<JsonArray>();
    for (int i = 0; i < DMX_UNIVERSE_SIZE; i++) values.add(p.values[i]);
  }

  File f = LittleFS.open(PRESETS_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void loadWifi() {
  g_staSsid = "";
  g_staPass = "";

  if (!LittleFS.exists(WIFI_FILE)) return;

  File f = LittleFS.open(WIFI_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  g_staSsid = doc["ssid"] | "";
  g_staPass = doc["password"] | "";
}

void saveWifi() {
  JsonDocument doc;
  doc["ssid"] = g_staSsid;
  doc["password"] = g_staPass;

  File f = LittleFS.open(WIFI_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void loadArtnet() {
  g_artnetEnabled = ARTNET_ENABLED_DEFAULT;
  g_artnetUniverse = ARTNET_UNIVERSE_DEFAULT;

  if (!LittleFS.exists(ARTNET_FILE)) return;

  File f = LittleFS.open(ARTNET_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  g_artnetEnabled = doc["enabled"] | ARTNET_ENABLED_DEFAULT;
  g_artnetUniverse = doc["universe"] | ARTNET_UNIVERSE_DEFAULT;
}

void saveArtnet() {
  JsonDocument doc;
  doc["enabled"] = g_artnetEnabled;
  doc["universe"] = g_artnetUniverse;

  File f = LittleFS.open(ARTNET_FILE, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

// Checks that [address, address+channels-1] fits in the universe and does
// not overlap any existing fixture other than `excludeId`.
bool rangeIsFree(uint16_t address, uint8_t channels, uint16_t excludeId, String &errorOut) {
  if (address < 1 || address > DMX_UNIVERSE_SIZE) {
    errorOut = "Address out of range (1-512)";
    return false;
  }
  uint32_t end = static_cast<uint32_t>(address) + channels - 1;
  if (end > DMX_UNIVERSE_SIZE) {
    errorOut = "Fixture extends past channel 512";
    return false;
  }

  for (const Fixture &fx : g_fixtures) {
    if (fx.id == excludeId) continue;
    const FixtureProfile *fxProfile = nullptr;
    for (const FixtureProfile &p : g_profiles) {
      if (p.id == fx.profileId) { fxProfile = &p; break; }
    }
    if (!fxProfile) continue;  // orphaned reference, shouldn't happen
    uint32_t fxEnd = static_cast<uint32_t>(fx.address) + fxProfile->channelNames.size() - 1;
    bool overlap = address <= fxEnd && end >= fx.address;
    if (overlap) {
      errorOut = "Overlaps fixture \"" + fx.name + "\" (address " + String(fx.address) + ")";
      return false;
    }
  }
  return true;
}

} // namespace

void begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] Failed to mount LittleFS");
    return;
  }
  loadProfiles();
  loadFixtures();
  loadPresets();
  loadWifi();
  loadArtnet();
}

// ---- Fixture profiles -------------------------------------------------------

const std::vector<FixtureProfile> &profiles() { return g_profiles; }

FixtureProfile *findProfile(uint16_t id) {
  for (FixtureProfile &p : g_profiles) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

uint16_t addProfile(const String &name, const std::vector<String> &channelNames, String &errorOut) {
  if (g_profiles.size() >= MAX_PROFILES) {
    errorOut = "Maximum number of profiles reached (" + String(MAX_PROFILES) + ")";
    return 0;
  }
  if (channelNames.empty() || channelNames.size() > MAX_CHANNELS_PER_FIXTURE) {
    errorOut = "Invalid channel count (1-" + String(MAX_CHANNELS_PER_FIXTURE) + ")";
    return 0;
  }

  FixtureProfile p;
  p.id = g_nextProfileId++;
  p.name = name.length() ? name : ("Profile " + String(p.id));
  p.channelNames = channelNames;
  for (size_t i = 0; i < p.channelNames.size(); i++) {
    if (!p.channelNames[i].length()) p.channelNames[i] = "Ch " + String(i + 1);
  }
  g_profiles.push_back(p);
  saveProfiles();
  return p.id;
}

bool updateProfile(uint16_t id, const String &name, const std::vector<String> &channelNames, String &errorOut) {
  FixtureProfile *p = findProfile(id);
  if (!p) {
    errorOut = "Profile not found";
    return false;
  }
  if (channelNames.empty() || channelNames.size() > MAX_CHANNELS_PER_FIXTURE) {
    errorOut = "Invalid channel count (1-" + String(MAX_CHANNELS_PER_FIXTURE) + ")";
    return false;
  }

  p->name = name.length() ? name : p->name;
  p->channelNames = channelNames;
  for (size_t i = 0; i < p->channelNames.size(); i++) {
    if (!p->channelNames[i].length()) p->channelNames[i] = "Ch " + String(i + 1);
  }
  saveProfiles();
  return true;
}

bool removeProfile(uint16_t id, String &errorOut) {
  for (const Fixture &fx : g_fixtures) {
    if (fx.profileId == id) {
      errorOut = "In use by fixture \"" + fx.name + "\" — delete or reassign it first";
      return false;
    }
  }
  for (size_t i = 0; i < g_profiles.size(); i++) {
    if (g_profiles[i].id == id) {
      g_profiles.erase(g_profiles.begin() + i);
      saveProfiles();
      return true;
    }
  }
  errorOut = "Profile not found";
  return false;
}

// ---- Fixtures -----------------------------------------------------------------

const std::vector<Fixture> &fixtures() { return g_fixtures; }

Fixture *findFixture(uint16_t id) {
  for (Fixture &fx : g_fixtures) {
    if (fx.id == id) return &fx;
  }
  return nullptr;
}

uint16_t addFixture(const String &name, uint16_t address, uint16_t profileId, String &errorOut) {
  if (g_fixtures.size() >= MAX_FIXTURES) {
    errorOut = "Maximum number of fixtures reached (" + String(MAX_FIXTURES) + ")";
    return 0;
  }
  FixtureProfile *profile = findProfile(profileId);
  if (!profile) {
    errorOut = "Profile not found";
    return 0;
  }
  if (!rangeIsFree(address, profile->channelNames.size(), 0, errorOut)) return 0;

  Fixture fx;
  fx.id = g_nextId++;
  fx.name = name.length() ? name : (profile->name + " " + String(fx.id));
  fx.address = address;
  fx.profileId = profileId;
  g_fixtures.push_back(fx);
  saveFixtures();
  return fx.id;
}

bool updateFixture(uint16_t id, const String &name, uint16_t address, uint16_t profileId, String &errorOut) {
  Fixture *fx = findFixture(id);
  if (!fx) {
    errorOut = "Fixture not found";
    return false;
  }
  FixtureProfile *profile = findProfile(profileId);
  if (!profile) {
    errorOut = "Profile not found";
    return false;
  }
  if (!rangeIsFree(address, profile->channelNames.size(), id, errorOut)) return false;

  fx->name = name.length() ? name : fx->name;
  fx->address = address;
  fx->profileId = profileId;
  saveFixtures();
  return true;
}

bool removeFixture(uint16_t id) {
  for (size_t i = 0; i < g_fixtures.size(); i++) {
    if (g_fixtures[i].id == id) {
      g_fixtures.erase(g_fixtures.begin() + i);
      saveFixtures();
      return true;
    }
  }
  return false;
}

// ---- Presets --------------------------------------------------------------

const std::vector<Preset> &presets() { return g_presets; }

Preset *findPreset(uint16_t id) {
  for (Preset &p : g_presets) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

uint16_t addPreset(const String &name, const uint8_t *values, String &errorOut) {
  if (g_presets.size() >= MAX_PRESETS) {
    errorOut = "Maximum number of presets reached (" + String(MAX_PRESETS) + ")";
    return 0;
  }

  Preset p;
  p.id = g_nextPresetId++;
  p.name = name.length() ? name : ("Preset " + String(p.id));
  memcpy(p.values, values, DMX_UNIVERSE_SIZE);
  g_presets.push_back(p);
  savePresets();
  return p.id;
}

bool updatePreset(uint16_t id, const String &name, const uint8_t *values, String &errorOut) {
  Preset *p = findPreset(id);
  if (!p) {
    errorOut = "Preset not found";
    return false;
  }
  p->name = name.length() ? name : p->name;
  memcpy(p->values, values, DMX_UNIVERSE_SIZE);
  savePresets();
  return true;
}

bool removePreset(uint16_t id) {
  for (size_t i = 0; i < g_presets.size(); i++) {
    if (g_presets[i].id == id) {
      g_presets.erase(g_presets.begin() + i);
      savePresets();
      return true;
    }
  }
  return false;
}

String staSsid() { return g_staSsid; }
String staPassword() { return g_staPass; }
bool hasStaCredentials() { return g_staSsid.length() > 0; }

void setStaCredentials(const String &ssid, const String &password) {
  g_staSsid = ssid;
  g_staPass = password;
  saveWifi();
}

bool artnetEnabled() { return g_artnetEnabled; }
uint16_t artnetUniverse() { return g_artnetUniverse; }

void setArtnetConfig(bool enabled, uint16_t universe) {
  g_artnetEnabled = enabled;
  g_artnetUniverse = universe;
  saveArtnet();
}

} // namespace Storage
