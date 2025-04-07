#pragma once

#include <SunRise.h>
#include <OpenTherm.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <RunningAverage.h>
#include <Timer.h>

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
class Temperature
{
private:
  mutable char _to_string[8];  // buffer for toString
  float     _cur_val;           // current  value
  float     _min_val;           // minimum allowed value
  float     _max_val;           // maximum allowed value
  float     _max_diff_psec;     // max change per second (0.01 per sec = 6 degrees per min)
  float     _k;                 // factor of stddev in which a new value may differ (spike detection)
  uint16_t  _max_age;           // maximum age in minutes before N/A will be returned, and max time between entries in the statistics
  Timer     _age;               // age of last reading
  Periodic  _stat_interval;     // minimum interval between statistical entries
  Periodic  _longterm_stat_tmr;     // interval to update long term stat entries
  mutable RunningAverage _statistics;     // 10 valid values which are added each 30 seconds (5 minutes of data)
  mutable RunningAverage _longterm_stat;  // 10 average values added each 5 minutes (5 minutes of data)
public:
  Temperature(float value=0.0f, uint16_t max_age=0, float min=0.0f, float max=0.0f, float max_diff_psec=0.0f, float k=0.0f); // 0 value to disable
  bool set(float value, bool validate=true);
  float get() const;
  bool valid() const;
  float average() const;
  const char *toString(uint8_t precision, bool celcius=true) const; // celcius=true for adding C
  int trend() const; // negative for decline , 0 for stable, positive for incline
};

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
class HeatingCurve
{
private:
  float _factorA, _factorB, _factorC, _factor;
public:
  HeatingCurve();
  inline float current_factor() const { return _factor; }; 
  //returns current value, or if a valid (0<-->1.5) newval is given it will change the value and return the previous one
  float factorA(float newval=-1.0f);  // value used to keep the house warm, based on Ttarget - Toutside
  float factorB(float newval=-1.0f);  // value used to heat/cool the house, based on Ttarget - Tcurrent
  float factorC(float newval=-1.0f);  // the factor used for the curve, the leniar factor will be set to 1-curve by which the 20 and 0 degrees are aligned

  float calculate(Temperature *current, Temperature *target, Temperature *outside); // returns setpoint
};

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
struct OperatingFlags 
{
  bool enable_CH;         // CH enabled
  bool enable_DHW;        // DHW enabled
  bool enable_Cooling;    // Cooling enabled
  bool enable_OTC;        // Outside Temperature Control enabled
};

struct StatusFlags 
{
  bool fault;             // fault
  bool CH_mode;           // running for CH
  bool DHW_mode;          // running for WW
  bool Flame;             // heating
  bool Cooling;           // cooling
};

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
struct ConfigData {
  uint32_t magicNumber; // Magic number to validate data
  HeatingCurve    _curve;  // curve settings
  OperatingFlags  _flags;  // operating flags
};

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
class SmartControl : public OpenTherm
{
friend void handleResponse(unsigned long response, OpenThermResponseStatus state);
private:
  byte               _T_board[8];   // address of the DS18
  byte               _T_extern[8];   // address of the DS18
  OneWire            _wire;
  DallasTemperature  _dallas;
  SunRise            _sunrise;   // get sunrise and sunset times
  Periodic           _auto_resetter;
  Timer              _timer_switch_onoff;
  Periodic           _analyse_time;

  void _handleResponse(unsigned long response, OpenThermResponseStatus state);
public:
  SmartControl();
  static SmartControl *instance();
  int communication_errors;
  OperatingFlags  operating_flags;
  StatusFlags     status_flags;
  HeatingCurve    heating_curve;

  bool begin();
  bool loop();
  bool analysis();
  bool reset();
  float RoomCur();    // huidige kamer temperatuur
  float RoomSet();    // doel kamer temperatuur
  float SetPoint();   // aanvraag watertemperatuur

  Temperature inside;    // Troom    Current room temperature
  Temperature target;  // Ttarget  Target room temperature
  Temperature setpoint;// Tset     Calculated Water temperature

  Temperature inlet;   // Tr   boiler invoer / huis uitvoer
  Temperature outlet;  // Ta   boiler uitvoer / huis invoer
  Temperature outside; // Tout buiten temperatuur

  float ModLvl;       // Modulation level
  float Pressure;     // CH water pressure
};


