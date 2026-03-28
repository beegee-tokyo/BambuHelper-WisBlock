#ifdef _VARIANT_RAK3112_
#ifndef _RAK1921_
#include "clock_mode.h"
#include "display_ui.h"
#include "settings.h"
#include "config.h"
#include <time.h>

static int prevMinute = -1;

// Screen width and height changes depending on rotation
extern int32_t use_width;
extern int32_t use_height;

void resetClock() {
  prevMinute = -1;
}

void drawClock() {
  struct tm now;
  if (!getLocalTime(&now, 0)) return;

  // Only redraw when minute changes (resetClock() forces redraw)
  if (now.tm_min == prevMinute) return;
  prevMinute = now.tm_min;

  uint16_t bg = dispSettings.bgColor;

  // Clear clock area
  tft.fillRect(0, 50, use_width, 140, bg); // 240

  // Time — large 7-segment font
  char timeBuf[12];
  if (netSettings.use24h) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.tm_hour, now.tm_min);
  } else {
    int h = now.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d", h, now.tm_min);
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(7);
  tft.setTextColor(CLR_TEXT, bg);
  tft.drawString(timeBuf, use_width / 2, 100); // 120

  // AM/PM indicator for 12h mode
  if (!netSettings.use24h) {
    tft.setTextFont(4);
    tft.setTextColor(CLR_TEXT_DIM, bg);
	tft.drawString(now.tm_hour < 12 ? "AM" : "PM", use_width / 2, 135); // 120
  }

  // Date — smaller font below
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char dateBuf[20];
  snprintf(dateBuf, sizeof(dateBuf), "%s  %02d.%02d.%04d",
           days[now.tm_wday], now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);
  tft.setTextFont(4);
  tft.setTextColor(CLR_TEXT_DIM, bg);
  tft.drawString(dateBuf, use_width / 2, 155); // 120
}

#endif
#endif