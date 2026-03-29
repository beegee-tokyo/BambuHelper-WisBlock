#include "tasmota.h"
#include "settings.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define TASMOTA_TIMEOUT_MS  3000      // HTTP connect+read timeout
#define TASMOTA_STALE_MS    90000UL   // consider offline after 90s without data

static float    g_watts             = -1.0f;
static float    g_todayKwh          = -1.0f;  // kWh today from Tasmota
static float    g_yesterdayKwh      = -1.0f;  // kWh yesterday from Tasmota
static float    g_printStartTodayKwh = -1.0f; // Today at print start
static float    g_printUsedKwh      = -1.0f;  // frozen session kWh at print end
static uint32_t g_lastUpdateMs      = 0;
static uint32_t g_nextPollMs        = 0;
static bool     g_kwhChanged        = false;

void tasmotaInit() {
  g_watts              = -1.0f;
  g_todayKwh           = -1.0f;
  g_yesterdayKwh       = -1.0f;
  g_printStartTodayKwh = -1.0f;
  g_printUsedKwh       = -1.0f;
  g_lastUpdateMs       = 0;
  g_nextPollMs         = 0;
  g_kwhChanged         = false;
}

static void doPoll() {
  if (!tasmotaSettings.enabled || tasmotaSettings.ip[0] == '\0') return;

  char url[64];
  snprintf(url, sizeof(url), "http://%s/cm?cmnd=Status%%2010", tasmotaSettings.ip);

  HTTPClient http;
  http.setTimeout(TASMOTA_TIMEOUT_MS);
  if (!http.begin(url)) {
    Serial.printf("[Tasmota] begin failed: %s\n", url);
    return;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Tasmota] HTTP %d from %s\n", code, tasmotaSettings.ip);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[Tasmota] JSON parse error: %s\n", err.c_str());
    return;
  }

  JsonVariant energy = doc["StatusSNS"]["ENERGY"];
  if (energy.isNull()) energy = doc["ENERGY"];
  if (energy.isNull()) {
    Serial.println("[Tasmota] ENERGY object missing in response");
    return;
  }

  JsonVariant power     = energy["Power"];
  JsonVariant today     = energy["Today"];
  JsonVariant yesterday = energy["Yesterday"];

  if (power.isNull()) {
    Serial.println("[Tasmota] Power field missing");
    return;
  }

  g_watts        = power.as<float>();
  g_lastUpdateMs = millis();

  if (!today.isNull()) {
    float newToday = today.as<float>();
    if (newToday != g_todayKwh) {
      g_todayKwh   = newToday;
      g_kwhChanged = true;
    }
  }

  if (!yesterday.isNull()) {
    g_yesterdayKwh = yesterday.as<float>();
  }

  Serial.printf("[Tasmota] Power=%.0fW, Today=%.3fkWh, Yesterday=%.3fkWh\n",
                g_watts, g_todayKwh, g_yesterdayKwh);
}

void tasmotaLoop(unsigned long now) {
  if (!tasmotaSettings.enabled) return;
  if ((uint32_t)now >= g_nextPollMs) {
    doPoll();
    uint32_t intervalMs = (uint32_t)tasmotaSettings.pollInterval * 1000UL;
    g_nextPollMs = (uint32_t)now + intervalMs;
  }
}

float tasmotaGetWatts() {
  return g_watts;
}

bool tasmotaIsOnline() {
  if (!tasmotaSettings.enabled) return false;
  if (g_lastUpdateMs == 0) return false;
  return (millis() - g_lastUpdateMs) < TASMOTA_STALE_MS;
}

bool tasmotaIsActiveForSlot(uint8_t slot) {
  if (!tasmotaIsOnline()) return false;
  return (tasmotaSettings.assignedSlot == 255 || tasmotaSettings.assignedSlot == slot);
}

void tasmotaMarkPrintStart() {
  g_printStartTodayKwh = g_todayKwh;  // -1 if not yet polled
  g_printUsedKwh       = -1.0f;
  Serial.printf("[Tasmota] Print start marked, Today=%.3fkWh\n", g_printStartTodayKwh);
}

void tasmotaMarkPrintEnd() {
  if (g_printStartTodayKwh < 0.0f || g_todayKwh < 0.0f) {
    Serial.println("[Tasmota] Print end: no baseline, skipping");
    return;
  }

  if (g_todayKwh >= g_printStartTodayKwh) {
    // Normal case: no midnight reset during print
    g_printUsedKwh = g_todayKwh - g_printStartTodayKwh;
  } else if (g_yesterdayKwh >= 0.0f) {
    // Midnight passed: Yesterday holds the final Today value before reset
    g_printUsedKwh = (g_yesterdayKwh - g_printStartTodayKwh) + g_todayKwh;
  }

  Serial.printf("[Tasmota] Print end marked, used=%.3fkWh\n", g_printUsedKwh);
}

float tasmotaGetPrintKwhUsed() {
  return g_printUsedKwh;
}

bool tasmotaKwhChanged() {
  if (!g_kwhChanged) return false;
  g_kwhChanged = false;
  return true;
}
