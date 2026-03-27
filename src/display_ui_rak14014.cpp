#ifdef _VARIANT_RAK3112_
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

TFT_eSPI tft = TFT_eSPI();

// Screen width and height changes depending on rotation
int32_t use_width = SCREEN_W;
int32_t use_height = SCREEN_H;

// Use user-configured bg color instead of hardcoded CLR_BG
#undef  CLR_BG
#define CLR_BG  (dispSettings.bgColor)

static ScreenState currentScreen = SCREEN_SPLASH;
static ScreenState prevScreen = SCREEN_SPLASH;
static bool forceRedraw = true;
static unsigned long lastDisplayUpdate = 0;

// Previous state for smart redraw
static BambuState prevState;
static unsigned long connectScreenStart = 0;

// ---------------------------------------------------------------------------
//  Smooth gauge interpolation — values lerp toward MQTT actuals each frame
// ---------------------------------------------------------------------------
static float smoothNozzleTemp = 0;
static float smoothBedTemp    = 0;
static float smoothPartFan    = 0;
static float smoothAuxFan     = 0;
static float smoothChamberFan = 0;
static bool  smoothInited     = false;

static const float SMOOTH_ALPHA = 0.25f;  // per frame at 4Hz — ~1s to settle
static const float SNAP_THRESH  = 0.5f;   // snap when within 0.5 of target

static void smoothLerp(float& cur, float target) {
  float diff = target - cur;
  if (fabsf(diff) < SNAP_THRESH) cur = target;
  else cur += diff * SMOOTH_ALPHA;
}

// Returns true if any gauge is still animating
static bool tickGaugeSmooth(const BambuState& s, bool snap) {
  if (snap || !smoothInited) {
    smoothNozzleTemp = s.nozzleTemp;
    smoothBedTemp    = s.bedTemp;
    smoothPartFan    = s.coolingFanPct;
    smoothAuxFan     = s.auxFanPct;
    smoothChamberFan = s.chamberFanPct;
    smoothInited = true;
    return false;
  }
  smoothLerp(smoothNozzleTemp, s.nozzleTemp);
  smoothLerp(smoothBedTemp,    s.bedTemp);
  smoothLerp(smoothPartFan,    (float)s.coolingFanPct);
  smoothLerp(smoothAuxFan,     (float)s.auxFanPct);
  smoothLerp(smoothChamberFan, (float)s.chamberFanPct);

  const float ANIM_EPS = 0.01f;
  return (fabsf(smoothNozzleTemp - s.nozzleTemp) > ANIM_EPS) ||
         (fabsf(smoothBedTemp    - s.bedTemp)    > ANIM_EPS) ||
         (fabsf(smoothPartFan    - (float)s.coolingFanPct) > ANIM_EPS) ||
         (fabsf(smoothAuxFan     - (float)s.auxFanPct)     > ANIM_EPS) ||
         (fabsf(smoothChamberFan - (float)s.chamberFanPct) > ANIM_EPS);
}

// ---------------------------------------------------------------------------
//  Backlight
// ---------------------------------------------------------------------------
void setBacklight(uint8_t level) {
#if defined(BACKLIGHT_PIN) && BACKLIGHT_PIN >= 0
  analogWrite(BACKLIGHT_PIN, level);
#endif
}

// ---------------------------------------------------------------------------
//  Init
// ---------------------------------------------------------------------------
void initDisplay() {
  if ((dispSettings.rotation == 1) || (dispSettings.rotation == 3)) {
    use_width = SCREEN_H;
    use_height = SCREEN_W;
  } else {
    use_width = SCREEN_W;
    use_height = SCREEN_H;
  }

  Serial.println("Display: pre-init delay...");
  delay(500);
  Serial.println("Display: calling tft.init()...");
  Serial.flush();
  tft.init();  // TFT_eSPI configures SPI from build flags
  Serial.println("Display: tft.init() done");
  tft.setRotation(dispSettings.rotation);
  Serial.println("Display: setRotation done");
  tft.fillScreen(CLR_BG);
  Serial.println("Display: fillScreen done");

#if defined(BACKLIGHT_PIN) && BACKLIGHT_PIN >= 0
  pinMode(BACKLIGHT_PIN, OUTPUT);
  setBacklight(200);
#endif

  memset(&prevState, 0, sizeof(prevState));

  // Splash screen
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("BambuHelper", use_width / 2, use_height / 2 - 20);
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Printer Monitor", use_width / 2, use_height / 2 + 10);
  tft.setTextFont(1);
  tft.drawString(FW_VERSION, use_width / 2, use_height / 2 + 30);
}

