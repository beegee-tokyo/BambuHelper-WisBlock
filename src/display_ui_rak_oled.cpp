#ifdef _VARIANT_RAK3112_
#ifdef _RAK1921_
#include <Arduino.h>
#include "display_ui.h"
#include "display_gauges.h"
#include "display_anim.h"
#include "clock_mode.h"
#include "clock_pong.h"
#include "icons.h"
#include "config.h"
#include "bambu_state.h"
#include "bambu_mqtt.h"
#include "settings.h"
#include <WiFi.h>
#include <time.h>
#include <nRF_SSD1306Wire.h>

/** Width of the display in pixel */
#define OLED_WIDTH 128
/** Height of the display in pixel */
#define OLED_HEIGHT 64
/** Height of the status bar in pixel */
#define STATUS_BAR_HEIGHT 11
/** Height of a single line */
#define LINE_HEIGHT 10

/** Number of message lines */
#define NUM_OF_LINES (OLED_HEIGHT - STATUS_BAR_HEIGHT) / LINE_HEIGHT

/** Line buffer for messages */
char disp_buffer[NUM_OF_LINES + 1][32] = {0};

/** Helper for lines */
char helper[128];

/** Current line used */
uint8_t current_line = 0;

/** Display class using Wire */
SSD1306Wire oled_display(0x3c, PIN_WIRE_SDA, PIN_WIRE_SCL, GEOMETRY_128_64, &Wire);
void rak1921_write_header(char *header_line);
void rak1921_draw_line(int32_t line, char *content);
void rak1921_clear_lines(void);

static ScreenState currentScreen = SCREEN_SPLASH;
static ScreenState prevScreen = SCREEN_SPLASH;
static bool forceRedraw = true;
static unsigned long lastDisplayUpdate = 0;

// Previous state for smart redraw
static BambuState prevState;
static unsigned long connectScreenStart = 0;

// ---------------------------------------------------------------------------
//  Backlight
// ---------------------------------------------------------------------------
void setBacklight(uint8_t level) {
}

// ---------------------------------------------------------------------------
//  Init
// ---------------------------------------------------------------------------
void initDisplay() {
  Wire.begin();

  Serial.println("Display: pre-init delay...");
  delay(500); // Give display reset some time
  oled_display.setI2cAutoInit(true);
  Serial.println("Display: calling oled_display.init()...");
  Serial.flush();
  oled_display.init();
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  Serial.println("Display: oled_display.init() done");
  if (dispSettings.rotation != 2) {
    oled_display.flipScreenVertically();
    Serial.println("Display: setRotation done");
  }
  oled_display.setContrast(128);
  oled_display.setFont(ArialMT_Plain_10);
  oled_display.display();

  memset(&prevState, 0, sizeof(prevState));

  // Splash screen
  rak1921_write_header((char *)"BambuHelper Monitor");
  rak1921_draw_line(1, (char *)"Printer Monitor");
  rak1921_draw_line(2, (char *)FW_VERSION);
}

void applyDisplaySettings() {
  if (dispSettings.rotation != 2) {
    oled_display.flipScreenVertically();
    Serial.println("Display: setRotation done");
  }
}

void triggerDisplayTransition() {
  // Clear previous state so everything redraws for the new printer
  memset(&prevState, 0, sizeof(prevState));
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  rak1921_draw_line(1, (char *)"Printer Monitor");
  rak1921_draw_line(2, (char *)FW_VERSION);
  forceRedraw = true;
}

void setScreenState(ScreenState state) {
  currentScreen = state;
}

ScreenState getScreenState() {
  return currentScreen;
}

// ---------------------------------------------------------------------------
//  Nozzle label helper (dual nozzle H2D/H2C)
// ---------------------------------------------------------------------------
static const char *nozzleLabel(const BambuState &s) {
  if (!s.dualNozzle)
    return "Nozzle";
  return s.activeNozzle == 0 ? "Nozzle R" : "Nozzle L";
}

// ---------------------------------------------------------------------------
//  Speed level name helper
// ---------------------------------------------------------------------------
static const char *speedLevelName(uint8_t level) {
  switch (level) {
  case 1:
    return "Sil";
  case 2:
    return "Std";
  case 3:
    return "Fst";
  case 4:
    return "Lud";
  default:
    return "---";
  }
}

