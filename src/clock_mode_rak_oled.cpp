#ifdef _VARIANT_RAK3112_
#ifdef _RAK1921_
#include "clock_mode.h"
#include "display_ui.h"
#include "settings.h"
#include "config.h"
#include <time.h>
#include <nRF_SSD1306Wire.h>

static int prevMinute = -1;

extern SSD1306Wire oled_display;
void rak1921_write_header(char *header_line);
void rak1921_draw_line(int32_t line, char *content);
void rak1921_clear_lines(void);

void resetClock()
{
  prevMinute = -1;
}

void drawClock()
{
  struct tm now;
  if (!getLocalTime(&now, 0))
    return;

  // Only redraw when minute changes (resetClock() forces redraw)
  if (now.tm_min == prevMinute)
    return;
  prevMinute = now.tm_min;

  uint16_t bg = dispSettings.bgColor;

  // Clear clock area
  oled_display.displayOff();
  oled_display.clear();
  oled_display.displayOn();

  // Time — large 7-segment font
  char timeBuf[12];
  if (netSettings.use24h)
  {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.tm_hour, now.tm_min);
  }
  else
  {
    int h = now.tm_hour % 12;
    if (h == 0)
      h = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d %s", h, now.tm_min, now.tm_hour < 12 ? "AM" : "PM");
  }

  rak1921_write_header((char *)"BambuHelper - Idle");

  rak1921_draw_line(3, timeBuf);

  // Date — smaller font below
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char dateBuf[20];
  snprintf(dateBuf, sizeof(dateBuf), "%s  %02d.%02d.%04d",
           days[now.tm_wday], now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);
  rak1921_draw_line(4, dateBuf);
}
#endif
#endif