void applyDisplaySettings() {
  tft.setRotation(dispSettings.rotation);
  tft.fillScreen(dispSettings.bgColor);
  if ((dispSettings.rotation == 1) || (dispSettings.rotation == 3)) {
    use_width = SCREEN_H;
    use_height = SCREEN_W;
  } else {
    use_width = SCREEN_W;
    use_height = SCREEN_H;
  }

  forceRedraw = true;
}

void triggerDisplayTransition() {
  // Clear previous state so everything redraws for the new printer
  memset(&prevState, 0, sizeof(prevState));
  smoothInited = false;  // snap gauges to new printer's values
  resetGaugeTextCache();
  tft.fillScreen(dispSettings.bgColor);
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
static const char* nozzleLabel(const BambuState& s) {
  if (!s.dualNozzle) return "Nozzle";
  return s.activeNozzle == 0 ? "Nozzle R" : "Nozzle L";
}

// ---------------------------------------------------------------------------
//  Speed level name helper
// ---------------------------------------------------------------------------
static const char* speedLevelName(uint8_t level) {
  switch (level) {
    case 1: return "Silent";
    case 2: return "Std";
    case 3: return "Sport";
    case 4: return "Ludicr";
    default: return "---";
  }
}

static uint16_t speedLevelColor(uint8_t level) {
  switch (level) {
    case 1: return CLR_BLUE;
    case 2: return CLR_GREEN;
    case 3: return CLR_ORANGE;
    case 4: return CLR_RED;
    default: return CLR_TEXT_DIM;
  }
}

// ---------------------------------------------------------------------------
//  Screen: AP Mode
// ---------------------------------------------------------------------------
static void drawAPMode() {
  tft.setTextDatum(MC_DATUM);

  // Title
  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("WiFi Setup", use_width / 2, 40);

  // Instructions
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connect to WiFi:", use_width / 2, 80);

  // AP SSID
  tft.setTextColor(CLR_CYAN, CLR_BG);
  tft.setTextFont(4);
  char ssid[32];
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  snprintf(ssid, sizeof(ssid), "%s%04X", WIFI_AP_PREFIX, mac);
  tft.drawString(ssid, use_width / 2, 110);

  // Password
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Password:", use_width / 2, 140);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString(WIFI_AP_PASSWORD, use_width / 2, 158);

  // IP
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Then open:", use_width / 2, 185);
  tft.setTextColor(CLR_ORANGE, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("192.168.4.1", use_width / 2, 210);
}

// ---------------------------------------------------------------------------
//  Screen: Connecting WiFi
// ---------------------------------------------------------------------------
static void drawConnectingWiFi() {
  tft.setTextDatum(MC_DATUM);

  // Title
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connecting to WiFi", use_width / 2, use_height / 2 - 20);

  int16_t tw = tft.textWidth("Connecting to WiFi");
  drawAnimDots(tft, use_width / 2 + tw / 2, use_height / 2 - 26, CLR_TEXT);

  // Slide bar
  const int16_t barW = 180;
  const int16_t barH = 8;
  drawSlideBar(tft, (use_width - barW) / 2, use_height / 2 + 4,
               barW, barH, CLR_BLUE, CLR_TRACK);
}

// ---------------------------------------------------------------------------
//  Screen: WiFi Connected (show IP)
// ---------------------------------------------------------------------------
static void drawWiFiConnected() {
  if (!forceRedraw) return;

  tft.setTextDatum(MC_DATUM);

  // Checkmark circle with tick
  int cx = use_width / 2;
  int cy = use_height / 2 - 40;
  tft.fillCircle(cx, cy, 25, CLR_GREEN);
  // Draw thick tick mark (3px wide)
  for (int i = -1; i <= 1; i++) {
    tft.drawLine(cx - 12, cy + i, cx - 4, cy + 8 + i, CLR_BG);     // short leg
    tft.drawLine(cx - 4, cy + 8 + i, cx + 12, cy - 6 + i, CLR_BG); // long leg
  }

  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("WiFi Connected", use_width / 2, use_height / 2 + 10);

  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.setTextFont(2);
  tft.drawString(WiFi.localIP().toString().c_str(), use_width / 2, use_height / 2 + 40);
}

// ---------------------------------------------------------------------------
//  Screen: Connecting MQTT
// ---------------------------------------------------------------------------
static void drawConnectingMQTT() {
  tft.setTextDatum(MC_DATUM);

  // Title
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connecting to Printer", use_width / 2, use_height / 2 - 40);

  int16_t tw = tft.textWidth("Connecting to Printer");
  drawAnimDots(tft, use_width / 2 + tw / 2, use_height / 2 - 46, CLR_TEXT);
  tft.setTextDatum(MC_DATUM);

  // Slide bar
  const int16_t barW = 180;
  const int16_t barH = 8;
  drawSlideBar(tft, (use_width - barW) / 2, use_height / 2 - 14,
               barW, barH, CLR_ORANGE, CLR_TRACK);

  // Connection mode + printer info
  PrinterSlot& p = displayedPrinter();
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.setTextFont(2);

  const char* modeStr = isCloudMode(p.config.mode) ? "Cloud" : "LAN";
  char infoBuf[40];
  if (isCloudMode(p.config.mode)) {
    snprintf(infoBuf, sizeof(infoBuf), "[%s] %s", modeStr,
             strlen(p.config.serial) > 0 ? p.config.serial : "no serial!");
  } else {
    snprintf(infoBuf, sizeof(infoBuf), "[%s] %s",  modeStr,
             strlen(p.config.ip) > 0 ? p.config.ip : "no IP!");
  }
  tft.drawString(infoBuf, use_width / 2, use_height / 2 + 10);

  // Elapsed time
  if (connectScreenStart > 0) {
    unsigned long elapsed = (millis() - connectScreenStart) / 1000;
    char elBuf[16];
    snprintf(elBuf, sizeof(elBuf), "%lus", elapsed);
    tft.fillRect(use_width / 2 - 30, use_height / 2 + 22, 60, 16, CLR_BG);
    tft.drawString(elBuf, use_width / 2, use_height / 2 + 30);
  }

  // Diagnostics (only after first attempt)
  const MqttDiag& d = getMqttDiag(rotState.displayIndex);
  if (d.attempts > 0) {
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);

    char buf[40];
    snprintf(buf, sizeof(buf), "Attempt: %u", d.attempts);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    tft.drawString(buf, use_width / 2, use_height / 2 + 50);

    if (d.lastRc != 0) {
      snprintf(buf, sizeof(buf), "Err: %s", mqttRcToString(d.lastRc));
      tft.setTextColor(CLR_RED, CLR_BG);
      tft.drawString(buf, use_width / 2, use_height / 2 + 62);
    }
  }
}

// ---------------------------------------------------------------------------
//  Screen: Idle (connected, not printing)
// ---------------------------------------------------------------------------
static void drawIdleNoPrinter() {
  if (!forceRedraw) return;

  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("BambuHelper", use_width / 2, use_height / 6); // 40

  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.setTextFont(2);
  tft.drawString("WiFi Connected", use_width / 2, use_height / 3); // 80

  tft.fillCircle(use_width / 2, 105, 5, CLR_GREEN);

  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.setTextFont(2);
  tft.drawString("No printer configured", use_width / 2, 140);
  tft.drawString("Open in browser:", use_width / 2, 165);

  tft.setTextColor(CLR_ORANGE, CLR_BG);
  tft.setTextFont(4);
  tft.drawString(WiFi.localIP().toString().c_str(), use_width / 2, 200);
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
    tft.fillScreen(dispSettings.bgColor);
    memset(&prevState, 0, sizeof(prevState));
    forceRedraw = true;
  }

  PrinterSlot& p = displayedPrinter();
  BambuState& s = p.state;

  bool animating = tickGaugeSmooth(s, forceRedraw);
  bool stateChanged = forceRedraw || (strcmp(s.gcodeState, prevState.gcodeState) != 0);
  bool tempChanged = forceRedraw || animating ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);
  bool connChanged = forceRedraw || (s.connected != prevState.connected);
  bool wifiChanged = forceRedraw || (s.wifiSignal != prevState.wifiSignal);

  tft.setTextDatum(MC_DATUM);

  // Printer name (only on forceRedraw — name doesn't change)
  if (forceRedraw) {
    tft.setTextColor(CLR_GREEN, CLR_BG);
    tft.setTextFont(4);
    const char* name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    tft.drawString(name, use_width / 2, use_height / 8); // 30
  }

  // Status badge — only redraw when state changes
  if (stateChanged) {
    tft.setTextFont(2);
    uint16_t stateColor = CLR_TEXT_DIM;
    const char* stateStr = s.gcodeState;
    if (strcmp(s.gcodeState, "IDLE") == 0) {
      stateColor = CLR_GREEN;
      stateStr = "Ready";
    } else if (strcmp(s.gcodeState, "FAILED") == 0) {
      stateColor = CLR_RED;
      stateStr = "ERROR";
    } else if (strcmp(s.gcodeState, "UNKNOWN") == 0 || s.gcodeState[0] == '\0') {
      stateStr = "Waiting...";
    }
    tft.fillRect(0, 50, use_width, 20, CLR_BG);
    tft.setTextColor(stateColor, CLR_BG);
    tft.drawString(stateStr, use_width / 2, use_height / 4); // 60
  }

  // Connected indicator
  if (connChanged) {
    tft.fillCircle(use_width / 2, use_height / 3 + 5, 5, s.connected ? CLR_GREEN : CLR_RED); // 85
  }

  // Nozzle temp gauge
  if (tempChanged) {
    drawTempGauge(tft, use_width / 2 - 55, use_height / 2 + 20, 30,
                  s.nozzleTemp, s.nozzleTarget, 300.0f,
                  dispSettings.nozzle.arc, nozzleLabel(s), nullptr, forceRedraw,
                  &dispSettings.nozzle, smoothNozzleTemp); // 140

    // Bed temp gauge
    drawTempGauge(tft, use_width / 2 + 55, use_height / 2 + 20, 30,
                  s.bedTemp, s.bedTarget, 120.0f,
                  dispSettings.bed.arc, "Bed", nullptr, forceRedraw,
                  &dispSettings.bed, smoothBedTemp); // 140
  }

  // Bottom: filament indicator or WiFi signal
  bool bottomChanged = wifiChanged || (s.ams.activeTray != prevState.ams.activeTray);
  if (bottomChanged) {
    tft.fillRect(0, use_height - 18, use_width, 18, CLR_BG);
    tft.setTextFont(2);
    tft.setTextDatum(BC_DATUM);

    if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS && s.ams.trays[s.ams.activeTray].present) {
      AmsTray& t = s.ams.trays[s.ams.activeTray];
      int cx = use_width / 2 - tft.textWidth(t.type) / 2 - 8;
      tft.drawCircle(cx, use_height - 8, 5, CLR_TEXT_DARK);
      tft.fillCircle(cx, use_height - 8, 4, t.colorRgb565);
      tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
      tft.drawString(t.type, use_width / 2 + 4, use_height - 2);
    } else {
      tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
      char wifiBuf[24];
      snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %d dBm", s.wifiSignal);
      tft.drawString(wifiBuf, use_width / 2, use_height - 2);
    }
  }
}

