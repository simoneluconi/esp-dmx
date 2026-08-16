#include "dmx_controller.h"
#include "config.h"

#include <esp_dmx.h>

namespace DmxController {

namespace {

const dmx_port_t kPort = static_cast<dmx_port_t>(DMX_PORT_NUM);

// data[0] is the DMX start code (always 0 for "null start code"),
// channels 1..512 live in data[1..512].
uint8_t dmxData[DMX_PACKET_SIZE];
SemaphoreHandle_t dataMutex = nullptr;
TaskHandle_t txTaskHandle = nullptr;

void txTask(void *) {
  uint8_t localBuf[DMX_PACKET_SIZE];

  for (;;) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      memcpy(localBuf, dmxData, DMX_PACKET_SIZE);
      xSemaphoreGive(dataMutex);
    }

    dmx_write(kPort, localBuf, DMX_PACKET_SIZE);
    dmx_send(kPort);
    dmx_wait_sent(kPort, DMX_TIMEOUT_TICK);

    vTaskDelay(pdMS_TO_TICKS(DMX_REFRESH_MS));
  }
}

} // namespace

void begin() {
  memset(dmxData, 0, sizeof(dmxData));
  dataMutex = xSemaphoreCreateMutex();

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {{1, "Default Personality"}};
  if (!dmx_driver_install(kPort, &config, personalities, 1)) {
    Serial.println("[DMX] ERROR: dmx_driver_install failed, DMX output is disabled");
  }
  if (!dmx_set_pin(kPort, DMX_TX_PIN, DMX_RX_PIN, DMX_EN_PIN)) {
    Serial.println("[DMX] ERROR: dmx_set_pin failed, check DMX_TX_PIN/DMX_RX_PIN/DMX_EN_PIN");
  }

  xTaskCreatePinnedToCore(txTask, "dmx_tx", 4096, nullptr, 5, &txTaskHandle, 0);
}

void setChannel(uint16_t channel, uint8_t value) {
  if (channel < 1 || channel > DMX_UNIVERSE_SIZE) return;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    dmxData[channel] = value;
    xSemaphoreGive(dataMutex);
  }
}

void setChannels(uint16_t startChannel, const uint8_t *values, uint16_t count) {
  if (startChannel < 1 || startChannel > DMX_UNIVERSE_SIZE) return;
  uint16_t maxCount = DMX_UNIVERSE_SIZE - startChannel + 1;
  if (count > maxCount) count = maxCount;

  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    memcpy(&dmxData[startChannel], values, count);
    xSemaphoreGive(dataMutex);
  }
}

uint8_t getChannel(uint16_t channel) {
  if (channel < 1 || channel > DMX_UNIVERSE_SIZE) return 0;
  uint8_t v = 0;
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    v = dmxData[channel];
    xSemaphoreGive(dataMutex);
  }
  return v;
}

void getAllChannels(uint8_t *out) {
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    memcpy(out, &dmxData[1], DMX_UNIVERSE_SIZE);
    xSemaphoreGive(dataMutex);
  }
}

void blackout() {
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    memset(&dmxData[1], 0, DMX_UNIVERSE_SIZE);
    xSemaphoreGive(dataMutex);
  }
}

void fullOn() {
  if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
    memset(&dmxData[1], 255, DMX_UNIVERSE_SIZE);
    xSemaphoreGive(dataMutex);
  }
}

} // namespace DmxController
