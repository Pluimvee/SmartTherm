#ifndef HOME_ASSIST_SMARTTHERM
#define HOME_ASSIST_SMARTTHERM

#include <HADevice.h>
#include <device-types\HABinarySensor.h>
#include <device-types\HASwitch.h>
#include <device-types\HASensorNumber.h>
#include <device-types\HANumber.h>

#define SENSOR_COUNT 25    // Total number of sensors, with some slack

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
class HAOTMonitor : public HADevice 
{
friend void settingsChanged(HANumeric number, HANumber* sender);
friend void switchChanged(bool state, HASwitch* sender);

private:
  void _change_setting(HANumeric number, HANumber* sender);
  void _change_switch(bool state , HASwitch* sender);
public:
  HAOTMonitor();
  static HAOTMonitor *instance();

  // Master to slave
  HASensorNumber  inside;
  HANumber  target;         // adjustable
  HASensorNumber  setpoint;
  // Slave to master
  HASensorNumber  outside;
  HASensorNumber  inlet;
  HASensorNumber  outlet;
  HASensorNumber  modlvl;  // modulation level (eg power)
  
  // Heating-curve parameters (adjustable)
  HASensorNumber  factor;    // current applied factor
  HANumber  factor_outside; // angle of curve based on outside
  HANumber  factor_inside;  // angle adjustment based on inside
  HANumber  factor_curve;   // curve

  // flags
  HASwitch       CH_enabled;
  HASwitch       DHW_enabled;
  HASwitch       Cooling_enabled;
  HASwitch       OTC_enabled;
  HABinarySensor fault;
  HABinarySensor CH_mode;
  HABinarySensor DHW_mode;
  HABinarySensor Flame;
  HABinarySensor Cooling;

  bool begin(const byte mac[6], HAMqtt *mqqt);
  bool update();                                   
};

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

#endif