// ---------------------------------------------------------------------------
//  Helper: draw WiFi signal indicator in bottom-left corner
// ---------------------------------------------------------------------------
static void drawWifiSignalIndicator(const BambuState& s) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  char wifiBuf[16];
  snprintf(wifiBuf, sizeof(wifiBuf), "%ddBm", s.wifiSignal);
  tft.drawString(wifiBuf, 4, use_height - 8); // 232
}

// ---------------------------------------------------------------------------
//  Screen: Printing (main dashboard)
//  Layout: LED bar | header | 2x3 gauge grid | info line
// ---------------------------------------------------------------------------
static void drawPrinting() {
  PrinterSlot& p = displayedPrinter();
  BambuState& s = p.state;

  bool animating = tickGaugeSmooth(s, forceRedraw);
  bool progChanged = forceRedraw || (s.progress != prevState.progress);
  bool tempChanged = forceRedraw || animating ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);
  bool etaChanged = forceRedraw ||
                     (s.remainingMinutes != prevState.remainingMinutes);
  bool fansChanged = forceRedraw || animating ||
                     (s.coolingFanPct != prevState.coolingFanPct) ||
                     (s.auxFanPct != prevState.auxFanPct) ||
                     (s.chamberFanPct != prevState.chamberFanPct);
  bool stateChanged = forceRedraw ||
                      (strcmp(s.gcodeState, prevState.gcodeState) != 0);

  // 2x3 gauge grid constants
  const int16_t gR = 32;                                  // radius for all gauges
  const int16_t gT = 6;                                   // thickness for progress arc
  const int16_t col1 = (use_width / 2) - (use_width / 3); // 42 left column center X
  const int16_t col2 = use_width / 2;                     // 120 middle column center X
  const int16_t col3 = (use_width / 2) + (use_width / 3); // 198 right column center X
  const int16_t row1Y = use_height / 4;                   // 60 row 1 center Y (progress, nozzle, bed)
  const int16_t row2Y = use_height / 2 + 28;              // 148 row 2 center Y (part fan, aux fan, chamb fan)

  // === H2-style LED progress bar (y=0-5) ===
  if (progChanged) {
    drawLedProgressBar(tft, 0, s.progress);
  }

  // === Header bar (y=7-25) ===
  if (forceRedraw || stateChanged) {
    uint16_t hdrBg = dispSettings.bgColor;
    tft.fillRect(0, 7, use_width, use_height / 12, hdrBg); // 20

    // Printer name (left)
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(CLR_TEXT, hdrBg);
    const char* name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    tft.drawString(name, 6, use_height / 12 - 3); // 17

    // State badge (right)
    uint16_t badgeColor = CLR_TEXT_DIM;
    if (strcmp(s.gcodeState, "RUNNING") == 0) badgeColor = CLR_GREEN;
    else if (strcmp(s.gcodeState, "PAUSE") == 0) badgeColor = CLR_YELLOW;
    else if (strcmp(s.gcodeState, "FAILED") == 0) badgeColor = CLR_RED;
    else if (strcmp(s.gcodeState, "PREPARE") == 0) badgeColor = CLR_BLUE;

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(badgeColor, hdrBg);
    tft.setTextFont(2);
    tft.fillCircle(use_width - 8 - tft.textWidth(s.gcodeState) - 10, use_height / 12 - 3, 4, badgeColor); // 17
    tft.drawString(s.gcodeState, use_width - 8, use_height / 12 - 3);                                      // 17

    // Printer indicator dots (multi-printer)
    if (getActiveConnCount() > 1) {
      for (uint8_t di = 0; di < MAX_ACTIVE_PRINTERS; di++) {
        if (!isPrinterConfigured(di)) continue;
        uint16_t dotClr = (di == rotState.displayIndex) ? CLR_GREEN : CLR_TEXT_DARK;
        tft.fillCircle(use_width / 2 - 5 + di * 10, use_height / 24, 3, dotClr); // 10
      }
    }
  }

  // === Row 1: Progress | Nozzle | Bed (y=30-100) ===

  if (progChanged || forceRedraw) {
    drawProgressArc(tft, col1, row1Y, gR, gT,
                    s.progress, prevState.progress,
                    s.remainingMinutes, forceRedraw);
  }

  if (tempChanged) {
    drawTempGauge(tft, col2, row1Y, gR,
                  s.nozzleTemp, s.nozzleTarget, 300.0f,
                  dispSettings.nozzle.arc, nozzleLabel(s), nullptr, forceRedraw,
                  &dispSettings.nozzle, smoothNozzleTemp);

    drawTempGauge(tft, col3, row1Y, gR,
                  s.bedTemp, s.bedTarget, 120.0f,
                  dispSettings.bed.arc, "Bed", nullptr, forceRedraw,
                  &dispSettings.bed, smoothBedTemp);
  }

  // === Row 2: Part Fan | Aux Fan | Chamber Fan (y=106-176) ===

  if (fansChanged) {
    drawFanGauge(tft, col1, row2Y, gR,
                 s.coolingFanPct, dispSettings.partFan.arc, "Part", forceRedraw,
                 &dispSettings.partFan, smoothPartFan);

    drawFanGauge(tft, col2, row2Y, gR,
                 s.auxFanPct, dispSettings.auxFan.arc, "Aux", forceRedraw,
                 &dispSettings.auxFan, smoothAuxFan);

    drawFanGauge(tft, col3, row2Y, gR,
                 s.chamberFanPct, dispSettings.chamberFan.arc, "Chamber", forceRedraw,
                 &dispSettings.chamberFan, smoothChamberFan);
  }

  // === Info line — ETA finish time or PAUSE/ERROR alert (below row 2 labels) ===
  if (etaChanged || stateChanged) {
    tft.fillRect(0, use_height - 50, use_width, 30, CLR_BG); // 190
    tft.setTextDatum(MC_DATUM);

    if (strcmp(s.gcodeState, "PAUSE") == 0) {
      // Prominent PAUSE alert
      tft.setTextFont(4);
      tft.setTextColor(CLR_YELLOW, CLR_BG);
      tft.drawString("PAUSED", use_width / 2, use_height - 33); // 207
    } else if (strcmp(s.gcodeState, "FAILED") == 0) {
      // Prominent ERROR alert
      tft.setTextFont(4);
      tft.setTextColor(CLR_RED, CLR_BG);
      tft.drawString("ERROR!", use_width / 2, use_height - 33); // 207
    } else if (s.remainingMinutes > 0) {
      // Use time() directly - avoids getLocalTime() race condition with timeout 0.
      // Once NTP syncs the RTC keeps running; ntpSynced latches true forever.
      static bool ntpSynced = false;
      time_t nowEpoch = time(nullptr);
      struct tm now;
      localtime_r(&nowEpoch, &now);
      if (now.tm_year > (2020 - 1900)) ntpSynced = true;

      if (ntpSynced) {
        // Calculate ETA: current time + remaining minutes
        time_t etaEpoch = nowEpoch + (time_t)s.remainingMinutes * 60;
        struct tm etaTm;
        localtime_r(&etaEpoch, &etaTm);

        char etaBuf[32];
        int etaH = etaTm.tm_hour;
        const char* ampm = "";
        if (!netSettings.use24h) {
          ampm = etaH < 12 ? "AM" : "PM";
          etaH = etaH % 12;
          if (etaH == 0) etaH = 12;
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
        tft.setTextFont(4);
        tft.setTextColor(CLR_GREEN, CLR_BG);
        tft.drawString(etaBuf, use_width / 2, use_height - 33); // 207
      } else {
        // NTP not synced yet - show remaining time only
        char remBuf[24];
        uint16_t h = s.remainingMinutes / 60;
        uint16_t m = s.remainingMinutes % 60;
        snprintf(remBuf, sizeof(remBuf), "Remaining: %dh %02dm", h, m);
        tft.setTextFont(4);
        tft.setTextColor(CLR_TEXT, CLR_BG);
        tft.drawString(remBuf, use_width / 2, use_height - 33); // 207
      }
    } else {
      tft.setTextFont(4);
      tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
      tft.drawString("ETA: ---", use_width / 2, use_height - 33); // 207
    }
  }

  // === Bottom status bar — Filament/WiFi | Layer | Speed (y=222-240) ===
  // WiFi signal only matters when AMS indicator is not shown
  bool showingWifi = !(s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS && s.ams.trays[s.ams.activeTray].present)
                  && !(s.ams.vtPresent && s.ams.activeTray == 254);
  bool bottomChanged = forceRedraw ||
                       (s.speedLevel != prevState.speedLevel) ||
                       (s.layerNum != prevState.layerNum) ||
                       (s.totalLayers != prevState.totalLayers) ||
                       (s.ams.activeTray != prevState.ams.activeTray) ||
                       (showingWifi && s.wifiSignal != prevState.wifiSignal);
  if (bottomChanged) {
    tft.fillRect(0, use_height - 18, use_width, 18, CLR_BG); // 222
    tft.setTextFont(2);

    // Left: filament indicator (if AMS active) or WiFi signal
    if (s.ams.present && s.ams.activeTray < AMS_MAX_TRAYS) {
      AmsTray& t = s.ams.trays[s.ams.activeTray];
      if (t.present) {
        tft.drawCircle(10, use_height - 8, 5, CLR_TEXT_DARK); // 232
        tft.fillCircle(10, use_height - 8, 4, t.colorRgb565); // 232
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
        tft.drawString(t.type, 19, use_height - 8); // 232
      } else {
        drawWifiSignalIndicator(s);
      }
    } else if (s.ams.vtPresent && s.ams.activeTray == 254) {
      tft.drawCircle(10, use_height - 8, 5, CLR_TEXT_DARK);       // 232
      tft.fillCircle(10, use_height - 8, 4, s.ams.vtColorRgb565); // 232
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
      tft.drawString(s.ams.vtType, 19, use_height - 8); // 232
    } else {
      drawWifiSignalIndicator(s);
    }

    // Layer count (center)
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    char layerBuf[20];
    snprintf(layerBuf, sizeof(layerBuf), "L%d/%d", s.layerNum, s.totalLayers);
    tft.drawString(layerBuf, use_width / 2, use_height - 8); // 232

    // Speed mode (right)
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(speedLevelColor(s.speedLevel), CLR_BG);
    tft.drawString(speedLevelName(s.speedLevel), use_width - 4, use_height - 8); // 232
  }
}

