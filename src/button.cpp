#include "button.h"
#include "settings.h"

#if defined(USE_XPT2046)
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>
  static SPIClass touchSPI(HSPI);
  static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
  static bool touchReady = false;
#elif defined(USE_CST816)
  #include <Wire.h>
  #define CST816_ADDR          0x15
  #define CST816_TOUCH_NUM_REG 0x02
  static bool cst816BusReady = false;
  static bool cst816Seen = false;

  static bool cst816Probe() {
    Wire.beginTransmission(CST816_ADDR);
    return Wire.endTransmission(true) == 0;
  }

  static bool cst816ReadReg(uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)CST816_ADDR, (uint8_t)1) != 1) return false;
    value = Wire.read();
    return true;
  }
#elif defined(TOUCH_CS)
  #include "display_ui.h"  // extern tft for getTouch()
#elif defined(_VARIANT_RAK3112_)
#include "RAK14014_FT6336U.h"
/** Touch screen driver instance */
FT6336U ft6336u;
bool wait_for_touch = true;

static void keyIntHandle(void)
{
	// Conditional debug print
	wait_for_touch = false;
	detachInterrupt(digitalPinToInterrupt(WB_IO6));
}
#endif

  static bool lastRaw = false;
  static bool stableState = false;
  static unsigned long lastChangeMs = 0;
  static const unsigned long DEBOUNCE_MS = 50;

  void initButton() {
	  if (buttonType == BTN_DISABLED) return;
#if defined(USE_XPT2046)
	  if (buttonType == BTN_TOUCHSCREEN) {
		  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
		  ts.begin(touchSPI);
		  touchReady = true;
		  Serial.println("XPT2046 touch initialized (separate SPI)");
		  return;
	  }
#elif defined(USE_CST816)
	  if (buttonType == BTN_TOUCHSCREEN) {
#ifdef CST816_RST
    // Hardware reset - required for CST816 to respond on I2C
    pinMode(CST816_RST, OUTPUT);
    digitalWrite(CST816_RST, LOW);
    delay(20);
    digitalWrite(CST816_RST, HIGH);
    delay(50);  // wait for controller to boot after reset
#endif
		  Wire.begin(CST816_SDA, CST816_SCL);
		  Wire.setClock(400000);
    cst816BusReady = true;
    if (cst816Probe()) {
      uint8_t touchNum = 0;
      if (cst816ReadReg(CST816_TOUCH_NUM_REG, touchNum)) {
        Serial.printf("CST816 touch initialized (I2C SDA=%d SCL=%d, reg0x%02X=0x%02X)\n",
                      CST816_SDA, CST816_SCL, CST816_TOUCH_NUM_REG, touchNum);
        cst816Seen = true;
      } else {
        Serial.printf("CST816 detected on I2C addr 0x%02X, but register reads failed (SDA=%d SCL=%d)\n",
                      CST816_ADDR, CST816_SDA, CST816_SCL);
      }
    } else {
      Serial.printf("CST816 touch did not answer at init (addr 0x%02X, SDA=%d SCL=%d); will keep retrying at runtime\n",
                    CST816_ADDR, CST816_SDA, CST816_SCL);
    }
		  return;
	  }
#elif defined(_VARIANT_RAK3112_)
	  // if (buttonType == BTN_TOUCHSCREEN) { // RAK14014 touch screen enforced
		  ft6336u.begin();
		  attachInterrupt(digitalPinToInterrupt(WB_IO6), keyIntHandle, FALLING);
		  buttonType = BTN_TOUCHSCREEN;
		  return;
	  // }
#endif
	  if (buttonType == BTN_TOUCHSCREEN) return;
	  if (buttonPin == 0) return;
	  if (buttonType == BTN_PUSH) {
		  pinMode(buttonPin, INPUT_PULLUP);
	  } else { // BTN_TOUCH (TTP223)
		  pinMode(buttonPin, INPUT);
	  }
	  lastRaw = false;
	  stableState = false;
	  lastChangeMs = 0;
  }

bool wasButtonPressed() {
  if (buttonType == BTN_DISABLED) return false;

  bool raw;
  if (buttonType == BTN_TOUCHSCREEN) {
#if defined(USE_XPT2046)
    if (!touchReady) return false;
    raw = ts.touched();
#elif defined(USE_CST816)
    if (!cst816BusReady) return false;
    uint8_t touchNum = 0;
    if (!cst816ReadReg(CST816_TOUCH_NUM_REG, touchNum)) return false;
    if (!cst816Seen) {
      Serial.printf("CST816 touch became responsive at runtime (addr 0x%02X)\n", CST816_ADDR);
      cst816Seen = true;
    }
    raw = (touchNum > 0);
#elif defined(TOUCH_CS)
    uint16_t tx, ty;
    raw = tft.getTouch(&tx, &ty);
#elif defined(_VARIANT_RAK3112_)
	if (wait_for_touch) {
		return false;
	} else {
		wait_for_touch = true;
		attachInterrupt(digitalPinToInterrupt(WB_IO6), keyIntHandle, FALLING);
		return true;
	}
#else
	  return false;
#endif
  } else if (buttonType == BTN_PUSH) {
    if (buttonPin == 0) return false;
    raw = (digitalRead(buttonPin) == LOW);   // active LOW with pull-up
  } else {
    if (buttonPin == 0) return false;
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