// ---------------------------------------------------------------------------
//  Screen: AP Mode
// ---------------------------------------------------------------------------
static void drawAPMode() {
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  rak1921_draw_line(1, (char *)"WiFi Setup");

  // Instructions
  rak1921_draw_line(2, (char *)"Connect to WiFi:");

  // AP SSID
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  snprintf(helper, sizeof(helper), "%s%04X", WIFI_AP_PREFIX, mac);
  rak1921_draw_line(3, helper);

  // Password
  snprintf(helper, sizeof(helper), "PW: %s", WIFI_AP_PASSWORD);
  rak1921_draw_line(4, helper);

  // IP
  rak1921_draw_line(5, (char *)"Open: 192.168.4.1");
}

// ---------------------------------------------------------------------------
//  Screen: Connecting WiFi
// ---------------------------------------------------------------------------
static void drawConnectingWiFi() {
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  // Title
  rak1921_draw_line(1, (char *)"Connecting to WiFi");
}

// ---------------------------------------------------------------------------
//  Screen: WiFi Connected (show IP)
// ---------------------------------------------------------------------------
static void drawWiFiConnected() {
  if (!forceRedraw)
    return;

  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  // Title
  rak1921_draw_line(1, (char *)"WiFi Connected");
}

// ---------------------------------------------------------------------------
//  Screen: Connecting MQTT
// ---------------------------------------------------------------------------
static void drawConnectingMQTT() {
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  // Title
  rak1921_draw_line(1, (char *)"Connecting to Printer");

  // Connection mode + printer info
  PrinterSlot &p = displayedPrinter();

  const char *modeStr = isCloudMode(p.config.mode) ? "Cloud" : "LAN";
  char infoBuf[40];
  if (isCloudMode(p.config.mode)) {
    snprintf(helper, sizeof(helper), "[%s] %s", modeStr,
             strlen(p.config.serial) > 0 ? p.config.serial : "no serial!");
  } else {
    snprintf(helper, sizeof(helper), "[%s] %s", modeStr,
             strlen(p.config.ip) > 0 ? p.config.ip : "no IP!");
  }
  rak1921_draw_line(2, helper);

  // Elapsed time
  if (connectScreenStart > 0) {
    unsigned long elapsed = (millis() - connectScreenStart) / 1000;
    snprintf(helper, sizeof(helper), "%lus", elapsed);
    rak1921_draw_line(3, helper);
  }

  // Diagnostics (only after first attempt)
  const MqttDiag &d = getMqttDiag(rotState.displayIndex);
  if (d.attempts > 0) {
    snprintf(helper, sizeof(helper), "Attempt: %u", d.attempts);
    rak1921_draw_line(4, helper);

    if (d.lastRc != 0) {
      snprintf(helper, sizeof(helper), "Err: %s", mqttRcToString(d.lastRc));
      rak1921_draw_line(5, helper);
    }
  }
}

// ---------------------------------------------------------------------------
//  Screen: Idle (connected, not printing)
// ---------------------------------------------------------------------------
static void drawIdleNoPrinter() {
  if (!forceRedraw)
    return;

  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();
  rak1921_write_header((char *)"BambuHelper Monitor");
  // Title
  rak1921_draw_line(1, (char *)"WiFi Connected");

  rak1921_draw_line(2, (char *)"No printer configured");
  rak1921_draw_line(3, (char *)"Open in browser:");
  sprintf(helper, "%s", WiFi.localIP().toString().c_str());
  rak1921_draw_line(4, helper);
}

static bool wasNoPrinter = false;

