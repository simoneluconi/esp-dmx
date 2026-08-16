#include "artnet_receiver.h"
#include "config.h"
#include "storage.h"
#include "dmx_controller.h"

#include <ArtnetWifi.h>

namespace ArtnetReceiver {

namespace {

ArtnetWifi artnet;

bool g_enabled = ARTNET_ENABLED_DEFAULT;
uint16_t g_universe = ARTNET_UNIVERSE_DEFAULT;

volatile bool g_dirty = false;
volatile uint32_t g_lastPacketMs = 0;
String g_lastSenderIp;

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t /*sequence*/, uint8_t *data) {
  if (!g_enabled) return;
  if (universe != g_universe) return;

  DmxController::setChannels(1, data, length);
  g_lastPacketMs = millis();
  g_lastSenderIp = artnet.getSenderIp().toString();
  g_dirty = true;
}

} // namespace

void begin() {
  g_enabled = Storage::artnetEnabled();
  g_universe = Storage::artnetUniverse();

  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();
}

void poll() { artnet.read(); }

bool enabled() { return g_enabled; }

void setEnabled(bool on) {
  g_enabled = on;
  Storage::setArtnetConfig(g_enabled, g_universe);
}

uint16_t universe() { return g_universe; }

void setUniverse(uint16_t u) {
  g_universe = u;
  Storage::setArtnetConfig(g_enabled, g_universe);
}

bool consumeDirty() {
  if (!g_dirty) return false;
  g_dirty = false;
  return true;
}

uint32_t lastPacketMs() { return g_lastPacketMs; }

String lastSenderIp() { return g_lastSenderIp; }

} // namespace ArtnetReceiver
