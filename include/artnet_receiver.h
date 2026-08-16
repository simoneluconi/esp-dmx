#pragma once

#include <Arduino.h>

// Wraps the ArtnetWifi library to turn incoming Art-Net packets into DMX
// output. Always listens on the network once begin() is called, but only
// applies received data to DmxController while enabled() is true — so
// toggling it off fully hands control back to the local web UI without
// needing to tear down the UDP socket.
namespace ArtnetReceiver {

// Loads the persisted enabled/universe state and starts listening for
// Art-Net packets. Call after Wi-Fi is up.
void begin();

// Call every loop() iteration; processes any pending Art-Net packet.
void poll();

bool enabled();
void setEnabled(bool on);  // updates live behavior and persists to flash

uint16_t universe();
void setUniverse(uint16_t u);  // updates live behavior and persists to flash

// True once after new Art-Net data has been applied since the last call;
// meant for a throttled "should I broadcast the new state over WS?" check.
bool consumeDirty();

// millis() timestamp of the last accepted packet, or 0 if none yet.
uint32_t lastPacketMs();

// IP address text of the sender of the last accepted packet, or "" if none yet.
String lastSenderIp();

} // namespace ArtnetReceiver