static void drawIdle() {
  if (!isAnyPrinterConfigured()) {
    wasNoPrinter = true;
    drawIdleNoPrinter();
    return;
  }

  // Transition from "no printer" to configured — clear stale screen
  if (wasNoPrinter) {
    wasNoPrinter = false;
    oled_display.displayOff();
    oled_display.clear();
    oled_display.displayOn();
    memset(&prevState, 0, sizeof(prevState));
    forceRedraw = true;
  }

  PrinterSlot &p = displayedPrinter();
  BambuState &s = p.state;

  bool stateChanged = forceRedraw || (strcmp(s.gcodeState, prevState.gcodeState) != 0);
  bool tempChanged = forceRedraw ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);
  bool connChanged = forceRedraw || (s.connected != prevState.connected);
  bool wifiChanged = forceRedraw || (s.wifiSignal != prevState.wifiSignal);

  // Printer name (only on forceRedraw — name doesn't change)
  if (forceRedraw) {
    const char *name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    snprintf(helper, sizeof(helper), "BambuHelper %s", name);
    rak1921_write_header(helper);
  }

  // Status badge
  const char *stateStr = s.gcodeState;
  if (strcmp(s.gcodeState, "IDLE") == 0) {
    rak1921_draw_line(1, (char *)"Ready");
  }
  else if (strcmp(s.gcodeState, "FAILED") == 0) {
    rak1921_draw_line(1, (char *)"ERROR");
  }
  else if (strcmp(s.gcodeState, "UNKNOWN") == 0 || s.gcodeState[0] == '\0') {
    rak1921_draw_line(1, (char *)"Waiting...");
  }

  // Print name
  if (s.subtaskName[0] != '\0') {
    char truncName[26];
    strncpy(truncName, s.subtaskName, 25);
    truncName[25] = '\0';
    snprintf(helper, sizeof(helper), "%s %s", truncName, s.gcodeState);
    rak1921_draw_line(2, helper);
  }

  // Nozzle temp gauge
  if (tempChanged) {
    snprintf(helper, sizeof(helper), "Nozzle: %.1f C", s.nozzleTemp);
    rak1921_draw_line(3, helper);
    snprintf(helper, sizeof(helper), "Bed: %.1f C", s.bedTemp);
    rak1921_draw_line(4, helper);
  }

  // Bottom: filament indicator or WiFi signal
  if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS && s.ams.trays[s.ams.activeTray].present) {
    AmsTray &t = s.ams.trays[s.ams.activeTray];
    snprintf(helper, sizeof(helper), "AMS: %s  WiFi: %d dBm", t.type, s.wifiSignal);
    rak1921_draw_line(5, helper);
  } else {
    snprintf(helper, sizeof(helper), "WiFi: %d dBm", s.wifiSignal);
    rak1921_draw_line(5, helper);
  }
}

