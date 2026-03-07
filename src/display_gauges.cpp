#include "display_gauges.h"
#include "config.h"

// ---------------------------------------------------------------------------
//  H2-style LED progress bar
// ---------------------------------------------------------------------------
void drawLedProgressBar(TFT_eSPI& tft, int16_t y, uint8_t progress) {
  const int16_t barW = 236;
  const int16_t barH = 5;
  const int16_t barX = (SCREEN_W - barW) / 2;

  tft.fillRect(barX, y, barW, barH, CLR_BG);

  if (progress == 0) return;

  int16_t fillW = (progress * barW) / 100;
  if (fillW < 1) fillW = 1;

  // Color: blue -> gold -> green
  uint16_t barColor;
  if (progress < 25) {
    barColor = CLR_BLUE;
  } else if (progress < 75) {
    barColor = CLR_GOLD;
  } else {
    barColor = CLR_GREEN;
  }

  tft.fillRoundRect(barX, y, fillW, barH, 2, barColor);

  uint16_t glowColor = tft.alphaBlend(160, CLR_TEXT, barColor);
  tft.drawFastHLine(barX + 1, y + barH / 2, fillW - 2, glowColor);

  if (fillW > 4 && progress < 100) {
    tft.fillRect(barX + fillW - 3, y, 3, barH, glowColor);
  }

  if (fillW < barW) {
    tft.fillRoundRect(barX + fillW, y, barW - fillW, barH, 2, CLR_TRACK);
  }
}

// ---------------------------------------------------------------------------
//  Helper: draw 270-deg arc track + fill, handling decrease properly
// ---------------------------------------------------------------------------
static void drawArc270(TFT_eSPI& tft, int16_t cx, int16_t cy,
                       int16_t radius, int16_t thickness,
                       uint16_t fillEnd, uint16_t fillColor, bool forceRedraw) {
  const uint16_t startAngle = 60;
  const uint16_t endAngle = 300;

  if (forceRedraw) {
    tft.fillCircle(cx, cy, radius + 2, CLR_BG);
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, endAngle, CLR_TRACK, CLR_BG, false);
  }

  // Draw filled portion
  if (fillEnd > startAngle) {
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, fillEnd, fillColor, CLR_BG, false);
  }

  // Always redraw track for unfilled portion (handles value decrease)
  if (fillEnd < endAngle) {
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      fillEnd, endAngle, CLR_TRACK, CLR_BG, false);
  }
}

// ---------------------------------------------------------------------------
//  Helper: clear gauge center and prepare for text
// ---------------------------------------------------------------------------
static void clearGaugeCenter(TFT_eSPI& tft, int16_t cx, int16_t cy,
                             int16_t radius, int16_t thickness) {
  int16_t textR = radius - thickness - 1;
  tft.fillCircle(cx, cy, textR, CLR_BG);
}

// ---------------------------------------------------------------------------
//  Main progress arc (270 degrees)
// ---------------------------------------------------------------------------
void drawProgressArc(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                     int16_t thickness, uint8_t progress, uint8_t prevProgress,
                     uint16_t remainingMin, bool forceRedraw) {
  const uint16_t startAngle = 60;

  uint16_t fillEnd = startAngle + (progress * 240) / 100;
  if (fillEnd > 300) fillEnd = 300;

  // Color: blue -> gold -> green
  uint16_t arcColor;
  if (progress < 25) {
    arcColor = CLR_BLUE;
  } else if (progress < 75) {
    arcColor = CLR_GOLD;
  } else {
    arcColor = CLR_GREEN;
  }

  drawArc270(tft, cx, cy, radius, thickness, fillEnd, arcColor, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  bool compact = (radius < 50);

  // Percentage (no "%" in compact to keep text narrow)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.setTextFont(compact ? 4 : 6);
  char pctBuf[8];
  if (compact) {
    snprintf(pctBuf, sizeof(pctBuf), "%d", progress);
  } else {
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress);
  }
  tft.drawString(pctBuf, cx, cy - (compact ? 4 : 8));

  // Time remaining
  tft.setTextFont(compact ? 1 : 2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  char timeBuf[16];
  if (remainingMin >= 60) {
    snprintf(timeBuf, sizeof(timeBuf), "%dh%dm", remainingMin / 60, remainingMin % 60);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "%dm", remainingMin);
  }
  tft.drawString(timeBuf, cx, cy + (compact ? 10 : 18));

  // Label below gauge
  if (compact) {
    tft.setTextFont(1);
    tft.setTextColor(CLR_GREEN, CLR_BG);
    tft.drawString("Progress", cx, cy + radius + 6);
  }
}

// ---------------------------------------------------------------------------
//  Temperature arc gauge (270 degrees)
// ---------------------------------------------------------------------------
void drawTempGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                   float current, float target, float maxTemp,
                   uint16_t accentColor, const char* label,
                   const uint8_t* icon, bool forceRedraw) {
  const uint16_t startAngle = 60;
  const int16_t thickness = 6;

  float ratio = (maxTemp > 0) ? (current / maxTemp) : 0;
  if (ratio > 1.0f) ratio = 1.0f;
  if (ratio < 0.0f) ratio = 0.0f;

  uint16_t fillEnd = startAngle + (uint16_t)(ratio * 240);
  if (fillEnd <= startAngle && ratio > 0.01f) fillEnd = startAngle + 1;
  if (fillEnd > 300) fillEnd = 300;

  uint16_t tempColor;
  if (ratio < 0.3f) {
    tempColor = CLR_BLUE;
  } else if (ratio < 0.7f) {
    tempColor = accentColor;
  } else {
    tempColor = CLR_RED;
  }

  uint16_t drawFill = (ratio > 0.01f) ? fillEnd : startAngle;
  drawArc270(tft, cx, cy, radius, thickness, drawFill, tempColor, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  // Current temp
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  char tempBuf[12];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", current);
  tft.drawString(tempBuf, cx, cy - 4);

  // Target temp
  tft.setTextFont(1);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  snprintf(tempBuf, sizeof(tempBuf), "/%.0f", target);
  tft.drawString(tempBuf, cx, cy + 10);

  // Label below gauge
  tft.setTextColor(accentColor, CLR_BG);
  tft.drawString(label, cx, cy + radius + 6);
}

// ---------------------------------------------------------------------------
//  Fan speed gauge (270 degrees, 0-100%)
// ---------------------------------------------------------------------------
void drawFanGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                  uint8_t percent, uint16_t accentColor, const char* label,
                  bool forceRedraw) {
  const uint16_t startAngle = 60;
  const int16_t thickness = 6;

  uint16_t fillEnd = startAngle + (percent * 240) / 100;
  if (fillEnd > 300) fillEnd = 300;

  uint16_t fanColor;
  if (percent == 0) {
    fanColor = CLR_TEXT_DIM;
  } else if (percent < 50) {
    fanColor = accentColor;
  } else {
    fanColor = CLR_GREEN;
  }

  uint16_t drawFill = (percent > 0) ? fillEnd : startAngle;
  drawArc270(tft, cx, cy, radius, thickness, drawFill, fanColor, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  // Percentage value
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", percent);
  tft.drawString(buf, cx, cy);

  // Label below gauge
  tft.setTextFont(1);
  tft.setTextColor(accentColor, CLR_BG);
  tft.drawString(label, cx, cy + radius + 6);
}