// ---------------------------------------------------------------------------
//  Screen: Finished (same layout as printing, but with 2 gauges + status)
// ---------------------------------------------------------------------------
static void drawFinished() {
  PrinterSlot& p = displayedPrinter();
  BambuState& s = p.state;

  bool animating = tickGaugeSmooth(s, forceRedraw);
  bool tempChanged = forceRedraw || animating ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);

  const int16_t gR = 32;
  const int16_t gaugeLeft = use_width / 3;      // 72 two gauges centered on 240px
  const int16_t gaugeRight = use_width / 3 * 2; // 168
  const int16_t gaugeY = use_height / 3;        // 80

  // === H2-style LED progress bar at 100% (y=0-5) ===
  if (forceRedraw) {
    drawLedProgressBar(tft, 0, 100);
  }

  // === Header bar (y=7-25) — same as printing screen ===
  if (forceRedraw) {
    uint16_t hdrBg = dispSettings.bgColor;
    tft.fillRect(0, 7, use_width, 20, hdrBg);

    // Printer name (left)
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(CLR_TEXT, hdrBg);
    const char* name = (p.config.name[0] != '\0') ? p.config.name : "Printer";
    tft.drawString(name, 6, 17);

    // FINISH badge (right)
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(CLR_GREEN, hdrBg);
    tft.setTextFont(2);
    tft.fillCircle(use_width - 8 - tft.textWidth("FINISH") - 10, 17, 4, CLR_GREEN);
    tft.drawString("FINISH", use_width - 8, 17);

    // Printer indicator dots (multi-printer)
    if (getActiveConnCount() > 1) {
      for (uint8_t di = 0; di < MAX_ACTIVE_PRINTERS; di++) {
        if (!isPrinterConfigured(di)) continue;
        uint16_t dotClr = (di == rotState.displayIndex) ? CLR_GREEN : CLR_TEXT_DARK;
        tft.fillCircle(use_width / 2 - 5 + di * 10, 10, 3, dotClr);
      }
    }
  }

  // === Row 1: Nozzle | Bed (two gauges centered) ===
  if (tempChanged) {
    drawTempGauge(tft, gaugeLeft, gaugeY, gR,
                  s.nozzleTemp, s.nozzleTarget, 300.0f,
                  dispSettings.nozzle.arc, nozzleLabel(s), nullptr, forceRedraw,
                  &dispSettings.nozzle, smoothNozzleTemp);

    drawTempGauge(tft, gaugeRight, gaugeY, gR,
                  s.bedTemp, s.bedTarget, 120.0f,
                  dispSettings.bed.arc, "Bed", nullptr, forceRedraw,
                  &dispSettings.bed, smoothBedTemp);
  }

  // === "Print Complete!" status ===
  if (forceRedraw) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CLR_GREEN, CLR_BG);
    tft.setTextFont(4);
    tft.drawString("Print Complete!", use_width / 2, use_height / 2 + 28); // 148
  }

  // === File name ===
  if (forceRedraw) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    if (s.subtaskName[0] != '\0') {
      char truncName[26];
      strncpy(truncName, s.subtaskName, 25);
      truncName[25] = '\0';
      tft.drawString(truncName, use_width / 2, use_height / 2 + 58); // 178
    }
  }

  // === Bottom status bar — WiFi (y=218-236) ===
  if (forceRedraw) {
    tft.fillRect(0, use_height - 22, use_width, 22, CLR_BG); // 218
    tft.setTextFont(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    char wifiBuf[20];
    snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %d dBm", s.wifiSignal);
    tft.drawString(wifiBuf, use_width / 2, use_height - 10); // 230
  }
}