// ---------------------------------------------------------------------------
//  Screen: Printing (main dashboard)
//  Layout: LED bar | header | 2x3 gauge grid | info line
// ---------------------------------------------------------------------------
static void drawPrinting() {
  PrinterSlot &p = displayedPrinter();
  BambuState &s = p.state;

  // bool animating = tickGaugeSmooth(s, forceRedraw);
  bool progChanged = forceRedraw || (s.progress != prevState.progress);
  bool tempChanged = forceRedraw ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);
  bool etaChanged = forceRedraw ||
                    (s.remainingMinutes != prevState.remainingMinutes);
  bool fansChanged = forceRedraw ||
                     (s.coolingFanPct != prevState.coolingFanPct) ||
                     (s.auxFanPct != prevState.auxFanPct) ||
                     (s.chamberFanPct != prevState.chamberFanPct);
  bool stateChanged = forceRedraw ||
                      (strcmp(s.gcodeState, prevState.gcodeState) != 0);
  // WiFi signal only matters when AMS indicator is not shown
  bool showingWifi = !(s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS && s.ams.trays[s.ams.activeTray].present) && !(s.ams.vtPresent && s.ams.activeTray == 254);
  bool bottomChanged = forceRedraw ||
                       (s.speedLevel != prevState.speedLevel) ||
                       (s.layerNum != prevState.layerNum) ||
                       (s.totalLayers != prevState.totalLayers) ||
                       (s.ams.activeTray != prevState.ams.activeTray) ||
                       (showingWifi && s.wifiSignal != prevState.wifiSignal);

  if (forceRedraw || progChanged || tempChanged || etaChanged || fansChanged || stateChanged || bottomChanged || showingWifi) {
    const char *name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    snprintf(helper, sizeof(helper), "BambuHelper %s", name);
    rak1921_write_header(helper);

    // Print name
    if (s.subtaskName[0] != '\0') {
      char truncName[26];
      strncpy(truncName, s.subtaskName, 25);
      truncName[25] = '\0';
      snprintf(helper, sizeof(helper), "%s %s", truncName, s.gcodeState);
      rak1921_draw_line(1, helper);
    } else {
      rak1921_draw_line(1, s.gcodeState);
    }

    snprintf(helper, sizeof(helper), "N: %.1f C B: %.1f C", s.nozzleTemp, s.bedTemp);
    rak1921_draw_line(2, helper);
    snprintf(helper, sizeof(helper), "P: %.0f%% A: %.0f%% C: %.0f%%", s.coolingFanPct, s.auxFanPct, s.chamberFanPct);
    rak1921_draw_line(3, helper);

    if (strcmp(s.gcodeState, "PAUSE") == 0) {
      // // Prominent PAUSE alert
    }
    else if (strcmp(s.gcodeState, "FAILED") == 0) {
      // // Prominent ERROR alert
    }
    else if (s.remainingMinutes > 0) {
      // Use time() directly - avoids getLocalTime() race condition with timeout 0.
      // Once NTP syncs the RTC keeps running; ntpSynced latches true forever.
      static bool ntpSynced = false;
      time_t nowEpoch = time(nullptr);
      struct tm now;
      localtime_r(&nowEpoch, &now);
      if (now.tm_year > (2020 - 1900))
        ntpSynced = true;

      if (ntpSynced) {
        // Calculate ETA: current time + remaining minutes
        time_t etaEpoch = nowEpoch + (time_t)s.remainingMinutes * 60;
        struct tm etaTm;
        localtime_r(&etaEpoch, &etaTm);

        char etaBuf[32];
        int etaH = etaTm.tm_hour;
        const char *ampm = "";
        if (!netSettings.use24h) {
          ampm = etaH < 12 ? "AM" : "PM";
          etaH = etaH % 12;
          if (etaH == 0)
            etaH = 12;
        }
        // Show date only if finish is not today
        if (etaTm.tm_yday != now.tm_yday || etaTm.tm_year != now.tm_year) {
          if (netSettings.use24h)
            snprintf(etaBuf, sizeof(etaBuf), "ETA: %d.%02d %02d:%02d",
                     etaTm.tm_mday, etaTm.tm_mon + 1, etaH, etaTm.tm_min);
          else
            snprintf(etaBuf, sizeof(etaBuf), "ETA: %d.%02d %d:%02d%s",
                     etaTm.tm_mday, etaTm.tm_mon + 1, etaH, etaTm.tm_min, ampm);
        } else {
          if (netSettings.use24h)
            snprintf(etaBuf, sizeof(etaBuf), "ETA: %02d:%02d - %02d min", etaH, etaTm.tm_min, s.remainingMinutes);
          else
            snprintf(etaBuf, sizeof(etaBuf), "ETA: %d:%02d %s - %02d min", etaH, etaTm.tm_min, ampm, s.remainingMinutes);
        }
        rak1921_draw_line(4, etaBuf);
      } else {
        // NTP not synced yet - show remaining time only
        char remBuf[24];
        uint16_t h = s.remainingMinutes / 60;
        uint16_t m = s.remainingMinutes % 60;
        snprintf(remBuf, sizeof(remBuf), "Remaining: %dh %02dm", h, m);
        rak1921_draw_line(4, remBuf);
      }
    } else {
      rak1921_draw_line(4, (char *)"---");
    }
    if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS) {
      AmsTray &t = s.ams.trays[s.ams.activeTray];
      if (t.present) {
        sprintf(helper, "A: %s L%d/%d %s", t.type, s.layerNum, s.totalLayers, speedLevelName(s.speedLevel));
        rak1921_draw_line(5, helper);
      } else {
        sprintf(helper, "L%d/%d %s", s.layerNum, s.totalLayers, speedLevelName(s.speedLevel));
        rak1921_draw_line(5, helper);
      }
    }
    else if (s.ams.vtPresent && s.ams.activeTray == 254) {
      sprintf(helper, "A: %s L%d/%d %s", s.ams.vtType, s.layerNum, s.totalLayers, speedLevelName(s.speedLevel));
      rak1921_draw_line(5, helper);
    } else {
      sprintf(helper, "L%d/%d %s", s.layerNum, s.totalLayers, speedLevelName(s.speedLevel));
      rak1921_draw_line(5, helper);
    }
  }
}

