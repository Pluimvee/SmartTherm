#pragma once

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

class Display : public Adafruit_ST7735
{
private:
  void _watchdog();
  bool _print(int16_t x, int y, const GFXfont *font, uint16_t color, const char *value, char *cache);
  uint8_t _area;
public:
  Display();
  static Display *instance();
  bool begin();
  bool update(bool wifi_connected, bool mqtt_connected, int OT_errors);
  void log(const char *msg);
};
