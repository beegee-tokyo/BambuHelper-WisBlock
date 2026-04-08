#ifndef LAYOUT_H
#define LAYOUT_H

// Layout profile dispatcher.
// Each display target defines LY_* constants for screen dimensions,
// gauge positions, text positions, etc.
// To add a new display: create layout_xxx.h and add an #elif here.

#if defined(DISPLAY_CYD)
  #include "layout_cyd.h"       // ESP32-2432S028: ILI9341 240x320 portrait
#elif defined(DISPLAY_RAK14014)
  #include "layout_rak3312.h"
#else
  #include "layout_default.h"   // ESP32-S3 Mini: ST7789 240x240
#endif

#endif // LAYOUT_H