// ---------------------------------------------------------------------------
//  Screen: Finished (same layout as printing, but with 2 gauges + status)
// ---------------------------------------------------------------------------
static void drawFinished() {
  PrinterSlot &p = displayedPrinter();
  BambuState &s = p.state;

  // bool animating = tickGaugeSmooth(s, forceRedraw);
  bool tempChanged = forceRedraw ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);

  // === Header bar (y=7-25) — same as printing screen ===
  if (forceRedraw || tempChanged) {
    const char *name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    snprintf(helper, sizeof(helper), "BambuHelper %s", name);
    rak1921_write_header(helper);

    // FINISH badge (right)
    rak1921_draw_line(1, (char *)"PRINT COMPLETED");
    if (s.subtaskName[0] != '\0') {
      char truncName[26];
      strncpy(truncName, s.subtaskName, 25);
      truncName[25] = '\0';
      rak1921_draw_line(2, truncName);
    }

    // Temperatures and fans
    snprintf(helper, sizeof(helper), "N: %.1f B: %.1f", s.nozzleTemp, s.bedTemp);
    rak1921_draw_line(3, helper);
    snprintf(helper, sizeof(helper), "P: %.1f%% A: %.1f%% C: %.1f%%", s.coolingFanPct, s.auxFanPct, s.chamberFanPct);
    rak1921_draw_line(4, helper);

    snprintf(helper, sizeof(helper), "WiFi: %d dBm", s.wifiSignal);
    rak1921_draw_line(5, helper);
  }
}

// ---------------------------------------------------------------------------
//  Main update (called from loop)
// ---------------------------------------------------------------------------
void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_MS)
    return;
  lastDisplayUpdate = now;

  // Detect screen change
  if (currentScreen != prevScreen) {
    forceRedraw = true;
    if (currentScreen == SCREEN_CONNECTING_MQTT) {
      connectScreenStart = millis();
    }
    if (currentScreen == SCREEN_CLOCK) {
      resetClock();
    }
    prevScreen = currentScreen;

    rak1921_clear_lines();
  }

  switch (currentScreen) {
  case SCREEN_SPLASH:
    // Splash shown in initDisplay(), auto-advance handled by main.cpp
    break;

  case SCREEN_AP_MODE:
    if (forceRedraw)
      drawAPMode();
    break;

  case SCREEN_CONNECTING_WIFI:
    drawConnectingWiFi();
    break;

  case SCREEN_WIFI_CONNECTED:
    drawWiFiConnected();
    break;

  case SCREEN_CONNECTING_MQTT:
    drawConnectingMQTT();
    break;

  case SCREEN_IDLE:
    drawIdle();
    break;

  case SCREEN_PRINTING:
    drawPrinting();
    break;

  case SCREEN_FINISHED:
    drawFinished();
    break;

  case SCREEN_CLOCK:
    if (!dispSettings.pongClock)
      drawClock();
    // Pong clock is ticked before the throttle (above)
    break;

  case SCREEN_OFF:
    drawClock();
    break;
  }

  // Save state for next smart-redraw comparison
  memcpy(&prevState, &displayedPrinter().state, sizeof(BambuState));
  forceRedraw = false;
}

/**
 * @brief Write the top line of the display
 */
void rak1921_write_header(char *header_line) {
  oled_display.setFont(ArialMT_Plain_10);

  // clear the status bar
  oled_display.setColor(BLACK);
  oled_display.fillRect(0, 0, OLED_WIDTH, STATUS_BAR_HEIGHT);

  oled_display.setColor(WHITE);
  oled_display.setTextAlignment(TEXT_ALIGN_LEFT);

  oled_display.drawString(0, 0, header_line);

  // draw divider line
  oled_display.drawLine(0, 11, 128, 11);
  oled_display.display();
}

/**
 * @brief Write the top line of the display
 */
void rak1921_clear_lines(void) {
  // clear below the status bar
  oled_display.setColor(BLACK);
  oled_display.fillRect(0, STATUS_BAR_HEIGHT + 1, OLED_WIDTH, OLED_HEIGHT - STATUS_BAR_HEIGHT + 1);

  oled_display.setColor(WHITE);
}

/**
 * @brief Write a line to the display buffer
 *
 * @param line Line to write to
 * @param content Pointer to char array with the new line
 */
void rak1921_draw_line(int32_t line, char *content) {
  line -= 1;
  oled_display.setFont(ArialMT_Plain_10);
  oled_display.setColor(BLACK);
  oled_display.fillRect(0, (line * LINE_HEIGHT) + STATUS_BAR_HEIGHT + 1, OLED_WIDTH, 10);
  oled_display.setFont(ArialMT_Plain_10);
  oled_display.setColor(WHITE);
  oled_display.setTextAlignment(TEXT_ALIGN_LEFT);
  oled_display.drawString(0, (line * LINE_HEIGHT) + STATUS_BAR_HEIGHT + 1, content);
  oled_display.display();
}

#endif
#endif