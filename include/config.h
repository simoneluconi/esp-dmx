#pragma once

#include <Arduino.h>

// ============================================================================
// PINOUT — ESP32-C3 Super Mini  <->  TTL-to-RS485 module (MAX485 or similar)
// ============================================================================
// Module with DE and RE tied together on a single direction pin:
//   DE+RE  -> DMX_EN_PIN   (HIGH = transmit, LOW = receive)
//   DI     -> DMX_TX_PIN
//   RO     -> DMX_RX_PIN
//   VCC    -> 3V3 (or 5V if the module needs it, as long as RO/DI are 5V-tolerant)
//   GND    -> GND
// If the module has automatic direction switching (e.g. based on MAX13487),
// DMX_EN_PIN can be left unconnected: the library still drives it, it just
// has no effect on the module.
constexpr int DMX_TX_PIN = 4;
constexpr int DMX_RX_PIN = 5;
constexpr int DMX_EN_PIN = 6;

// Onboard status LED found on most "Super Mini" boards (active low)
constexpr int STATUS_LED_PIN = 8;
constexpr bool STATUS_LED_ACTIVE_LOW = true;

// ============================================================================
// DMX
// ============================================================================
constexpr int DMX_PORT_NUM = 1;          // UART1 (UART0 is used by native USB-CDC)
constexpr int DMX_UNIVERSE_SIZE = 512;   // channels 1..512
constexpr uint32_t DMX_REFRESH_MS = 25;  // ~40 packets/second

// ============================================================================
// Wi-Fi
// ============================================================================
// Base name for the access point; the last 2 bytes of the AP MAC address are
// appended at boot (e.g. "ESP32-DMX-9F92") so multiple units never collide.
// The same generated name is reused as the mDNS hostname and OTA hostname.
constexpr const char *AP_SSID_DEFAULT = "ESP32-DMX";
constexpr const char *AP_PASS_DEFAULT = "dmx12345"; // 8 characters minimum
constexpr uint32_t WIFI_STA_TIMEOUT_MS = 12000;

// ============================================================================
// OTA (Over-The-Air updates)
// ============================================================================
// Password used both by PlatformIO's "espota" network upload and by the
// browser-based firmware upload page. Change it before deploying!
constexpr const char *OTA_PASSWORD = "dmxota123";

// ============================================================================
// WebSocket realtime control
// ============================================================================
// Interval used only as a keep-alive/resync safety net; every channel change
// is pushed to clients immediately, this is not a polling interval.
constexpr uint32_t WS_CLEANUP_MS = 1000;

// ============================================================================
// Fixture profiles (templates) and patch (fixture instances)
// ============================================================================
// A "profile" is a reusable fixture template (like a fixture library entry
// on a real console): a name per channel, e.g. Red/Green/Blue/Dimmer.
// A "fixture" is a profile patched at a DMX start address.
constexpr int MAX_PROFILES = 32;
constexpr int MAX_CHANNELS_PER_FIXTURE = 32;
constexpr int MAX_FIXTURES = 32;

constexpr const char *PROFILES_FILE = "/profiles.json";
constexpr const char *FIXTURES_FILE = "/fixtures.json";
constexpr const char *WIFI_FILE = "/wifi.json";

// ============================================================================
// Presets (full-universe scene snapshots)
// ============================================================================
constexpr int MAX_PRESETS = 24;
constexpr const char *PRESETS_FILE = "/presets.json";

// ============================================================================
// Art-Net input (network -> DMX)
// ============================================================================
// Off by default; the enabled flag and universe are persisted and restored
// on boot. Universe is the raw 0-based Art-Net Port-Address, matching the
// "universe" field most Art-Net software exposes in its advanced/raw view.
constexpr bool ARTNET_ENABLED_DEFAULT = false;
constexpr uint16_t ARTNET_UNIVERSE_DEFAULT = 0;
constexpr const char *ARTNET_FILE = "/artnet.json";
