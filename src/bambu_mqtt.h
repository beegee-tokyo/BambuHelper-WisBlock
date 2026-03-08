#ifndef BAMBU_MQTT_H
#define BAMBU_MQTT_H

#include <Arduino.h>

// Connection diagnostics exposed for display/web
struct MqttDiag {
  int      lastRc;          // PubSubClient state code from last attempt
  uint32_t attempts;        // total reconnect attempts since boot
  uint32_t messagesRx;      // total MQTT messages received
  uint32_t freeHeap;        // heap at last attempt
  bool     tcpOk;           // last TCP reachability result
  unsigned long lastAttemptMs; // millis() of last attempt
  unsigned long connectDurMs;  // how long last connect() took
};

extern bool mqttDebugLog;   // verbose Serial logging (toggled via web)

void initBambuMqtt();
void handleBambuMqtt();
void disconnectBambuMqtt();
bool isPrinterConfigured();
const MqttDiag& getMqttDiag();

// Human-readable error string for PubSubClient rc
const char* mqttRcToString(int rc);

#endif // BAMBU_MQTT_H
