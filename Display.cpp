#include "Display.h"
#include "SmartControl.h"
#define LOG_REMOTE
#define LOG_LEVEL 2
#include <Logging.h>
#include <Timer.h>
#include <Clock.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>      //9-22px height
#include <Fonts/FreeSansBold9pt7b.h>  //9-22px height
#include <Fonts/FreeSansBold12pt7b.h>  //9-22px height
#include <Fonts/Org_01.h>     //  7px height
#include <Fonts/Picopixel.h>  //  7px heigth

//  °°°°°°°°°°°°
////////////////////////////////////////////////////////////////////////////////////////////
// Configuration
#define TFT_CS         -1 // not used
#define TFT_DC         16 // D0 - GPIO16
#define TFT_RST        -1 // not used

////////////////////////////////////////////////////////////////////////////////////////////
extern Clock rtc;

////////////////////////////////////////////////////////////////////////////////////////////
// singleton object
Display *_display = 0;
Display *Display::instance() { return _display; }

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
Display::Display()
: Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST)
{
  _area = 0;
  _display = this;
  initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool Display::begin()
{
  setRotation(1); // rotate 90 degrees
  fillScreen(ST77XX_BLACK);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool Display::_print(int16_t x, int y, const GFXfont *font, uint16_t color, const char *value, char *cache)
{
  if (cache != 0 && strcmp(value, cache) == 0)
    return false;  // value did not change
  
  setFont(font);
  setTextColor(color);
  setTextWrap(false);

  // clean background
  int16_t xx, yy, ww, hh;
  if (cache != 0) {
    getTextBounds(cache, x, y, &xx, &yy, (uint16_t*) &ww, (uint16_t*) &hh);
    if (strchr(cache, '.') != NULL)
      ww-=10;   // when a dot is found, the last two chars will be printed using default font-> for now just deduct a few pixels
    fillRect(xx, yy, ww, hh, ST77XX_BLACK);
    strcpy(cache, value);
  }
  char buf[32];
  strncpy(buf, value, sizeof(buf));

  // find a format like dd.dC
  char *dot = strtok(buf, ".");
  // print the text
  setCursor(x, y);
  print(buf);

  // if a dot was found, the value was delimited and we need to print the rest
  if (dot != NULL)
    dot = strtok(NULL, ".");

  if (dot != NULL)
  {
    // get the dimensions of what we already have printed
    getTextBounds(buf, x, y, &xx, &yy, (uint16_t*) &ww, (uint16_t*) &hh);
    setFont(NULL);
    // print eenheid
    setCursor(xx+ww+7, yy);
    print(dot+1);
    // print decimal
    dot[1] = dot[0];
    dot[0] = '.';
    setCursor(x+ww+2, yy+hh/2+2);
    print(dot);
  } 
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
#define ALIGN_LEFT    0
#define ALIGN_MIDDLE  60
#define ALIGN_RIGHT   120
#define ROW_WATCHDOG  0     // 1 line
#define ROW_LOG       1     // 5 pixels heigth
#define ROW_DATETIME  15     // 6 pixels
#define ROW_MAIN      55
#define ROW_WATER     90
#define ROW_STATUS    125
#define LABEL_Y_OFFSET -26
#define COLOR_LABELS  0xCCCC    // https://rgbcolorpicker.com/565
#define COLOR_DATETIME  0xDDDD
#define COLOR_LINE1  0x9ED0
#define COLOR_LINE2  0x4CF8
#define COLOR_LINE3  0x4CF8   

char cache_date[32] = "";
char cache_time[32] = "";
char cache_outside[8] = "";
char cache_roomcur[8] = "";
char cache_roomset[8] = "";
char cache_target[8] = "";
char cache_outlet[8] = "";
char cache_inlet[8] = "";
char cache_modlvl[8] = "";

Periodic update_display(200);

bool Display::update(bool wifi_connected, bool mqtt_connected, int OT_errors)
{
  if (!update_display)
    return false;
  
  _watchdog();

  switch(_area++)
  {
// LABELS
    case 0:
    {
      _print(ALIGN_LEFT,    ROW_MAIN +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Outside", NULL);
      _print(ALIGN_MIDDLE,  ROW_MAIN +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Room", NULL);
      _print(ALIGN_RIGHT,   ROW_MAIN +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Target", NULL);
      _print(ALIGN_LEFT,    ROW_WATER +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Inlet", NULL);
      _print(ALIGN_MIDDLE,  ROW_WATER +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Outlet", NULL);
      _print(ALIGN_RIGHT,   ROW_WATER +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Setpnt", NULL);
      _print(ALIGN_LEFT,    ROW_STATUS +LABEL_Y_OFFSET, NULL, COLOR_LABELS, "Level", NULL);
    }
    break;
// TOP LINE    
    case 1:
    {
      DateTime now = rtc.now();
      // date
      char date[32] = "DDD DD MMM YYYY";
      _print(ALIGN_LEFT, ROW_DATETIME, NULL, COLOR_DATETIME, now.toString(date), cache_date);

      // time
      char time[32] = "hh:mm";
      _print(130, ROW_DATETIME, NULL, COLOR_DATETIME, now.toString(time), cache_time);
    }
    break;
// MAIN LINE
    case 2:
      // Outside temp
      _print(ALIGN_LEFT, ROW_MAIN, &FreeSansBold12pt7b, COLOR_LINE1, 
            SmartControl::instance()->outside.toString(1, true),
            cache_outside);
    break;
    case 3:
      // Room temp
      _print(ALIGN_MIDDLE, ROW_MAIN, &FreeSansBold12pt7b, COLOR_LINE1, 
            SmartControl::instance()->inside.toString(1, true),
            cache_roomcur);
    break;
    case 4:
      // Target temp -> is always valid (while invalid)
      _print(ALIGN_RIGHT, ROW_MAIN, &FreeSansBold12pt7b, COLOR_LINE1, 
            SmartControl::instance()->target.toString(1, true),
            cache_roomset);
    break;
// WATER LINE
    case 5:
      // Inlet Water temp
      _print(ALIGN_LEFT, ROW_WATER, &FreeSansBold12pt7b, COLOR_LINE2, 
            SmartControl::instance()->inlet.toString(1, true),
            cache_inlet);
    break;
    case 6:
      // Outlet Water temp
      _print(ALIGN_MIDDLE, ROW_WATER, &FreeSansBold12pt7b, COLOR_LINE2, 
            SmartControl::instance()->outlet.toString(1, true),
            cache_outlet);
    break;
    case 7:
      // Target water temp SETPOINT
      _print(ALIGN_RIGHT, ROW_WATER, &FreeSansBold12pt7b, COLOR_LINE2, 
            SmartControl::instance()->setpoint.toString(1, true),
            cache_target);
    break;
//STATUS LINE
    case 8:
    {
      // Modulation level
      char tmp[8];
      sprintf(tmp, "%.1f%%", SmartControl::instance()->ModLvl);
      _print(ALIGN_LEFT, ROW_STATUS, &FreeSansBold12pt7b, COLOR_LINE3, tmp, cache_modlvl);
    }
    break;
    case 9:
    {
      // Connection status
      setFont(NULL);
      fillRect(ALIGN_MIDDLE, ROW_STATUS, width()-ALIGN_MIDDLE, height()-ROW_STATUS+20, ST77XX_BLACK);
      setTextColor(COLOR_LINE3);
      setCursor(ALIGN_MIDDLE, ROW_STATUS-20);
      print(wifi_connected ? "WiFi:ok" : "WiFi:n/a");
      setCursor(ALIGN_MIDDLE, ROW_STATUS-8);
      print(mqtt_connected ? "MQTT:ok" : "MQTT:n/a");
      setCursor(ALIGN_RIGHT, ROW_STATUS-20);
      print("OT-err");
      setCursor(ALIGN_RIGHT, ROW_STATUS-8);
      print(OT_errors, DEC);
    }
    // no break to restart at the first;
    default:
      _area = 0;  
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
int wd_pos = 0;

void Display::_watchdog()
{
  //".oOo."  // ------------ // pattern = "|/-\\|/-\\";
  if (wd_pos >=32)
    wd_pos = 0;

  if (wd_pos < 16) {
    drawLine(wd_pos*10, 0, wd_pos*10+10, 0, ST77XX_RED);
  }
  else if (wd_pos < 32) {
    drawLine((wd_pos-16)*10, 0, (wd_pos-16)*10+10, 0, ST77XX_BLACK);
  }
  wd_pos++;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
Periodic log_balancer(500); // no logging within 500ms

void Display::log(const char *msg) 
{
//  if (!log_balancer)
//    return;
  setTextWrap(false);
  setFont(&Picopixel);
  fillRect(ALIGN_LEFT, ROW_LOG, width(), Picopixel.yAdvance+1, ST77XX_BLACK);
  setTextColor(ST77XX_RED);
  setCursor(ALIGN_LEFT, ROW_LOG+Picopixel.yAdvance-1);
  int lg = strlen(msg);
  if (lg <= 32)
    print(msg);
  else
  {
    char tmp[52];
    strcpy(tmp, msg+lg-32);
    print(tmp);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
