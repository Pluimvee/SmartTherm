#include "SmartControl.h"
#include <Clock.h>
#define LOG_REMOTE
#define LOG_LEVEL 3
#include <Logging.h>
#include <Timer.h>

////////////////////////////////////////////////////////////////////////////////////////////
// Configuration
const int mInPin = D1; //D6; 
const int mOutPin = D2; //D5;
const int tempSensorPin = D4; //D7;         // D7 contains the DS18b20 one-wire temp sensor
// Amsterdam's latitude and longitude
float latitude = 52.3676;
float longitude = 4.9041;

extern Clock rtc;

#define CALIBRATE_TROOM     (-1.3f)         // DS sensor calibration
#define ERROR_RESETTER      (15*60*1000)    // 15 minutes to retry a DataId and to reset the comm-err counter
#define ANALYSE_TIME        (10*1000)       // once every 10 seconds we analyse the heating state
#define ANTIPENDEL_TIMEFRAME (30*60*1000)   // no turning on/off within a 30 minutes timeframe
#define DS_SENSOR_EXTERNAL  0               // "\x28\xB4\x51\x0C\x00\x00\x00\x8F"
#define DS_SENSOR_BOARD     1               // "\x28\xBF\x7A\x28\xA1\x22\x06\x51"
////////////////////////////////////////////////////////////////////////////////////////////
// 
#define LOG_MESSAGE(req, resp)    DEBUG("[%s] - % 3.3d = T:%s, data %04.4X -> B:%s, data %0.4f", \
                                  rtc.now().timestamp(DateTime::TIMESTAMP_TIME).c_str(),  \
                                  OpenTherm::getDataID(req), \
                                  OpenTherm::messageTypeToString(OpenTherm::getMessageType(req)), \
                                  OpenTherm::getUInt(req), \
                                  OpenTherm::messageTypeToString(OpenTherm::getMessageType(resp)), \
                                  OpenTherm::getFloat(resp));

////////////////////////////////////////////////////////////////////////////////////////////
// singleton object
SmartControl *_controller = 0;
SmartControl *SmartControl::instance() { return _controller; }

SmartControl::SmartControl()
: OpenTherm(mInPin, mOutPin, false)               // opentherm shield
, _wire(tempSensorPin), _dallas(&_wire)           // dalles inside temperature and print temperature
, _auto_resetter(ERROR_RESETTER)                  // auto reset
, _analyse_time(ANALYSE_TIME)
, inside(  20.0f, 5,  10.0f, 40.0f, 0.02f, 3.0f)  // inside can only change slow
, outside( 10.0f, 5, -15.0f, 40.0f, 0.02f, 3.0f)  // outside can only change slow
, target(  20.5f, 0,  18.0f, 25.0f)               // does not expire, and no spike detection needed
, setpoint(20.0f, 5,  10.0f, 55.0f)               // no spike detection needed
, inlet(   20.0f, 5,  10.0f, 55.0f, 1.0f, 3.0f)   // during defrosts the inlet can change fast
, outlet(  20.0f, 5,  10.0f, 55.0f, 1.0f, 3.0f)   // during defrosts the outlet can change fast
{
  _controller = this;

  memset(_T_board, 0, sizeof(_T_board));
  memset(_T_extern, 0, sizeof(_T_extern));

  communication_errors = 0;
  _sunrise.queryTime = 0;
  operating_flags.enable_CH      = false;    // disable heating per default
  operating_flags.enable_DHW     = false;    // disable DHW heating
  operating_flags.enable_Cooling = false;    // disable cooling per default
  operating_flags.enable_OTC     = false;    // we are using OTC, the slave may not ;)
}

