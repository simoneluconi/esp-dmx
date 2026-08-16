#pragma once

#include <Arduino.h>
#include <vector>

#include "config.h"

// A "profile" is a reusable fixture template — like an entry in a real
// console's fixture library: a name for each channel in order (e.g. Red,
// Green, Blue, Dimmer). Its channel count is its DMX footprint.
struct FixtureProfile {
  uint16_t id;
  String name;                       // e.g. "Generic RGB PAR"
  std::vector<String> channelNames;  // one entry per channel, in order
};

// A "fixture" is a profile patched at a DMX address, exactly like patching
// a fixture from the library onto a real console: start address + which
// profile it uses (the profile supplies the channel count and names).
struct Fixture {
  uint16_t id;         // internal id, assigned automatically
  String name;         // free-form instance name, e.g. "Stage left RGB PAR"
  uint16_t address;    // DMX start address, 1-512
  uint16_t profileId;  // which FixtureProfile this instance uses
};

// A "preset" is a full snapshot of all 512 channels, exactly like a scene/
// cue on a real console: save the current look, recall it instantly later.
struct Preset {
  uint16_t id;
  String name;
  uint8_t values[DMX_UNIVERSE_SIZE];
};

namespace Storage {

// Mounts the filesystem (LittleFS, formatting it on first boot) and loads
// previously saved fixtures and Wi-Fi credentials.
void begin();

// ---- Fixture profiles (templates) -------------------------------------------
const std::vector<FixtureProfile> &profiles();

FixtureProfile *findProfile(uint16_t id);

// Adds a profile (1-MAX_CHANNELS_PER_FIXTURE channel names).
// Returns the assigned id, or 0 on error (buffer full or invalid parameters).
uint16_t addProfile(const String &name, const std::vector<String> &channelNames, String &errorOut);

// Updates an existing profile (same validation as addProfile).
bool updateProfile(uint16_t id, const String &name, const std::vector<String> &channelNames, String &errorOut);

// Fails if any patched fixture still references this profile.
bool removeProfile(uint16_t id, String &errorOut);

// ---- Fixtures (profile patched at a DMX address) ----------------------------
const std::vector<Fixture> &fixtures();

// Returns a pointer to the fixture with the given id, or nullptr if missing.
Fixture *findFixture(uint16_t id);

// Adds a fixture, validating that profileId exists and that its channel
// range fits in the universe (1-512) without overlapping another fixture.
// Returns the assigned id, or 0 on error (buffer full or invalid parameters).
uint16_t addFixture(const String &name, uint16_t address, uint16_t profileId, String &errorOut);

// Updates an existing fixture (same validation as addFixture).
bool updateFixture(uint16_t id, const String &name, uint16_t address, uint16_t profileId, String &errorOut);

bool removeFixture(uint16_t id);

// ---- Presets ----------------------------------------------------------------
const std::vector<Preset> &presets();

Preset *findPreset(uint16_t id);

// Saves `values` (DMX_UNIVERSE_SIZE bytes) as a new preset.
// Returns the assigned id, or 0 on error (buffer full or invalid name).
uint16_t addPreset(const String &name, const uint8_t *values, String &errorOut);

// Overwrites an existing preset's name and/or stored values.
bool updatePreset(uint16_t id, const String &name, const uint8_t *values, String &errorOut);

bool removePreset(uint16_t id);

// ---- Wi-Fi ------------------------------------------------------------------
String staSsid();
String staPassword();
bool hasStaCredentials();
void setStaCredentials(const String &ssid, const String &password);

// ---- Art-Net ----------------------------------------------------------------
bool artnetEnabled();
uint16_t artnetUniverse();
void setArtnetConfig(bool enabled, uint16_t universe);

} // namespace Storage
