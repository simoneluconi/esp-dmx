#pragma once

#include <Arduino.h>

// Wrapper around the esp_dmx library that owns:
//  - initialization of UART1 in DMX-512 mode
//  - a shared buffer (protected by a mutex) holding the 512 channels
//  - a dedicated FreeRTOS task that continuously transmits the DMX packet
namespace DmxController {

// Starts the DMX UART and creates the continuous transmit task.
void begin();

// Sets the value (0-255) of a single channel (1-512). Out of range = ignored.
void setChannel(uint16_t channel, uint8_t value);

// Sets a contiguous block of channels starting at startChannel (1-512).
void setChannels(uint16_t startChannel, const uint8_t *values, uint16_t count);

// Reads the current value of a channel (1-512). Out of range = 0.
uint8_t getChannel(uint16_t channel);

// Copies the whole universe (512 channels, index 0 = channel 1) into `out`.
void getAllChannels(uint8_t *out);

// Sets every channel to zero (blackout).
void blackout();

// Sets every channel to full (255) — handy for quick tests.
void fullOn();

} // namespace DmxController