////////////////////////////////////////////////////////////////////////////////////////////
// Global functions / interupt handlers
void IRAM_ATTR mHandleInterrupt() {
    if (_controller == 0)
      return;
    _controller->handleInterrupt();
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void handleResponse(unsigned long response, OpenThermResponseStatus state)
{
    if (_controller == 0)
      return;
    _controller->_handleResponse(response, state);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool SmartControl::begin()
{
  OpenTherm::begin(mHandleInterrupt, handleResponse); // for handling the response messages
  
  _dallas.begin();
  if (_dallas.getDeviceCount() < 1) 
    return false;

  if (!_dallas.getAddress(_T_extern, DS_SENSOR_EXTERNAL))    // get address of the first (and only) DS sensor on this shield
  {
    ERROR("Unable to find a DS sensor");
    return false;
  }
  if (!_dallas.getAddress(_T_board, DS_SENSOR_BOARD))
  {
    ERROR("Unable to find a second DS sensor");
  }
  else
  {
    INFO("2 sensors found: %02.2X%02.2X%02.2X%02.2X%02.2X%02.2X%02.2X%02.2X and %02.2X%02.2X%02.2X%02.2X%02.2X%02.2X%02.2X%02.2X",
    _T_extern[0], _T_extern[1], _T_extern[2], _T_extern[3], _T_extern[4], _T_extern[5], _T_extern[6], _T_extern[7], 
    _T_board[0], _T_board[1], _T_board[2], _T_board[3], _T_board[4], _T_board[5], _T_board[6], _T_board[7]
    );
//    delay(1000);
  }
  _timer_switch_onoff.set(ANTIPENDEL_TIMEFRAME);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Measure the inside temp and return the value
////////////////////////////////////////////////////////////////////////////////////////////
float SmartControl::RoomCur() 
{
  float t = 0.0f;

  if (!_dallas.validAddress(_T_extern) || !_dallas.isConnected(_T_extern))
    _dallas.getAddress(_T_extern, DS_SENSOR_EXTERNAL); 

  if (_dallas.requestTemperaturesByAddress(_T_extern))   // send command to sensors to measure
  {
    t = _dallas.getTempC(_T_extern);
    t += CALIBRATE_TROOM;
    inside.set(t);
  }
  // if measurement wasnt successfull, and the existing value got expired, we should skip
  // sending the OT message
  return inside.get();
}

////////////////////////////////////////////////////////////////////////////////////////////
// In here we may want to place some logic depending on the time
// or we can leave it up to Home Assitant, or both
////////////////////////////////////////////////////////////////////////////////////////////
float SmartControl::RoomSet() {
  // as the heatingcurve takes the average, we will add the current value on each loop
  // slowely filling the statistics and moving towards the target in 10 cycles of 30seconds
  float t = target.get();
  target.set(t, false);  // no need to validate
  return t;  
}

////////////////////////////////////////////////////////////////////////////////////////////
// Toe te voegen mbt SetPoint:
//  1. Verhoging met maximaal 1 graad per x-minuten om te voorkomen dat de WP gaat 'boosten' 
//     (is mogelijk hetzelfde als 5)
//  2. Geen verhoging Setpoint tijdens hoge electra prijzen ()
//  3. Geen verlaging SetPoint tijdens lage electra prijzen (ga maar lekker door)
//  
//  5. Tsetpoint max x graden boven Tinlet indien Toutside rond vriespunt en Troom max 1 graad onder TroomSet 
//  6. Iets van een COP < 3 dan is gas goedkoper dus activeer CV ketel door
//      a) nachtlimiet
//      b) seperate aansturing dmv OT
//  7. Niet verlagen TSetPoint indien TroomCur boven TroomSet && Toutside < 10 && na 16:00 
//     (het is vast de bedoeling om het even op te stoken met de gashaard)
////////////////////////////////////////////////////////////////////////////////////////////
float SmartControl::SetPoint()
{
  setpoint.set(heating_curve.calculate(&inside, &target, &outside));

  if (operating_flags.enable_CH || operating_flags.enable_Cooling)
    return setpoint.get();

  // WeHeat bug: force OT TSet to 0 when CH is disabled
  return 0;    
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
uint16_t getStatus() {
  uint8_t flags = 0;
  SmartControl *c = _controller;
  if (c == NULL)
    return flags;

  flags |= (c->operating_flags.enable_CH     <<0);  // CH enabled
  flags |= (c->operating_flags.enable_DHW    <<1);  // DHW enabled
  flags |= (c->operating_flags.enable_Cooling<<2);  // Cooling enabled
  flags |= (c->operating_flags.enable_OTC    <<3);  // OTC enabled (outside temp control)
  flags |= (0 <<4);  // CH2 disabled
  
  return flags << 8;
}
void setStatus(unsigned long data) {
  SmartControl *c = _controller;
  if (c == NULL)
    return;

  c->status_flags.fault     = data & 0x01;
  c->status_flags.CH_mode   = data & 0x02;
  c->status_flags.DHW_mode  = data & 0x04;
  c->status_flags.Flame     = data & 0x08;
  c->status_flags.Cooling   = data & 0x10;
  bool CH2_mode = data & 0x20;
  bool diag     = data & 0x40;

  DEBUG("Flags F:%d CH:%d DHW:%d Flame:%d Cool:%d CH2:%d Diag:%d", 
    c->status_flags.fault?1:0,
    c->status_flags.CH_mode?1:0,
    c->status_flags.DHW_mode?1:0,
    c->status_flags.Flame?1:0,
    c->status_flags.Cooling?1:0,
    CH2_mode?1:0,
    diag?1:0
    );
}

////////////////////////////////////////////////////////////////////////////////////////////
uint16_t getTSetPoint() {
  return OpenTherm::temperatureToData(_controller->SetPoint());
}
uint16_t getTRoom() {
  return OpenTherm::temperatureToData(_controller->RoomCur());
}
uint16_t getTRoomSet() {
  return OpenTherm::temperatureToData(_controller->RoomSet());
}

////////////////////////////////////////////////////////////////////////////////////////////
void setTOutside(unsigned long data) {
  _controller->outside.set(OpenTherm::getFloat(data));
}
void setTInlet(unsigned long data) {
  _controller->inlet.set(OpenTherm::getFloat(data));
}
void setTOutlet(unsigned long data) {
  _controller->outlet.set(OpenTherm::getFloat(data));
}
void setModLvl(unsigned long data) {
  _controller->ModLvl = OpenTherm::getFloat(data);
}
void setPressure(unsigned long data) {
  _controller->Pressure = OpenTherm::getFloat(data);
}
void setSlaveVersion(unsigned long data) {
  uint16_t version = OpenTherm::getUInt(data);
  uint8_t version_major = version >> 8;
  uint8_t version_minor = version & 0xFF;
//  DEBUG("Version of boiler %d-%d", version_major, version_minor);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
  int failures;
  OpenThermMessageID msgId;
  OpenThermMessageType msgType;
  uint16_t (* getdata)();
  void (* setdata)(unsigned long);
 
} FUNCTION_MAP;

FUNCTION_MAP script[] = {
  { 0, OpenThermMessageID::Status,        OpenThermMessageType::READ_DATA,  getStatus,      setStatus   },
  { 0, OpenThermMessageID::TrSet,         OpenThermMessageType::WRITE_DATA, getTRoomSet,    NULL        },
  { 0, OpenThermMessageID::Tr,            OpenThermMessageType::WRITE_DATA, getTRoom,       NULL        },
  { 0, OpenThermMessageID::TSet,          OpenThermMessageType::WRITE_DATA, getTSetPoint,   NULL        },
  { 0, OpenThermMessageID::Toutside,      OpenThermMessageType::READ_DATA,  NULL,           setTOutside },
  { 0, OpenThermMessageID::Tret,          OpenThermMessageType::READ_DATA,  NULL,           setTInlet   },
  { 0, OpenThermMessageID::Tboiler,       OpenThermMessageType::READ_DATA,  NULL,           setTOutlet  },
  { 0, OpenThermMessageID::RelModLevel,   OpenThermMessageType::READ_DATA,  NULL,           setModLvl   },
  { 0, OpenThermMessageID::CHPressure,    OpenThermMessageType::READ_DATA,  NULL,           setPressure },
  { 0, OpenThermMessageID::SlaveVersion,  OpenThermMessageType::READ_DATA,  NULL,           setSlaveVersion }
};

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
FUNCTION_MAP *cmd = script;
Timer send_tm;
unsigned long last_request;
unsigned long last_response;

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void SmartControl::_handleResponse(unsigned long response, OpenThermResponseStatus state)
{
  last_response = response;
  LOG_MESSAGE(last_request, last_response);

  FUNCTION_MAP *c = script; 
  while (c->msgId != OpenTherm::getDataID(response) && c->msgId != OpenThermMessageID::SlaveVersion)
    c++;

  if (!OpenTherm::isValidResponse(response))
  {
    ERROR("Invalid response message received: %s", OpenTherm::statusToString(state));
    switch (c->msgId) 
    {
    case OpenThermMessageID::Status:
    case OpenThermMessageID::TSet:
      break;
    default:
      if (state == OpenThermResponseStatus::INVALID)
        c->failures++;
      if (state == OpenThermResponseStatus::TIMEOUT) {
        communication_errors++;
        if (communication_errors > 99)
          communication_errors = 99;
      }
      break;
    }
  }
  else
  {
    if (c->setdata)
      c->setdata(response);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////
/*  SWITCHING ON LOGIC
                            ==================== trend of TSet / 30 min ===================
    target	current	error 	-0,5  -0,4  -0,3	-0,2	-0,1	0  0,1   0,2   0,3   0,4   0,5
    21	    22	     1.0											
    21	    21,8	   0,8										                                          ON
    21	    21,6	   0,6									                                      ON	  ON
    21	    21,4	   0,4								                                  ON	  ON	  ON
    21	    21,2	   0,2							                              ON	  ON	  ON	  ON
    21	    21	     0.0					                            ON	  ON	  ON	  ON	  ON
    21	    20,8	  -0,2						                      ON	ON	  ON	  ON	  ON	  ON
    21	    20,6	  -0,4					                  ON	  ON	ON	  ON	  ON	  ON	  ON
    21	    20,4	  -0,6				                ON  ON	  ON	ON	  ON	  ON	  ON  	ON
    21	    20,2	  -0,8			            ON	  ON  ON	  ON	ON	  ON	  ON	  ON	  ON
    21	    20	    -1.0	        	ON	  ON	  ON  ON	  ON	ON	  ON	  ON  	ON    ON
                              ON		ON	  ON	  ON  ON	  ON	ON	  ON	  ON	  ON    ON
    ==> ON = (2*trend > error)
*/
////////////////////////////////////////////////////////////////////////////////////////////
// use 1 decimal precision
float round1(float value) {
  return round(10.0f * value) / 10.0f;  // rounding to 1 decimal
}

////////////////////////////////////////////////////////////////////////////////////////////
bool SmartControl::set_operating_mode()
{
  if (!_analyse_time)
    return false; // no change in heating or cooling

  // TODO: add logic to handle invalid temps, like invalid outside, inlet or other boiler temp

  // log some analysis
  DEBUG("T/trend >>> TRoom= %0.1f/%0.1f, TOutside= %0.1f/%0.1f, TSet= %0.1f/%0.1f, Ta= %0.1f/%0.1f, Tret= %0.1f/%0.1f", 
    inside.get(),   inside.trend(), 
    outside.get(),  outside.trend(), 
    setpoint.get(), setpoint.trend(), 
    outlet.get(),   outlet.trend(), 
    inlet.get(),    inlet.trend()
  );

  if (!_timer_switch_onoff.passed())       // last switching must been awhile back 
    return false; // no change in heating or cooling

  // detetmine if COOLING should be turned ON
  if (operating_flags.enable_Cooling == false     // when cooling is off
    && inside.valid()  && inside.get()  > 25.0f   // and inside is above 25 degrees
    && outside.valid() && outside.get() > 25.0f   // and outside is above 25 degrees
  ) {
    INFO("Switch ON Cooling, as Tinside %0.2f and Toutside %0.2f are above 25 degrees", inside.get(), outside.get());
    operating_flags.enable_Cooling = true;  // enable cooling
    _timer_switch_onoff.set(ANTIPENDEL_TIMEFRAME);
    return true;
  }
  // detetmine if COOLING should be turned OFF
  if (operating_flags.enable_Cooling == true        // when cooling enabled
      && inside.valid() && inside.get() < 25.0f     // and inside is below 25 degrees
  ) {
    INFO("Switch OFF Cooling, as Tinside %0.2f reached below 25 degrees", inside.get());
    operating_flags.enable_Cooling = false;  // disable cooling
    _timer_switch_onoff.set(ANTIPENDEL_TIMEFRAME);
    return true;
  }
  // detetmine if HEATING should be turned ON
  if (operating_flags.enable_CH == false          // when heating is off
      && inside.valid() && inside.get() < 25.0f   // and inside is below 25 degrees
      && target.valid() && setpoint.valid()
  ) {
    // error: positive when inside above target
    float error = round1(inside.get() - target.get()); 
    // trend: positive when setpoint is rising, which is when either inside or outside are declining (degrees/30 minutes)
    float trend  = round1(setpoint.trend());
    // meaning: when trend is high-incline we start heating early, even when error is still large positive
    if (2*trend > error)
    {  
      INFO("Switch ON Heating, Tinside error (%0.1f) < 2x trend-TSet (%0.1f)", error, trend);
      operating_flags.enable_CH = true;  // enable heating
      _timer_switch_onoff.set(ANTIPENDEL_TIMEFRAME);
      return true;
    }
    INFO("No need for heating: inside error (%0.1f) is above 2x trend-TSet (%0.1f)", error, trend);
  }
  // detetmine if HEATING should be turned OFF
  if (operating_flags.enable_CH == true       // heating enabled
    && setpoint.valid() && inlet.valid()
    && round1(inlet.get() - setpoint.get()) > 0.1f  // when Tr = 0.2 above Tset
  ) 
  {
    INFO("Switching off Heatpump, as Tset %0.2f reached below Tr %0.2f", setpoint.get(), inlet.get());
    operating_flags.enable_CH = false;  // disable heating
    _timer_switch_onoff.set(ANTIPENDEL_TIMEFRAME);
    return true;
  }
  return false; // no change in heating or cooling
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool SmartControl::loop() 
{
  process();  // handle any response messages 
  
  if (_auto_resetter) 
    reset();

  if (isReady() && send_tm.passed())
  {
    if (cmd->failures < 5)      // skip this cmd in script due to non-supported
    {
      last_request = OpenTherm::buildRequest(cmd->msgType, cmd->msgId, (cmd->getdata) != NULL ? cmd->getdata() : 0x00);

      if (!sendRequestAync(last_request))
        ERROR("OT Send error, status: %d", status);

      send_tm.set(2000);
    }
    if (cmd->msgId == OpenThermMessageID::SlaveVersion)
      cmd = script;
    else
      cmd++;
  }
  set_operating_mode();  // check if we need to switch on/off the heating or cooling
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool SmartControl::reset()
{
  for (FUNCTION_MAP *c = script; c->msgId != OpenThermMessageID::SlaveVersion; c++)
    c->failures = 0;

  communication_errors = 0;
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


