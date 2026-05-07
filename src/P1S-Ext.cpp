#include "P1S_Ext.h"
#include "settings.h"
#include "wifi_manager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define P1S_EXT_TIMEOUT_MS 3000 // 1500			 // HTTP timeout when plug is known-online
#define P1S_EXT_TIMEOUT_FAST_MS 3000 // 700		 // shorter timeout once plug is confirmed offline
#define P1S_EXT_STALE_MS 90000UL		 // consider offline after 90s without data
#define P1S_EXT_OFFLINE_RETRY_MS 10000UL // retry quickly after confirmed offline
#define P1S_EXT_DEFAULT_INTERVAL 10		 // seconds, used when pollInterval not set
#define P1S_EXT_FAILS_BEFORE_OFFLINE 3	 // tolerate a few transient misses before hiding watts

static volatile uint32_t g_lastUpdateP1S_ext_Ms = 0;
static volatile bool g_P1S_ext_Offline = false;
static volatile uint8_t g_P1S_Ext_failCount = 0;

TasmotaSettings P1S_Ext_Settings = {true, "192.168.0.74", 0, 30, 255};

static TaskHandle_t g_taskHandleP1S = NULL;

static void P1S_ext_markPollFailure()
{
	if (g_P1S_Ext_failCount < 255)
		g_P1S_Ext_failCount++;
	if (g_P1S_Ext_failCount >= P1S_EXT_FAILS_BEFORE_OFFLINE)
	{
		g_P1S_ext_Offline = true;
	}
}

static void P1S_ext_doPoll()
{
	if (!P1S_Ext_Settings.enabled || P1S_Ext_Settings.ip[0] == '\0')
		return;

	char url[64];
	snprintf(url, sizeof(url), "http://%s/api/data", P1S_Ext_Settings.ip);

	HTTPClient http;
	http.setTimeout(g_P1S_ext_Offline ? P1S_EXT_TIMEOUT_FAST_MS : P1S_EXT_TIMEOUT_MS);
	if (!http.begin(url))
	{
		Serial.printf("[P1S sensor] begin failed: %s\n", url);
		P1S_ext_markPollFailure();
		return;
	}

	int code = http.GET();
	if (code != 200)
	{
		Serial.printf("[P1S sensor] HTTP %d from %s\n", code, P1S_Ext_Settings.ip);
		http.end();
		P1S_ext_markPollFailure();
		return;
	}

	String body = http.getString();
	http.end();

	// Serial.printf("[P1S sensor] Received: %s\n", body.c_str());

	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, body);
	if (err)
	{
		Serial.printf("[P1S sensor] JSON parse error: %s\n", err.c_str());
		P1S_ext_markPollFailure();
		return;
	}

	BambuState &s = printers[0].state;

	JsonVariant p1s_temp = doc["P1S-Ext"]["Temperature"];
	JsonVariant p1s_humid = doc["P1S-Ext"]["Humidity"];
	JsonVariant p1s_door = doc["P1S-Ext"]["Door"];

	if (p1s_temp.isNull())
	{
		Serial.println("[P1S sensor] Temperature field missing");
		P1S_ext_markPollFailure();
		// return;
	}
	else
	{
		s.chamberTemp = p1s_temp.as<float>();
	}
	if (p1s_humid.isNull())
	{
		Serial.println("[P1S sensor] Humidity field missing");
		P1S_ext_markPollFailure();
		//   return;
	}

	if (p1s_door.isNull())
	{
		Serial.println("[P1S sensor] Door field missing");
		P1S_ext_markPollFailure();
		//   return;
	}
	else
	{
		s.doorSensorPresent = true;
		String sensor = doc["P1S-Ext"]["Door"].as<String>();
		// Serial.printf("Door status %s\n", sensor.c_str());
		s.doorOpen = sensor == "OPEN" ? true : false;
	}
	// Serial.printf("[P1S sensor] Temperature=%.2f, Humidity=%.2f, Door=%s\n",
	// 			  p1s_temp.as<float>(), p1s_humid.as<float>(), s.doorOpen ? "OPEN" : "CLOSED");
}

static void P1S_ext_pollTask(void *)
{
	while (true)
	{
		if (isWiFiConnected())
		{
			P1S_ext_doPoll();
		}
		uint32_t intervalMs = g_P1S_ext_Offline
								  ? P1S_EXT_OFFLINE_RETRY_MS
								  : (uint32_t)(P1S_Ext_Settings.pollInterval > 0
												   ? P1S_Ext_Settings.pollInterval
												   : P1S_EXT_DEFAULT_INTERVAL) *
										1000;
		vTaskDelay(pdMS_TO_TICKS(intervalMs));
	}
}

void P1S_ext_Init()
{
	if (g_taskHandleP1S != NULL)
	{
		vTaskSuspend(g_taskHandleP1S);
		vTaskDelete(g_taskHandleP1S);
		g_taskHandleP1S = NULL;
	}

	// Use Tasmota IP for now
	strcpy(P1S_Ext_Settings.ip, tasmotaSettings.ip);
	P1S_Ext_Settings.enabled = true;

	Serial.printf("Using %s/api/data\n", P1S_Ext_Settings.ip);
	g_lastUpdateP1S_ext_Ms = 0;
	g_P1S_ext_Offline = false;
	g_P1S_Ext_failCount = 0;

	if (P1S_Ext_Settings.enabled && P1S_Ext_Settings.ip[0] != '\0')
	{
		Serial.println("Starting task for P1S Extension");
		xTaskCreate(P1S_ext_pollTask, "P1S-Ext", 6144, NULL, 1, &g_taskHandleP1S);
	}
}