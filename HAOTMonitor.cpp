#include "HAOTMonitor.h"
#include <HAMqtt.h>
#include <String.h>
#include <DatedVersion.h>
DATED_VERSION(0, 9)
#define DEVICE_NAME  "SmartControl"
#define DEVICE_MODEL "OpenTherm SmartControl esp8266"
#define LOG_REMOTE
#define LOG_LEVEL 2
#include <Logging.h>
#include "SmartControl.h"

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
#define CONSTRUCT(var)          var(#var)
#define CONSTRUCT_P1(var)       var(#var, HABaseDeviceType::PrecisionP1)
#define CONSTRUCT_P2(var)       var(#var, HABaseDeviceType::PrecisionP2)

#define CONFIGURE_TEMP(var)     var.setName(#var); var.setDeviceClass("temperature"); var.setStateClass("measurement"); var.setIcon("mdi:thermometer"); var.setUnitOfMeasurement("°C")
#define CONFIGURE_INPUT(var)    var.setName(#var); var.setMin(0.0f);var.setMax(1.0f);var.setStep(0.05f); var.setMode(HANumber::ModeBox)

////////////////////////////////////////////////////////////////////////////////////////////
// singleton object
HAOTMonitor *_device = NULL;
HAOTMonitor *HAOTMonitor::instance() { return _device; };

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void settingsChanged(HANumeric number, HANumber* sender)
{
  if (_device == NULL)
    return;
  _device->_change_setting(number, sender);
}

