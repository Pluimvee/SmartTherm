#include <ESP8266WiFi.h>
#include <HAMqtt.h>
#include <HADevice.h>
#include <Clock.h>
#include <Timer.h>
#include <ArduinoOTA.h>
#include <DatedVersion.h>
DATED_VERSION(0, 2)
#define LOG_LEVEL 2
#include <Logging.h>
#include "secrets.h"
#include "OTDataObjects.h"
#include "HAOTMonitor.h"
#include "SmartControl.h"
#include "Display.h"

////////////////////////////////////////////////////////////////////////////////////////////
// Configuration
const char* sta_ssid      = STA_SSID;
const char* sta_pswd      = STA_PASS;

const char* mqtt_server   = "192.168.2.170";   // test.mosquitto.org"; //"broker.hivemq.com"; //6fee8b3a98cd45019cc100ef11bf505d.s2.eu.hivemq.cloud";
int         mqtt_port     = 1883;             // 8883;
const char* mqtt_user     = MQTT_USER;
const char *mqtt_passwd   = MQTT_PASS;

////////////////////////////////////////////////////////////////////////////////////////////
// Global instances
WiFiClient          socket;
HAOTMonitor         ha_monitor;
SmartControl        controller;
HAMqtt              mqtt(socket, ha_monitor, SENSOR_COUNT);      // Home Assistant MTTQ
Clock               rtc;                        // A real (software) time clock
Display             display;

////////////////////////////////////////////////////////////////////////////////////////////
// Callback functions
// Remote logging using MQTT
//void LOG_CALLBACK(const char *msg) { mqtt.publish("SmartTherm/log", msg, true); }
void LOG_CALLBACK(const char *msg) { 
  mqtt.publish("SmartTherm/log", msg, true); 
  Display::instance()->log(msg);
}
//void LOG_CALLBACK(const char* txt) {  Serial.println(txt); }
//void LOG_CALLBACK(const char *msg) { Display::instance()->log(msg); }

////////////////////////////////////////////////////////////////////////////////////////////
// MQTT Connect
void mqtt_connect() {
  INFO("Opentherm Gateway v%s saying hello\n", VERSION);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Connect to the STA network
bool wifi_connect() 
{ 
  if (((WiFi.getMode() & WIFI_STA) == WIFI_STA) && WiFi.isConnected())
    return true;

  INFO("Wifi connecting to %s.", sta_ssid);
  WiFi.begin(sta_ssid, sta_pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  INFO("WiFi connected with IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////
Timer _sync_clock;  //

void sync_clock() 
{
  if (!_sync_clock.passed())
    return;

  INFO("Synchonizing clock");
  if (!rtc.ntp_sync())
    return;
  
  _sync_clock.set(4*60*60*1000);  // resync in 4hrs
  INFO("Clock synchronized to %s\n", rtc.now().timestamp().c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void setup() 
{
  // Begin display drivers
  display.begin();

//  Serial.begin(115200);
  INFO("\nOpenTherm Controller Version %s", VERSION);
  wifi_connect();

  sync_clock(); 

  // start MQTT to enable remote logging asap
  INFO("Connecting to MQTT server %s", mqtt_server);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  ha_monitor.begin(mac, &mqtt);
  mqtt.onConnected(mqtt_connect);           // register function called when newly connected
  mqtt.begin(mqtt_server, mqtt_port, mqtt_user, mqtt_passwd);  // 

  // Begin opentherm libraries for master and slave
  INFO("Initialize Opentherm Shields");
  controller.begin();

  INFO("Initialize OTA\n");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("OpenTherm-SmartControl");
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() {
    INFO("[%s] - Starting remote software update",
          rtc.now().timestamp(DateTime::TIMESTAMP_TIME).c_str());
  });
  ArduinoOTA.onEnd([]() {
    INFO("[%s] - Remote software update finished",
          rtc.now().timestamp(DateTime::TIMESTAMP_TIME).c_str());
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ERROR("Error remote software update");
  });
  ArduinoOTA.begin();
  INFO("Setup complete");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
Periodic refresh(1000);

void loop() 
{
  // ensure we are still connected
  wifi_connect();
  // handle any OTA requests
  ArduinoOTA.handle();
  // once in awhile sync the clock
  sync_clock(); 
  // handle OT thermometer
  controller.loop();
  // handle any OTA requests
  ArduinoOTA.handle();
  // update display
  display.update(WiFi.isConnected(), mqtt.isConnected(), controller.communication_errors);
  // handle any OTA requests
  ArduinoOTA.handle();
  // update Home Assistant
  ha_monitor.update(); 
  // handle MQTT
  mqtt.loop();
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

