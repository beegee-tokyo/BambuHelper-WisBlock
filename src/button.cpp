#include "button.h"
#include "settings.h"

static bool lastRaw = false;
static bool stableState = false;
static unsigned long lastChangeMs = 0;
static const unsigned long DEBOUNCE_MS = 50;

#ifdef _VARIANT_RAK3112_
#include "RAK14014_FT6336U.h"
/** Touch screen driver instance */
FT6336U ft6336u;
bool wait_for_touch = true;
bool btnDebugLog = false;
#define BTN_LOG(fmt, ...) do { if (btnDebugLog) Serial.printf("BTN: " fmt "\n", ##__VA_ARGS__); } while(0)

static void keyIntHandle(void) 
{
  // Conditional debug print
  BTN_LOG("BTN INT");
  wait_for_touch = false;
  detachInterrupt(digitalPinToInterrupt(WB_IO6));
}
#endif

void initButton()
{
#ifdef _VARIANT_RAK3112_ 
#ifndef _RAK1921_
  // RAK14014 touch screen
  ft6336u.begin();
  attachInterrupt(digitalPinToInterrupt(WB_IO6), keyIntHandle, FALLING);
  BTN_LOG("BTN initialized");
  buttonType = BTN_PUSH;
  return;
#endif
#endif
  if (buttonType == BTN_DISABLED || buttonPin == 0)return;
  if (buttonType == BTN_PUSH) {
    pinMode(buttonPin, INPUT_PULLUP);
  } else {  // BTN_TOUCH (TTP223)
    pinMode(buttonPin, INPUT);
  }

  lastRaw = false;
  stableState = false;
  lastChangeMs = 0;
}

bool wasButtonPressed() {
#ifdef _VARIANT_RAK3112_
  if (wait_for_touch)
  {
	return false;
  } else {
	wait_for_touch = true;
    BTN_LOG("BTN touch return");
	attachInterrupt(digitalPinToInterrupt(WB_IO6), keyIntHandle, FALLING);
	return true;
  }
#endif
  if (buttonType == BTN_DISABLED || buttonPin == 0) return false;

  bool raw;
  if (buttonType == BTN_PUSH) {
    raw = (digitalRead(buttonPin) == LOW);   // active LOW with pull-up
  } else {
    raw = (digitalRead(buttonPin) == HIGH);  // TTP223: active HIGH
  }

  // Debounce
  if (raw != lastRaw) {
    lastChangeMs = millis();
    lastRaw = raw;
  }
  if ((millis() - lastChangeMs) < DEBOUNCE_MS) return false;

  // Rising edge detection
  bool result = false;
  if (raw && !stableState) {
    result = true;
  }
  stableState = raw;

  return result;
}