void switchChanged(bool state, HASwitch* sender)
{
  if (_device == NULL)
    return;
  _device->_change_switch(state, sender);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
HAOTMonitor::HAOTMonitor()
: CONSTRUCT_P2(inside), CONSTRUCT_P2(target), CONSTRUCT_P2(setpoint)
, CONSTRUCT_P2(outside), CONSTRUCT_P2(inlet), CONSTRUCT_P2(outlet), CONSTRUCT_P2(modlvl)
, CONSTRUCT_P2(factor), CONSTRUCT_P2(factor_outside), CONSTRUCT_P2(factor_inside), CONSTRUCT_P2(factor_curve)
, CONSTRUCT(CH_enabled), CONSTRUCT(DHW_enabled), CONSTRUCT(Cooling_enabled), CONSTRUCT(OTC_enabled)
, CONSTRUCT(fault), CONSTRUCT(CH_mode), CONSTRUCT(DHW_mode), CONSTRUCT(Flame), CONSTRUCT(Cooling)
{
  _device = this;
  CONFIGURE_TEMP(inside);
  CONFIGURE_TEMP(setpoint);
  CONFIGURE_TEMP(outside);
  CONFIGURE_TEMP(inlet);
  CONFIGURE_TEMP(outlet);
  modlvl.setName("Modulation Level"); modlvl.setUnitOfMeasurement("%"); modlvl.setDeviceClass("power_factor"); modlvl.setStateClass("measurement");

  target.setName("target"); target.setIcon("mdi:thermometer"); target.setUnitOfMeasurement("°C");
  target.setMin(18.0f); target.setMax(25.0f); target.setStep(0.1f); target.setMode(HANumber::ModeBox);
  CONFIGURE_INPUT(factor_outside); 
  CONFIGURE_INPUT(factor_inside); 
  CONFIGURE_INPUT(factor_curve); 
  factor.setName("factor"); factor.setDeviceClass("power"); factor.setStateClass("measurement");

  target.onCommand(settingsChanged);
  factor_outside.onCommand(settingsChanged);
  factor_inside.onCommand(settingsChanged);
  factor_curve.onCommand(settingsChanged);

  CH_enabled.setName("Enable CH");
  DHW_enabled.setName("Enable DWH");
  Cooling_enabled.setName("Enable Cooling");
  OTC_enabled.setName("Enable OTC");
  CH_enabled.onCommand(switchChanged);
  DHW_enabled.onCommand(switchChanged);
  Cooling_enabled.onCommand(switchChanged);
  OTC_enabled.onCommand(switchChanged);

  fault.setName("Fault");
  CH_mode.setName("CH mode");
  DHW_mode.setName("DWH mode");
  Flame.setName("Heating");
  Cooling.setName("Cooling");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool HAOTMonitor::begin(const byte mac[6], HAMqtt *mqtt) 
{
  setUniqueId(mac, 6);
  setManufacturer("InnoVeer");
  setName(DEVICE_NAME);
  setSoftwareVersion(VERSION);
  setModel(DEVICE_MODEL);

  mqtt->addDeviceType(&inside);  
  mqtt->addDeviceType(&target);  
  mqtt->addDeviceType(&setpoint);  

  mqtt->addDeviceType(&outside);  
  mqtt->addDeviceType(&inlet);  
  mqtt->addDeviceType(&outlet);  
  mqtt->addDeviceType(&modlvl);  

  mqtt->addDeviceType(&factor);  
  mqtt->addDeviceType(&factor_outside);  
  mqtt->addDeviceType(&factor_inside);  
  mqtt->addDeviceType(&factor_curve);  

  mqtt->addDeviceType(&CH_enabled);
  mqtt->addDeviceType(&DHW_enabled);
  mqtt->addDeviceType(&Cooling_enabled);
  mqtt->addDeviceType(&OTC_enabled);
  mqtt->addDeviceType(&fault);
  mqtt->addDeviceType(&CH_mode);
  mqtt->addDeviceType(&DHW_mode);
  mqtt->addDeviceType(&Flame);
  mqtt->addDeviceType(&Cooling);
    
  // initialize with current values
  inside.setAvailability(false);
  setpoint.setAvailability(false);
  outside.setAvailability(false);
  inlet.setAvailability(false);
  outlet.setAvailability(false);
  factor.setCurrentValue(SmartControl::instance()->heating_curve.current_factor());

  // slave status
  CH_enabled.setCurrentState(SmartControl::instance()->operating_flags.enable_CH);
  DHW_enabled.setCurrentState(SmartControl::instance()->operating_flags.enable_DHW);
  Cooling_enabled.setCurrentState(SmartControl::instance()->operating_flags.enable_Cooling);
  OTC_enabled.setCurrentState(SmartControl::instance()->operating_flags.enable_OTC);
  fault.setCurrentState(SmartControl::instance()->status_flags.fault);
  CH_mode.setCurrentState(SmartControl::instance()->status_flags.CH_mode);
  DHW_mode.setCurrentState(SmartControl::instance()->status_flags.DHW_mode);
  Flame.setCurrentState(SmartControl::instance()->status_flags.Flame);
  Cooling.setCurrentState(SmartControl::instance()->status_flags.Cooling);

  // inputs
  target.setCurrentState(SmartControl::instance()->target.get());
  factor_outside.setCurrentState(SmartControl::instance()->heating_curve.factorA());
  factor_inside.setCurrentState(SmartControl::instance()->heating_curve.factorB());
  factor_curve.setCurrentState(SmartControl::instance()->heating_curve.factorC());

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void HAOTMonitor::_change_setting(HANumeric number, HANumber* sender)
{
  if (!number.isFloat()) {
    ERROR("HA MQTT: Invalid new setting value");
    return;
  }
  float newval = number.toFloat();

  if (sender == &target) {
    if (SmartControl::instance()->target.set(newval))
      INFO("new target temp set to %0.2f", newval);
    else
      ERROR("Could not change target temp to %0.2f", newval);
  }
  else if (sender == &factor_outside)
    SmartControl::instance()->heating_curve.factorA(newval);

  else if (sender == &factor_inside)
    SmartControl::instance()->heating_curve.factorB(newval);

  else if (sender == &factor_curve)
    SmartControl::instance()->heating_curve.factorC(newval);

  else
    ERROR("HA MQTT: Could not determine which setting to change");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void HAOTMonitor::_change_switch(bool state, HASwitch* sender)
{
  if (sender == &CH_enabled) {
    SmartControl::instance()->operating_flags.enable_CH = state;
    INFO("CH changed to %s", state? "on" :"off");
  }
  else if (sender == &DHW_enabled) {
    SmartControl::instance()->operating_flags.enable_DHW = state;
    INFO("DWH changed to %s", state? "on" :"off");
  }
  else if (sender == &Cooling_enabled) {
    SmartControl::instance()->operating_flags.enable_Cooling = state;
    INFO("Cooling changed to %s", state? "on" :"off");
  }
  else if (sender == &OTC_enabled) {
    SmartControl::instance()->operating_flags.enable_OTC = state;
    INFO("OTC changed to %s", state? "on" :"off");
  }
  else
    ERROR("HA MQTT: Could not determine which switch was turned");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
#define UPDATE_TEMP_SENSOR(var)   var.setAvailability(c->var.valid());  if (var.isOnline()) var.setValue(c->var.get())

bool HAOTMonitor::update()
{
  SmartControl *c = SmartControl::instance();
  if (c == NULL)
    return false;
  
  // update values without force (only update when changed)
  UPDATE_TEMP_SENSOR(inside);
  UPDATE_TEMP_SENSOR(setpoint);
  UPDATE_TEMP_SENSOR(outside);
  UPDATE_TEMP_SENSOR(outlet);
  UPDATE_TEMP_SENSOR(inlet);

  modlvl.setValue(c->ModLvl);
  factor.setValue(c->heating_curve.current_factor());
  target.setState(c->target.get());
  factor_outside.setState(c->heating_curve.factorA());
  factor_inside.setState(c->heating_curve.factorB());
  factor_curve.setState(c->heating_curve.factorC());

  CH_enabled.setState(c->operating_flags.enable_CH);
  DHW_enabled.setState(c->operating_flags.enable_DHW);
  Cooling_enabled.setState(c->operating_flags.enable_Cooling);
  OTC_enabled.setState(c->operating_flags.enable_OTC);
  fault.setState(c->status_flags.fault);
  CH_mode.setState(c->status_flags.CH_mode);
  DHW_mode.setState(c->status_flags.DHW_mode);
  Flame.setState(c->status_flags.Flame);
  Cooling.setState(c->status_flags.Cooling);
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