// ---------------------------------------------------------------------------
//  Main update (called from loop)
// ---------------------------------------------------------------------------
void updateDisplay() {
  // Shimmer runs at its own cadence (~40fps), independent of display refresh
  if (currentScreen == SCREEN_PRINTING) {
    BambuState& sh = displayedPrinter().state;
    tickProgressShimmer(tft, 0, sh.progress, sh.printing);
  }
  // Pong clock runs at ~50fps, independent of display refresh
  if (currentScreen == SCREEN_CLOCK && dispSettings.pongClock) {
    tickPongClock();
  }

  unsigned long now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_MS) return;
  lastDisplayUpdate = now;

  // Detect screen change
  if (currentScreen != prevScreen) {
    // Restore backlight when leaving SCREEN_OFF
    if (prevScreen == SCREEN_OFF && currentScreen != SCREEN_OFF) {
      setBacklight(brightness);
    }
    // Reset text size in case Pong clock left it scaled up
    tft.setTextSize(1);
    tft.fillScreen(currentScreen == SCREEN_OFF ? TFT_BLACK : dispSettings.bgColor);
    forceRedraw = true;
    if (currentScreen == SCREEN_CONNECTING_MQTT) {
      connectScreenStart = millis();
    }
    if (currentScreen == SCREEN_CLOCK) {
      if (dispSettings.pongClock) resetPongClock();
      else resetClock();
    }
    prevScreen = currentScreen;
  }

  switch (currentScreen) {
  case SCREEN_SPLASH:
    // Splash shown in initDisplay(), auto-advance handled by main.cpp
    break;

  case SCREEN_AP_MODE:
      if (forceRedraw) drawAPMode();
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
      if (!dispSettings.pongClock) drawClock();
    // Pong clock is ticked before the throttle (above)
    break;

  case SCREEN_OFF:
    if (Serial.isPlugged()) drawClock();
    else if (forceRedraw){
      tft.fillScreen(TFT_BLACK);
      setBacklight(0);
      }
    }
    break;
  }

  // Save state for next smart-redraw comparison
  memcpy(&prevState, &displayedPrinter().state, sizeof(BambuState));
  forceRedraw = false;
}

#endif