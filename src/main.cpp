#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <iostream>
#include <string>
#include "time.h"
// SSID and Password
// const char *ssid = "JIEYIAN@unifi";
// const char *password = "NGBRO123";

const char *ssid = "Galaxy A23E242";
const char *password = "twrg0778";

// The REST API endpoint - Change the IP Address
const char *base_rest_url = "https://zvtk1gj0-5000.asse.devtunnels.ms/";
// const char *base_rest_url = "https://irrigationsystem.pythonanywhere.com/";

// NTP server to request epoch time
const char *ntpServer = "pool.ntp.org";

// Variable to save current epoch time
unsigned long epochTime;

WiFiClientSecure client;
HTTPClient http;

// StaticJsonDocument<500> doc;

// Read interval
unsigned long previousMillis = 0;
const long readInterval = 8000;
unsigned long lastphReadingsTakenTime = 0;

char dhtObjectId[30];
char dhtObjectId_2[30];

#define DHTPIN 22   // Digital pin connected to the DHT sensor
#define DHTPIN_2 23 // Digital pin connected to the DHT sensor

#define DHTTYPE DHT22
#define DHTTYPE_2 DHT22

const int MoisturePin1 = A6; // pin 34

// ldr pin
const int ldrPin = A0;

// pH pin
const int phPin = A7; // pin 35

// EC pin
const int ecPin = A5; // pin 39

// TDS meter isolation pins
const int ecIso_PowerPin = 21;
const int ecIso_GndPin = 19;

// Relay Pins
const int RELAY_PIN_1 = 25;
const int RELAY_PIN_2 = 33;

// Pump & L298N pin
const int in1Pin = 17;
const int in2Pin = 15;
const int EN = 18;

// Intake and Exhuast Fan Pin
int inputFan_1 = 26;
int exhaustFan_1 = 32;
int count = 0;
int count2 = 0;

// Relay limit in integer
int intThreshold;
int u_intThreshold;

// Current fan speed
int currInFanSpeed;
int currExFanSpeed;
int latestInRpm;
int latestExRpm;

// Last humidifier triggered time
unsigned long lastTriggerTime = 0;

// Variable to store average internal readings
float aInterTemp = 0;
float aInterHumidity = 0;
float aExterTemp = 0;
float aExterHumidity = 0;

float inTemp;
float exTemp;
float inHumidity;
float exHumidity;

// ph and ec read time
int lastReadecTime = 0;

// check pump operation time
int lastCheckPumpTime = 0;

// pH
float pH_value;
float voltage;
float actual_ph;
float average_actual_ph;

// EC
float ec_value;
float ec_voltage;
float actual_ec;
float average_actual_ec;

// ENVIRONMENT STATE COMPARISON
const char *TempThresholdState;
const char *ExTempCompareState;
const char *HumidThresholdState;
const char *ExHumidityCompareState;

// Check brightness time
int lastcheckBrightnessTime = 0;
int ignore_startup_alert = 0;

// Store brightness value
float previous_lux = 0;

// System startup variable
int first_test_1 = 0;
int first_test_2 = 0;
int first_test_3 = 0;
int first_test_4 = 0;
int first_test_5 = 0;
int first_humidifier_reminder = 0;

// Moisture sensor
int moisturePercentage1;
int actual_moisturePercentage;
int avg_humidity;

// humidifier
int last_refill_humidifier_time = 0;

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN_2, DHTTYPE_2);

// Struct to read DHT22 readings
struct DHT22Readings
{
  float temperature;
  float humidity;
};

struct IdealTemperatureRange
{
  char sensor_id[10];
  char lower_limit_v[5];
  char upper_limit_v[5];
};

struct IdealphRange
{
  char sensor_id[10];
  char pHlimit[5];
  char u_pHlimit[5];
};

struct IdealecRange
{
  char sensor_id[10];
  char eclimit[5];
  char u_eclimit[5];
};

struct HumidifierInterval
{
  char sensor_id[10];
  char interval[5];
};

// Struct to represent humidifer trigger limit record stored in MongoDB
struct HumidifierThreshold
{
  char sensor_id[10];
  char limit[3];
  char u_limit[3];
};

// pH data record stored in MongoDB
struct phData
{
  char sensor_id[10];
  char ph[5];
};

// pH data record stored in MongoDB
struct ecData
{
  char sensor_id[10];
  char ec[5];
};

struct burstModeToggleSwitch
{
  char sensor_id[10];
  char burst_mode[5];
};

struct inFanSpeed
{
  char sensor_id[10];
  char lv_one[20];
  char lv_two[20];
  char lv_three[20];
  char lv_four[20];
  char lv_five[20];
};

struct exFanSpeed
{
  char sensor_id[10];
  char lv_one[20];
  char lv_two[20];
  char lv_three[20];
  char lv_four[20];
  char lv_five[20];
};

struct currentFanSpeed
{
  char sensor_id[10];
  char inspeed[10];
  char exspeed[10];
};

struct pumpState
{
  char button_id[15];
  char button_state[15];
};

struct waterTestButton
{
  char button_id[15];
  char button_pressed_time[15];
};

// Function that gets current epoch time and returns as a string
const char *getTimeAsString()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return "";
  }
  time(&now);

  // Max length of the string representation
  const int bufferSize = 20;          // Adjust based on your requirement
  static char timeString[bufferSize]; // Static so it persists after the function call

  // Using snprintf to convert time_t to string
  snprintf(timeString, bufferSize, "%lld", (long long)now);

  return timeString;
}

// Size of the JSON document. Use the ArduinoJSON JSONAssistant
const int JSON_DOC_SIZE = 384;

// HTTP GET Call
StaticJsonDocument<JSON_DOC_SIZE> callHTTPGet(const char *sensor_id)
{
  char rest_api_url[200];
  // Calling our API server
  sprintf(rest_api_url, "%sapi/sensors?sensor_id=%s", base_rest_url, sensor_id);
  Serial.println(rest_api_url);

  http.useHTTP10(true);
  http.begin(rest_api_url);
  http.addHeader("Content-Type", "application/json");
  http.GET();

  StaticJsonDocument<JSON_DOC_SIZE> doc;
  // Parse response
  DeserializationError error = deserializeJson(doc, http.getStream());

  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    Serial.println(http.errorToString(http.GET()));
    return doc;
  }

  http.end();
  return doc;
}

// water quality test button pressed time
waterTestButton getButtonPressedTime(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *buttonId = item["sensor_id"];
    const char *button_pressed_time = item["button_pressed_time"];

    waterTestButton WaterTestButtonDataSet = {};
    strcpy(WaterTestButtonDataSet.button_id, buttonId);
    strcpy(WaterTestButtonDataSet.button_pressed_time, button_pressed_time);
    return WaterTestButtonDataSet;
  }
  return {}; // or RELAY{}
}

// pump state
pumpState getpumpState(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *buttonId = item["sensor_id"];
    const char *button_state = item["button_state"];

    pumpState pumpStateDataSet = {};
    strcpy(pumpStateDataSet.button_id, buttonId);
    strcpy(pumpStateDataSet.button_state, button_state);
    return pumpStateDataSet;
  }
  return {}; // or RELAY{}
}

// Get ideal temp range
IdealTemperatureRange getIdealTempRange(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *lower_limit = item["lower_limit_value"];
    const char *upper_limit = item["upper_limit_value"];

    IdealTemperatureRange idealTempDataSet = {};
    strcpy(idealTempDataSet.sensor_id, sensorId);
    strcpy(idealTempDataSet.lower_limit_v, lower_limit);
    strcpy(idealTempDataSet.upper_limit_v, upper_limit);
    return idealTempDataSet;
  }
  return {}; // or RELAY{}
}

// Get ideal pH range
IdealphRange getIdealpHRange(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *lower_limit = item["pHlimit"];
    const char *upper_limit = item["u_pHlimit"];

    IdealphRange idealphDataSet = {};
    strcpy(idealphDataSet.sensor_id, sensorId);
    strcpy(idealphDataSet.pHlimit, lower_limit);
    strcpy(idealphDataSet.u_pHlimit, upper_limit);
    return idealphDataSet;
  }
  return {}; // or RELAY{}
}

// Get ideal ec range
IdealecRange getIdealecRange(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *lower_limit = item["EClimit"];
    const char *upper_limit = item["u_EClimit"];

    IdealecRange idealecDataSet = {};
    strcpy(idealecDataSet.sensor_id, sensorId);
    strcpy(idealecDataSet.eclimit, lower_limit);
    strcpy(idealecDataSet.u_eclimit, upper_limit);
    return idealecDataSet;
  }
  return {}; // or RELAY{}
}

// Get trigger relay threshold for humidifer
HumidifierThreshold getHumidifierLimit(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *limits = item["limit"];
    const char *upper_limits = item["u_limit"];

    HumidifierThreshold humidifierTemp = {};
    strcpy(humidifierTemp.sensor_id, sensorId);
    strcpy(humidifierTemp.limit, limits);
    strcpy(humidifierTemp.u_limit, upper_limits);
    return humidifierTemp;
  }
  return {}; // or RELAY{}
}

// Get humidifier operating interval
HumidifierInterval getHumidifierInterval(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {};
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *interval = item["interval"];

    HumidifierInterval humidifierIntervalData = {};
    strcpy(humidifierIntervalData.sensor_id, sensorId);
    strcpy(humidifierIntervalData.interval, interval);
    return humidifierIntervalData;
  }
  return {};
}

// Get pH value
phData getphValue(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *ph_value = item["ph"];

    phData readyphData = {};
    strcpy(readyphData.sensor_id, sensorId);
    strcpy(readyphData.ph, ph_value);
    return readyphData;
  }
  return {}; // or RELAY{}
}

// Get EC value
ecData getecValue(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *ec_value = item["ec"];

    ecData readyecData = {};
    strcpy(readyecData.sensor_id, sensorId);
    strcpy(readyecData.ec, ec_value);
    return readyecData;
  }
  return {}; // or RELAY{}
}

// GET current fan speed
currentFanSpeed getCurrentFanSpeed(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *in_fan_speed = item["inspeed"];
    const char *ex_fan_speed = item["exspeed"];

    currentFanSpeed readyfanData = {};
    strcpy(readyfanData.sensor_id, sensorId);
    strcpy(readyfanData.inspeed, in_fan_speed);
    strcpy(readyfanData.exspeed, ex_fan_speed);
    return readyfanData;
  }
  return {}; // or RELAY{}
}

burstModeToggleSwitch getHumidifierModeStates(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *mode_state = item["burst_mode"];

    burstModeToggleSwitch humidifierModeData = {};
    strcpy(humidifierModeData.sensor_id, sensorId);
    strcpy(humidifierModeData.burst_mode, mode_state);
    return humidifierModeData;
  }
  return {}; // or RELAY{}
}

inFanSpeed getIntakeFanSpeed(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *first_level = item["lv_one"];
    const char *second_level = item["lv_two"];
    const char *third_level = item["lv_three"];
    const char *fourth_level = item["lv_four"];
    const char *fifth_level = item["lv_five"];

    inFanSpeed inFanData = {};
    strcpy(inFanData.sensor_id, sensorId);
    strcpy(inFanData.lv_one, first_level);
    strcpy(inFanData.lv_two, second_level);
    strcpy(inFanData.lv_three, third_level);
    strcpy(inFanData.lv_four, fourth_level);
    strcpy(inFanData.lv_five, fifth_level);
    return inFanData;
  }
  return {}; // or RELAY{}
}

exFanSpeed getExhaustFanSpeed(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return {}; // or RELAY{}
  for (JsonObject item : doc.as<JsonArray>())
  {
    const char *sensorId = item["sensor_id"];
    const char *exhaust_first_level = item["lv_one"];
    const char *exhuast_second_level = item["lv_two"];
    const char *exhaust_third_level = item["lv_three"];
    const char *exhaust_fourth_level = item["lv_four"];
    const char *exhaust_fifth_level = item["lv_five"];

    exFanSpeed exFanData = {};
    strcpy(exFanData.sensor_id, sensorId);
    strcpy(exFanData.lv_one, exhaust_first_level);
    strcpy(exFanData.lv_two, exhuast_second_level);
    strcpy(exFanData.lv_three, exhaust_third_level);
    strcpy(exFanData.lv_four, exhaust_fourth_level);
    strcpy(exFanData.lv_five, exhaust_fifth_level);
    return exFanData;
  }
  return {}; // or RELAY{}
}

int convertStatus(const char *value)
{
  if (strcmp(value, "HIGH") == 0)
  {
    Serial.println("Setting LED to HIGH");
    return HIGH;
  }
  else
  {
    Serial.println("Setting LED to LOW");
    return LOW;
  }
}

// Send DHT22 readings using HTTP POST
void sendDHT22Readings()
{
  char rest_api_url[200];
  float humidity_initial = dht.readHumidity();
  delay(2000);
  float humidity = 1.1119 * humidity_initial - 17.125;
  // Read temperature as Celsius (the default)
  float temperatureInC_initial = dht.readTemperature();
  delay(2000);
  float temperatureInC = 1.2638 * temperatureInC_initial - 6.067;

  // Calling our API server

  if (isnan(humidity))
  {
    humidity = 0;
    Serial.println("Nan humidity reading, converted to 0 which will be ignored");
  }

  if (isnan(temperatureInC))
  {
    temperatureInC = 0;
    Serial.println("Nan temperature reading, converted to 0 which will be ignored");
  }
  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  JsonObject readings = doc.createNestedObject("readings");
  doc["sensor_id"] = "dht22_1";
  readings["temperature"] = temperatureInC;
  readings["humidity"] = humidity;
  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// Send external dht22 reading to database
void sendDHT22Readings_2()
{
  char rest_api_url[200];
  float humidity_2_initial = dht2.readHumidity();
  delay(2000);
  float humidity_2 = 0.9698 * humidity_2_initial - 3.1216;

  // Read temperature as Celsius (the default)
  float temperatureInC_2_initial = dht2.readTemperature();
  float temperatureInC_2 = 1.1042 * temperatureInC_2_initial - 1.7263;

  // Calling our API server

  if (isnan(humidity_2))
  {
    humidity_2 = 0;
    Serial.println("Nan humidity reading for external dht22, converted to 0 which will be ignored");
  }

  if (isnan(temperatureInC_2))
  {
    temperatureInC_2 = 0;
    Serial.println("Nan temperature reading for external dht22, converted to 0 which will be ignored");
  }

  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  JsonObject readings = doc.createNestedObject("readings");
  doc["sensor_id"] = "dht22_2";
  readings["temperature"] = temperatureInC_2;
  readings["humidity"] = humidity_2;
  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

//  Send pH readings using HTTP POST
void sendphReadings()
{
  char rest_api_url[200];

  // Calling our API server

  if (isnan(average_actual_ph))
  {
    average_actual_ph = 0;
    Serial.println("Nan ph reading, converted to 0 which will be ignored");
  }

  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  doc["sensor_id"] = "ph_sensor";
  doc["ph"] = average_actual_ph;

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

//  Send EC readings using HTTP POST
void sendecReadings()
{
  char rest_api_url[200];
  // Read temperature as Celsius (the default)

  // Calling our API server

  if (isnan(average_actual_ec))
  {
    average_actual_ec = 0;
    Serial.println("Nan ec reading, converted to 0 which will be ignored");
  }

  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  doc["sensor_id"] = "ec_sensor";
  doc["ec"] = average_actual_ec;

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// Send current fan speed to update chart
void sendFanSpeed()
{
  char rest_api_url[200];
  // Read temperature as Celsius (the default)

  // Calling our API server

  if (isnan(currInFanSpeed))
  {
    currInFanSpeed = 0;
    Serial.println("Nan intake fan reading, converted to 0 which will be ignored");
  }
  else if (isnan(currExFanSpeed))
  {
    currExFanSpeed = 0;
    Serial.println("Nan intake fan reading, converted to 0 which will be ignored");
  }
  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  doc["sensor_id"] = "c_fan_speed";
  doc["inspeed"] = currInFanSpeed;
  doc["exspeed"] = currExFanSpeed;

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// POST latest intake fan speed to database
void sendLatestIntakeFanSpeed(int intake_fan_level, int exhaust_fan_level)
{
  char rest_api_url[200];
  // Calling our API server
  sprintf(rest_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  // JsonObject readings = doc.createNestedObject("readings");
  doc["sensor_id"] = "c_fan_speed";
  doc["inspeed"] = intake_fan_level;
  doc["exspeed"] = exhaust_fan_level;
  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// send latest water quality test up time
// void sendLatestQualityTestUpTime(const char *test_up_time)
// {
//   char rest_api_url[200];
//   // Calling our API server
//   sprintf(rest_api_url, "%sapi/qualitytestuptime/quality_test", base_rest_url);
//   Serial.println(rest_api_url);

//   // Prepare our JSON data
//   String jsondata = "";
//   StaticJsonDocument<JSON_DOC_SIZE> doc;
//   // JsonObject readings = doc.createNestedObject("readings");
//   doc["test_id"] = "quality_test";
//   doc["test_up_time"] = test_up_time;

//   serializeJson(doc, jsondata);
//   Serial.println("JSON Data...");
//   Serial.println(jsondata);

//   http.begin(client, rest_api_url);
//   http.addHeader("Content-Type", "application/json");

//   // Send the PUT request
//   int httpResponseCode = http.PUT(jsondata);
//   if (httpResponseCode > 0)
//   {
//     String response = http.getString();
//     Serial.println(httpResponseCode);
//     Serial.println(response);
//   }
//   else
//   {
//     Serial.print("Error on handling PUT: ");
//     Serial.println(httpResponseCode);
//     Serial.println(http.errorToString(http.PUT(jsondata)));

//     http.end();
//   }
// }

// update latest pump state
void sendLatestPumpState()
{
  char rest_api_url[200];

  // Calling our API server

  sprintf(rest_api_url, "%sapi/test/6553769e3f9e48b6f5be442e", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  doc["sensor_id"] = "test_button";
  doc["button_state"] = "0";

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.PUT(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// Sending latest ph reading for user to monitor during water quality check
void updateLatestph(float ph)
{
  char rest_api_url[200];
  // Calling our API server
  sprintf(rest_api_url, "%sapi/sensors/655380633f9e48b6f5ca6707", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  // JsonObject readings = doc.createNestedObject("readings");
  doc["sensor_id"] = "latest_ph";
  doc["value"] = ph;

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.PUT(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// update latest ec during water quality test
void updateLatestec(int ec)
{
  char rest_api_url[200];
  // Calling our API server
  sprintf(rest_api_url, "%sapi/sensors/655380463f9e48b6f5ca4584", base_rest_url);
  Serial.println(rest_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc;
  // JsonObject readings = doc.createNestedObject("readings");
  doc["sensor_id"] = "latest_ec";
  doc["value"] = ec;

  serializeJson(doc, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, rest_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.PUT(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// Get DHT22 ObjectId
void getDHT22ObjectId(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return;
  for (JsonObject item : doc.as<JsonArray>())
  {
    Serial.println(item);
    const char *objectId = item["_id"]["$oid"]; // "dht22_1"
    strcpy(dhtObjectId, objectId);

    return;
  }
  return;
}

// Get external DHT22 ObjectId
void getDHT22ObjectId_2(const char *sensor_id)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc = callHTTPGet(sensor_id);
  if (doc.isNull() || doc.size() > 1)
    return;
  for (JsonObject item : doc.as<JsonArray>())
  {
    Serial.println(item);
    const char *objectId = item["_id"]["$oid"]; // "dht22_1"
    strcpy(dhtObjectId_2, objectId);

    return;
  }
  return;
}

// Read DHT22 sensor
DHT22Readings readDHT22()
{
  float humidity = dht.readHumidity();
  delay(2000);
  // Read temperature as Celsius (the default)
  float temperatureInC = dht.readTemperature();
  // Calibration
  float actual_temperatureInC = 1.2638 * temperatureInC - 6.067;
  delay(2000);
  float actual_humidity = 1.1119 * humidity - 17.125;
  return {actual_temperatureInC, actual_humidity};
}

// Read external DHT22 sensor
DHT22Readings readDHT22_2()
{
  float humidity_2 = dht2.readHumidity();
  delay(2000);
  // Read temperature as Celsius (the default)
  float temperatureInC_2 = dht2.readTemperature();
  float actual_temperatureInC_2 = 1.1042 * temperatureInC_2 - 1.7263;
  delay(2000);
  float actual_humidity_2 = 0.9698 * humidity_2 - 3.1216;
  return {actual_temperatureInC_2, actual_humidity_2};
}

void sendMoistureReading()
{

  char combined_api_url[200];
  int sum_humidity_reading = 0;
  for (int i = 0; i < 10; i++)
  {
    Serial.println(analogRead(A6));
    moisturePercentage1 = 100 - ((analogRead(MoisturePin1) / 4095.00) * 100);
    delay(1000);
    actual_moisturePercentage = map(moisturePercentage1, 30, 70, 0, 100); // change the range to 0-100
    sum_humidity_reading += moisturePercentage1;
    Serial.print("The humidity of outlet 3 is: ");
    Serial.print(moisturePercentage1);
    Serial.println("%");
    delay(2000);
  }
  avg_humidity = sum_humidity_reading / 10;
  Serial.print("The average humidity at outlet 3 is: ");
  Serial.print(avg_humidity);
  Serial.println("%");
  // moisturePercentage2 = 100 - ((analogRead(MoisturePin2) / 4095.00) * 100);
  // int moisturePercentage3 = 100 - ((analogRead(MoisturePin3) / 4095.00) * 100);

  // Serial.print("Soil Humidity : ");
  // Serial.println(moisturePercentage2);
  // Serial.println(moisturePercentage3);
  // Calling our API server
  sprintf(combined_api_url, "%sapi/sensors", base_rest_url);
  Serial.println(combined_api_url);

  // Prepare our JSON data
  String jsondata = "";
  StaticJsonDocument<JSON_DOC_SIZE> doc2;
  JsonObject readings = doc2.createNestedObject("readings");
  doc2["sensor_id"] = "cMoistureSensor";
  readings["soil_moisture_1"] = avg_humidity;
  // readings["soil_moisture_2"] = moisturePercentage2;
  // readings["soil_moisture_3"] = moisturePercentage3;

  serializeJson(doc2, jsondata);
  Serial.println("JSON Data...");
  Serial.println(jsondata);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST(jsondata);
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on handling PUT: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(http.PUT(jsondata)));

    http.end();
  }
}

// Send email about temperature cannot adjust to ideal range
void postTempCantAdjustEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%stempCantAdjust", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send temp could not adjust alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postHumidierCantHelp()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%shumidifierCantHelpAdjustTemp", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send humidifier cant help adjust  temp alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

// Humidifier cant adjust to ideal range for too long
void postHumidityCantAdjustEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%shumidifierCantAdjustToIdeal", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send humidifier cant adjust to ideal alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

// post ph lv email to user
void postpHHighEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%sphhigh", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ph high alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postpHLowEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%sphlow", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ph low alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postpHIdealEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%sphideal", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ph ideal alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postecHighEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%sechigh", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ec high alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postecLowEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%seclow", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ec low alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postecIdealEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%secideal", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send ec ideal alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postBrightnessLowEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%slowbrightness", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send brightness low alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void postBrightnessIdealEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%sidealbrightness", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send brightness ideal alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void sendPumpNotWorkingEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%spumpOperation", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the PUT request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on sending pump not working alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

// Send email to remind users refill water for humidifier
void postRefillHumidifierWaterEmail()
{
  char combined_api_url[200];

  sprintf(combined_api_url, "%shumidifierwater", base_rest_url);
  Serial.println(combined_api_url);

  http.begin(client, combined_api_url);
  http.addHeader("Content-Type", "application/json");

  // Send the POST request
  int httpResponseCode = http.POST("");
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on send refill water for humidifier alert email: ");
    Serial.println(httpResponseCode);

    http.end();
  }
}

void counter()
{
  count++;
}

void counter2()
{
  count2++;
}

void setup()
{
  Serial.begin(115200);
  for (uint8_t t = 2; t > 0; t--)
  {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //  Start DHT Sensor readings
  dht.begin();
  dht2.begin();

  configTime(0, 0, ntpServer);

  //  Get the ObjectId of the DHT22 sensor
  getDHT22ObjectId("dht22_1");
  getDHT22ObjectId_2("dht22_2");

  // Setup Relay
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(MoisturePin1, INPUT);

  // Fan
  pinMode(inputFan_1, OUTPUT);
  pinMode(exhaustFan_1, OUTPUT);
  analogWrite(inputFan_1, 0);
  analogWrite(exhaustFan_1, 0);

  attachInterrupt(digitalPinToInterrupt(27), counter, RISING);  // Input fan interrupt pin
  attachInterrupt(digitalPinToInterrupt(22), counter2, RISING); // Exhuast fan interrupt pin

  // ph pin and ec pin
  pinMode(phPin, INPUT);
  pinMode(ecPin, INPUT);

  // LDR
  pinMode(ldrPin, INPUT);

  // Isolation pins
  pinMode(ecIso_GndPin, OUTPUT);
  pinMode(ecIso_PowerPin, OUTPUT);

  digitalWrite(ecIso_PowerPin, HIGH);
  digitalWrite(ecIso_GndPin, LOW);

  // Submersible pump
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(EN, OUTPUT);

  digitalWrite(in1Pin, LOW);
  digitalWrite(in2Pin, LOW);
  analogWrite(EN, 255);
}

void loop()
{

  Serial.println("-------CHECKING BRIGHTNESS-------");
  int checkBrightnessTime = millis();
  if ((checkBrightnessTime - lastcheckBrightnessTime >= 24 * 60 * 60 * 1000) || first_test_1 == 0 || first_test_1 == 1)
  {
    // Allow system to perform test when it is just started
    first_test_1 = first_test_1 + 1;
    float brightnessSum = 0;
    for (int i = 0; i < 10; i++)
    {
      float raw_brightness_value = analogRead(ldrPin);
      Serial.println(raw_brightness_value);
      float ldrVoltage = (raw_brightness_value / 4095) * 3.3;
      brightnessSum += ldrVoltage;
      Serial.print("The voltage of LDR is: ");
      Serial.println(ldrVoltage);
      delay(2000);
    }
    float average_brightness_voltage = brightnessSum / 10;
    Serial.println(brightnessSum);
    Serial.println(average_brightness_voltage);
    float lux = 1.9289 * (exp(2.9662 * average_brightness_voltage));
    Serial.print("The lux value is: ");
    Serial.println(lux);

    if ((abs(lux - previous_lux) > 1000 || abs(lux - previous_lux) <= 1000) && ignore_startup_alert == 0)
    {
      ignore_startup_alert = ignore_startup_alert + 1;
      previous_lux = lux;
      Serial.println("Ignoring the send low or ideal brightness alert email request due to system just started");
    }

    else if (abs(lux - previous_lux) > 1000 && ignore_startup_alert != 0)
    {
      postBrightnessLowEmail();
      Serial.println("Low brightness alert email is sent");
      lastcheckBrightnessTime = millis();
    }

    else if (abs(lux - previous_lux) <= 1000 && ignore_startup_alert != 0)
    {
      postBrightnessIdealEmail();
      Serial.println("Ideal brightness email sent");
      lastcheckBrightnessTime = millis();
    }
  }

  else
  {
    Serial.println("Check brightness task on cooldown");
  }

  Serial.println("-------CHECKING BRIGHTNESS DONE-------");
  Serial.println(" ");
  Serial.println(" ");
  delay(2000);

  Serial.println("-------UPLOADING LATEST TEMPERATURE AND HUMIDITY DATA TO DATABASE STARTS-------");
  // Send internal DHT22 sensor readings
  // Locate the ObjectId of our DHT22 document in MongoDB
  Serial.println("Sending latest internal DHT22 readings");
  DHT22Readings readings = readDHT22();
  Serial.print("Temperature : ");
  Serial.println(readings.temperature);
  Serial.print("Humidity : ");
  Serial.println(readings.humidity);
  delay(2000);
  // Check if any reads failed and exit early (to try again).
  if (isnan(readings.humidity) || isnan(readings.temperature))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  sendDHT22Readings();

  Serial.println("---------------");
  // Send external DHT22 sensor readings
  Serial.println("Sending latest external DHT22 readings");
  DHT22Readings external_readings = readDHT22_2();
  Serial.print("Temperature : ");
  Serial.println(external_readings.temperature);
  Serial.print("Humidity : ");
  Serial.println(external_readings.humidity);
  delay(2000);
  // Check if any reads failed and exit early (to try again).
  if (isnan(external_readings.humidity) || isnan(external_readings.temperature))
  {
    Serial.println(F("Failed to read from external DHT sensor!"));
  }
  sendDHT22Readings_2();
  Serial.println("-------UPLOADING LATEST TEMPERATURE AND HUMIDITY DATA TO DATABASE ENDS-------");
  delay(2000);
  Serial.println("");
  Serial.println("");

  // Humidifier loop
  Serial.println("-------GETTING THE MOISTURE SENSOR READINGS STARTS HERE-------");
  int pumpOperationReadTime = millis();

  if (first_test_5 == 0) // check every hour
  {
    Serial.println("System is just started, pump operation is checked.");
    first_test_5 = first_test_5 + 1;
    sendMoistureReading();
    if (avg_humidity < 5)
    {
      Serial.println("Humidity percentage at outlet number 3 is abnormal.");
      sendPumpNotWorkingEmail();
    }

    else
    {
      Serial.println("The pump is working.");
    }
  }

  else if (pumpOperationReadTime - lastCheckPumpTime >= 1 * 60 * 60 * 1000)
  {
    sendMoistureReading();
    pumpOperationReadTime = lastCheckPumpTime;
    if (avg_humidity < 30)
    {
      Serial.println("Humidity percentage at outlet number 3 is abnormal.");
      sendPumpNotWorkingEmail();
    }
    else
    {
      Serial.println("The pump is working.");
    }
  }

  else
  {
    Serial.println("Still in cooldown. Pump operation check delayed until cooldown completes.");
  }

  Serial.println("-------GETTING THE MOISTURE SENSOR READINGS DONE-------");
  delay(2000);
  Serial.println("");
  Serial.println("");

  Serial.println("-------GETTING THE IDEAL HUMIDITY RANGE SET BY USERS-------");

  HumidifierThreshold thresholdSetup = getHumidifierLimit("relay_limit");

  intThreshold = std::stoi(thresholdSetup.limit);
  std::cout << "The initial lower limit of ideal humidity range is: " << intThreshold << "%" << std::endl;

  u_intThreshold = std::stoi(thresholdSetup.u_limit);
  std::cout << "The initial upper limit of ideal humidity range is: " << u_intThreshold << "%" << std::endl;

  delay(2000);
  Serial.println("");
  // Hardcoded humidity range to avoid looping
  Serial.println("The humidity ideal range is increased by 10 (+/- 5 for upper/lower limit) to avoid constant looping");
  int l_humidity_threshold = intThreshold - 5;
  int u_humidity_threshold = u_intThreshold + 5;

  Serial.print("The final lower limit of ideal humidity range is: ");
  Serial.print(l_humidity_threshold);
  Serial.println("%");
  Serial.print("The final upper limit of ideal humidity range is: ");
  Serial.print(u_humidity_threshold);
  Serial.println("%");

  Serial.println("");
  Serial.println("-------GETTING THE IDEAL HUMIDITY RANGE ENDS HERE-------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  Serial.println("-----------READ TEMPERATURE IDEAL RANGE STARTS HERE---------------");

  IdealTemperatureRange idealTempSetup = getIdealTempRange("ideal_temp");

  // Convert temperature limit to integer
  int l_temp_threshold = std::stoi(idealTempSetup.lower_limit_v);
  std::cout << "The lower limit of ideal temp range is: " << l_temp_threshold << "°C" << std::endl;

  int u_temp_threshold = std::stoi(idealTempSetup.upper_limit_v);
  std::cout << "The upper limit of ideal temp range is: " << u_temp_threshold << "°C" << std::endl;

  Serial.println("-----------READ TEMPERATURE IDEAL RANGE ENDS HERE---------------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  Serial.println("-------ADD HUMIDIFIER WATER REMINDER STARTS HERE-------");
  int refill_water_reminder = millis();

  if (first_humidifier_reminder == 0)
  {
    first_humidifier_reminder += 1;
    postRefillHumidifierWaterEmail();
    Serial.println("Reminder to refill humidifier water is sent through email.");
  }

  else if (refill_water_reminder - last_refill_humidifier_time >= 12 * 60 * 60 * 1000) // remind every 12 hours
  {
    last_refill_humidifier_time = refill_water_reminder;
    postRefillHumidifierWaterEmail();
    Serial.println("Reminder to refill humidifier water is sent through email.");
  }

  else
  {
    Serial.println("Reminder to refill water for humidifier is on cooldown.");
  }
  Serial.println("-------ADD HUMIDIFIER WATER REMINDER ENDS HERE-------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  
  // pH
  Serial.println("-----------PH TASK STARTS HERE---------------");
  unsigned long currentpHMillis = millis();
  if ((currentpHMillis - lastphReadingsTakenTime >= 24 * 60 * 60 * 1000) || first_test_2 == 0) // 24hours
  {
    first_test_2 = first_test_2 + 1;
    // off the tds meter first
    digitalWrite(ecIso_PowerPin, HIGH);
    digitalWrite(ecIso_GndPin, LOW);
    lastphReadingsTakenTime = currentpHMillis;

    Serial.println("-----------READ pH IDEAL RANGE STARTS HERE---------------");
    IdealphRange idealphSetup = getIdealpHRange("ph_sensor_limit");

    // Convert pH limit to integer
    int l_ph_threshold = std::stoi(idealphSetup.pHlimit);
    std::cout << "The lower limit of ideal pH range is: " << l_ph_threshold << std::endl;

    int u_ph_threshold = std::stoi(idealphSetup.u_pHlimit);
    std::cout << "The upper limit of ideal pH range is: " << u_ph_threshold << std::endl;

    Serial.println("-----------READ pH IDEAL RANGE ENDS HERE---------------");
    Serial.println("");
    Serial.println("");
    // Turning on pump
    Serial.println("Starting submersible pump");
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);

    delay(30000);

    float pHsum = 0;
    Serial.println("actual pH value obtained through linear formula");
    for (int p = 0; p < 5; p++)
    {
      float pH_value = analogRead(phPin);
      float voltage = pH_value * (3.3 / 4095);
      float actual_ph = -5.2141 * voltage + 21.165;
      Serial.print("The ph value is: ");
      Serial.println(actual_ph);
      pHsum += actual_ph;
      delay(3000);
    }
    Serial.println(pHsum);
    average_actual_ph = pHsum / 5;
    Serial.print("The current average pH value is: ");
    Serial.println(average_actual_ph);

    // Turning off pump
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
    sendphReadings();
    Serial.println("pH value uploaded to database");

    if (average_actual_ph > u_ph_threshold)
    {
      Serial.println("Sending ph high alert email");
      postpHHighEmail();
      Serial.println("ph high alert email sent");
    }

    else if (average_actual_ph < l_ph_threshold)
    {
      Serial.println("Sending ph low alert email");
      postpHLowEmail();
      Serial.println("ph low alert email sent");
    }

    else
    {
      Serial.println("Sending ph ideal email");
      postpHIdealEmail();
      Serial.println("ph ideal email sent");
    }
  }

  else
  {
    Serial.println("reading pH task on cooldown ");
  }
  Serial.println("-----------PH TASK ENDS HERE---------------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  // EC
  Serial.println("----------EC TASK STARTS HERE---------------");

  int ecReadTime = millis();

  // wait for 24 hours
  if (ecReadTime - lastReadecTime >= 24 * 60 * 60 * 1000 || first_test_3 == 0) // 24 hours
  {
    first_test_3 = first_test_3 + 1;
    digitalWrite(ecIso_PowerPin, LOW);
    digitalWrite(ecIso_GndPin, HIGH);
    delay(3000);
    lastReadecTime = ecReadTime;

    Serial.println("-----------READ EC IDEAL RANGE STARTS HERE---------------");

    IdealecRange idealecSetup = getIdealecRange("ec_sensor_limit");

    // Convert EC limit to integer
    int l_ec_threshold = std::stoi(idealecSetup.eclimit);
    std::cout << "The lower limit of ideal EC range is: " << l_ec_threshold << std::endl;

    int u_ec_threshold = std::stoi(idealecSetup.u_eclimit);
    std::cout << "The upper limit of ideal EC range is: " << u_ec_threshold << std::endl;

    Serial.println("-----------READ EC IDEAL RANGE ENDS HERE---------------");
    Serial.println("Starting pump");
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
    delay(30000);
    int ecsum = 0;
    for (int x = 0; x < 10; x++)
    {
      float ec_value = analogRead(ecPin);
      float ec_voltage = ec_value * (3.3 / 4095);
      Serial.print("Actual PPM Value: ");
      float actual_ec = 330.97 * ec_voltage - 0.2043;
      Serial.println(actual_ec);
      ecsum += actual_ec;
      delay(3000);
    }
    average_actual_ec = ecsum / 10;
    Serial.print("The current PPM value is: ");
    Serial.print(average_actual_ec);
    Serial.println(" ppm");
    sendecReadings();
    Serial.println("Latest EC value is uploaded to database");
    digitalWrite(ecIso_PowerPin, HIGH);
    digitalWrite(ecIso_GndPin, LOW);
    // Turning off pump
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
    delay(2000);
    // 1750ppm = 3500 microSiemen/cm
    if (average_actual_ec > u_ec_threshold)
    {
      Serial.println("Sending ec high alert email");
      postecHighEmail();
      Serial.println("ec high alert email sent");
    }

    // 600ppm = 1200 microSiemen/cm
    else if (average_actual_ec < l_ec_threshold)
    {
      Serial.println("Sending ec low alert email");
      postecLowEmail();
      Serial.println("ec low alert email sent");
    }

    else
    {
      Serial.println("Sending ec ideal email");
      postecIdealEmail();
      Serial.println("ec ideal email sent");
    }
  }

  else
  {
    Serial.println("reading EC task on cooldown ");
  }
  Serial.println("-----------EC TASK ENDS HERE---------------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  Serial.println("-----------WATER QUALTIY TEST STARTS HERE---------------");
  pumpState pumpStateSetup = getpumpState("test_button");
  Serial.println("The latest pump state retrieved is: ");
  Serial.println(pumpStateSetup.button_state);
  delay(1000);
  int intButtonState = std::stoi(pumpStateSetup.button_state);
  bool exitQualityTestOuterLoop = false;
  if (exitQualityTestOuterLoop == false && intButtonState == 1)
  {
    int qualityTestUptime = 0;
    std::cout << "The current pump state is: " << intButtonState << " (On)" << std::endl;
    Serial.println("Turning on pump");
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
    delay(45000);
    while (intButtonState == 1)
    {
      // Turn on ec sensor to test EC first since EC sensor is shorter than pH sensor
      digitalWrite(ecIso_PowerPin, LOW);
      digitalWrite(ecIso_GndPin, HIGH);
      delay(2000);
      for (int i = 0; i < 5; i++)
      {
        float ec_value = analogRead(ecPin);
        delay(1000);
        float ec_voltage = ec_value * (3.3 / 4095);
        Serial.print("The latest PPM Value is: ");
        float actual_ec = 330.97 * ec_voltage - 0.2043;
        Serial.println(actual_ec);
        updateLatestec(actual_ec);
        delay(10000);
      }
      digitalWrite(ecIso_PowerPin, LOW);
      digitalWrite(ecIso_GndPin, HIGH);
      delay(4000);
      for (int i = 0; i < 5; i++)
      {
        pH_value = analogRead(phPin);
        voltage = pH_value * (3.3 / 4095);
        actual_ph = -5.2141 * voltage + 21.165;
        Serial.print("The latest ph value is: ");
        Serial.println(actual_ph);
        updateLatestph(actual_ph);
        delay(10000);
      }
      const char *epochTimeString = getTimeAsString();
      // Using snprintf to convert time_t to string
      Serial.print("The epoch time now is: ");
      Serial.println(epochTimeString);
      // sendLatestQualityTestUpTime(epochTimeString);
      int esp32testuptime = std::stoi(epochTimeString);
      delay(3000);
      waterTestButton waterTestButtonSetup = getButtonPressedTime("start_test_button");
      Serial.print("The water test button is last pressed at: ");
      Serial.println(waterTestButtonSetup.button_pressed_time);
      int intButtonPressedTime = std::stoi(waterTestButtonSetup.button_pressed_time);
      if (esp32testuptime - intButtonPressedTime >= 20)
      {
        Serial.print("The value after deduction is: ");
        Serial.println(esp32testuptime - intButtonPressedTime);
        sendLatestPumpState();
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, LOW);
        Serial.println("The water quality test time limit has been reached,pump is turned off");
        exitQualityTestOuterLoop = true;
        break;
      }

      else
      {
        Serial.println("The water quality test time limit has not been reached yet.");
      }
      delay(8000);
      pumpState pumpStateSetup_2 = getpumpState("test_button");
      intButtonState = std::stoi(pumpStateSetup_2.button_state);
      Serial.print("The button state after one round of testing is: ");
      Serial.println(intButtonState);
    }
    Serial.println("Water qualtiy test is done / quality test time limit is reached, pump is turned off");
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
    // sendLatestQualityTestUpTime("0");
  }

  else
  {
    Serial.println("Quality test is not started by user.");
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, LOW);
  }
  Serial.println("-----------WATER QUALTIY TEST ENDS HERE---------------");
  Serial.println("");
  Serial.println("");
  delay(2000);

  // Humidifier interval
  Serial.println("-----------READ HUMIDIFIER OPERATING INTERVAL STARTS HERE---------------");
  // Read humidifier operating interval
  HumidifierInterval humidifierSetup = getHumidifierInterval("humidifier_interval");

  // Convert interval to integer
  int intInterval = std::stoi(humidifierSetup.interval);
  std::cout << "The operating interval of humidifier is: " << intInterval << " hour(s)" << std::endl;
  Serial.println("-----------READ HUMIDIFIER OPERATING INTERVAL ENDS HERE---------------");
  delay(2000);

  Serial.println("-----------TEMPERATURE AND HUMIDITY TASKS START HERE---------------");
  // Check if at least x hours have passed
  unsigned long currentTime = millis();
  unsigned long timePassed = currentTime - lastTriggerTime;
  // Convert interval obtained from database form hours to miliseconds
  int intervalInMilis = intInterval * 60 * 60 * 1000;
  // int intervalInMilis = 0;

  // Check whether current time have passed the interval
  if (timePassed >= intervalInMilis || first_test_4 == 0)
  {
    first_test_4 = first_test_4 + 1;
    Serial.println("-----------CALCULATING AVERAGE TEMPERATURE AND HUMIDITY READINGS---------------");

    // Obtaining average in,ex temp and humidity
    for (int i = 0; i < 5; i++)
    {
      inTemp = 1.2638 * dht.readTemperature() - 6.067;
      delay(2000);
      aInterTemp += inTemp;
    }

    float finalAverageInTemp = aInterTemp / 5;
    Serial.print("The current average temperature inside greenhouse is: ");
    Serial.print(finalAverageInTemp);
    Serial.println("°C");

    for (int i = 0; i < 5; i++)
    {
      inHumidity = 1.1119 * dht.readHumidity() - 17.125;
      delay(2000);
      aInterHumidity += inHumidity;
    }

    float finalAverageInHumidity = aInterHumidity / 5;
    Serial.print("The current average humidity inside greenhouse is: ");
    Serial.print(finalAverageInHumidity);
    Serial.println("%");

    for (int i = 0; i < 5; i++)
    {
      exTemp = 1.1042 * dht2.readTemperature() - 1.7263;
      delay(2000);
      aExterTemp += exTemp;
    }

    float finalAverageExTemp = aExterTemp / 5;
    Serial.print("The current average temperature outside greenhouse is: ");
    Serial.print(finalAverageExTemp);
    Serial.println("°C");

    for (int i = 0; i < 5; i++)
    {
      exHumidity = 0.9698 * dht2.readHumidity() - 3.1216;
      delay(2000);
      aExterHumidity += exHumidity;
    }

    float finalAverageExHumidity = aExterHumidity / 5;
    Serial.print("The current average humidity outside greenhouse is: ");
    Serial.print(finalAverageExHumidity);
    Serial.println("%");

    Serial.println("-----------AVERAGE TEMP AND HUMIDITY READINGS OBTAINED---------------");
    delay(2000);

    Serial.println("-----------CHECKING WHETHER TEMPERATURE AND HUMIDITY IS IN IDEAL RANGE---------------");
    // Check whether the temperature in greenhouse is in ideal range or outside ideal range
    if (finalAverageInTemp > u_temp_threshold)
    {
      TempThresholdState = "HIGH";
    }

    else if (finalAverageInTemp < l_temp_threshold)
    {
      TempThresholdState = "LOW";
    }

    else
    {
      TempThresholdState = "MEDIUM";
    }
    Serial.print("State of the internal temperature is: ");
    Serial.println(TempThresholdState);

    // Check whether the temp in greenhouse is higher or lower than temp outside greenhouse
    if (finalAverageInTemp < finalAverageExTemp)
    {
      ExTempCompareState = "HIGH";
    }

    else
    {
      ExTempCompareState = "LOW";
    }
    Serial.print("State of the external temperature is: ");
    Serial.println(ExTempCompareState);

    // Check whether the humidity in greenhouse is in ideal range or outside ideal range
    if (finalAverageInHumidity > u_humidity_threshold)
    {
      HumidThresholdState = "HIGH";
    }

    else if (finalAverageInHumidity < l_humidity_threshold)
    {
      HumidThresholdState = "LOW";
    }

    else
    {
      HumidThresholdState = "MEDIUM";
    }
    Serial.print("State of the internal humidity is: ");
    Serial.println(HumidThresholdState);

    // Check whether the humidity in greenhouse is higher or lower than humidity outside greenhouse
    if (finalAverageInHumidity < finalAverageExHumidity)
    {
      ExHumidityCompareState = "HIGH";
    }

    else
    {
      ExHumidityCompareState = "LOW";
    }
    Serial.print("State of the external humidity is: ");
    Serial.println(ExHumidityCompareState);
    Serial.println("-----------STATES FOR EACH PARAMETER DECIDED---------------");
    delay(2000);

    Serial.println("-----------EXECUTION OF TEMPERATURE AND HUMIDITY TASKS START HERE---------------");
    // reduce fan speed to allow basic ventilation only and turn on humidifier provide colder/hotter air until temp reaches ideal temp, if more than 20minutes, force shut down send email
    if ((TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "LOW") ||
        (TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "LOW") ||
        (TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "HIGH" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "LOW") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "LOW") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
        (TempThresholdState == "LOW" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW"))
    {
      // FULL SPEED AND TURN ON HUMIDIFIER
      int systemStartTime = millis();
      analogWrite(inputFan_1, 0);
      analogWrite(exhaustFan_1, 0);
      sendLatestIntakeFanSpeed(0, 0);
      bool exitOuterLoop = false;

      while (exitOuterLoop == false && (finalAverageInTemp < l_temp_threshold || finalAverageInTemp > u_temp_threshold))
      {
        int systemRunningDuration = millis();
        // turning on the humidifier to help cool down temperature for 20 minutes
        if (systemRunningDuration - systemStartTime < (20 * 60 * 1000))
        {
          // Turn on humidifier
          digitalWrite(RELAY_PIN_1, HIGH);
          delay(1000);
          digitalWrite(RELAY_PIN_1, LOW);
          delay(30000);
          digitalWrite(RELAY_PIN_1, HIGH);
          delay(1000);
          digitalWrite(RELAY_PIN_1, LOW);
          delay(5000);
          aInterTemp = 0;
          // Calculate average temperature
          for (int a = 0; a < 5; a++)
          {
            inTemp = dht.readTemperature();
            aInterTemp += inTemp;
            delay(2000);
          }
          finalAverageInTemp = aInterTemp / 5;
          Serial.print("The current average temperature inside greenhouse is: ");
          Serial.print(finalAverageInTemp);
          Serial.print("°C. ");

          if (finalAverageInTemp >= l_temp_threshold && finalAverageInTemp <= u_temp_threshold)
          {
            Serial.println("The temperature in greenhouse is in ideal range now");
            exitOuterLoop = true;
          }

          else
          {
            Serial.println("Temperature still not in ideal, continue while loop");
          }
        }
        // if the temp is still non-ideal after running for more than 20minutes
        else
        {
          postTempCantAdjustEmail();
          Serial.println("Non-ideal temperature not adjusting alert email sent succesfully");
          exitOuterLoop = true;
        }
      }
      Serial.println("Updating latest system up time");
      lastTriggerTime = currentTime;
    }

    // increase fan to full speed to remove/intake hot air until it reaches ideal temp, if fail to reach ideal after 10min, trigger humidifier, after 15minutes still fail, force shut dwn send email
    else if ((TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "LOW") ||
             (TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "LOW") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "HIGH" && ExHumidityCompareState == "LOW") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "MEDIUM" && ExHumidityCompareState == "LOW"))
    {
      // FULL SPEED AND TURN ON HUMIDIFIER IF FAIL TO ACHIEVE IDEAL TEMP
      int systemStartTime = millis();
      analogWrite(inputFan_1, 4095);
      analogWrite(exhaustFan_1, 4095);
      sendLatestIntakeFanSpeed(1700, 1700);
      Serial.println("fan is set to full speed");
      bool exitOuterLoop = false;

      while (exitOuterLoop == false && (finalAverageInTemp < l_temp_threshold || finalAverageInTemp > u_temp_threshold))
      {
        int systemRunningDuration = millis();
        // let the fans run at full speed for 3 minutes
        delay(180000);
        // obtaining the current temperature inside greenhouse to determine whether it reaches ideal
        aInterTemp = 0;
        for (int a = 0; a < 5; a++)
        {
          inTemp = dht.readTemperature();
          aInterTemp += inTemp;
          delay(2000);
        }
        finalAverageInTemp = aInterTemp / 5;
        Serial.print("The current average temperature inside greenhouse is: ");
        Serial.print(finalAverageInTemp);
        Serial.println(" °C");

        if (finalAverageInTemp >= l_temp_threshold && finalAverageInTemp <= u_temp_threshold)
        {
          Serial.println("Temp back to normal range through fans only, no humidifier needed");
          analogWrite(inputFan_1, 0);
          analogWrite(exhaustFan_1, 0);
          sendLatestIntakeFanSpeed(0, 0);
          break; // Exit the while loop
        }

        else
        {
          if (systemRunningDuration - systemStartTime >= 1 * 60 * 1000)
          {
            Serial.println("More than 10minutes, humidifier will be turned on to help reduce internal temperature");
            digitalWrite(RELAY_PIN_1, HIGH);
            delay(1000);
            digitalWrite(RELAY_PIN_1, LOW);
            // wait for 5 minutes
            delay(300000);
            // Remove former value stored in the variable
            aInterTemp = 0;
            for (int a = 0; a < 5; a++)
            {
              inTemp = dht.readTemperature();
              aInterTemp += inTemp;
              delay(2000);
            }
            finalAverageInTemp = aInterTemp / 5;
            Serial.print("The current average temperature inside greenhouse is: ");
            Serial.print(finalAverageInTemp);
            Serial.println(" °C");
            if (finalAverageInTemp < l_temp_threshold || finalAverageInTemp > u_temp_threshold)
            {
              postHumidierCantHelp();
              digitalWrite(RELAY_PIN_1, HIGH);
              delay(1000);
              digitalWrite(RELAY_PIN_1, LOW);
              exitOuterLoop = true;
            }

            else
            {
              Serial.println("the humidifier successfully helps to adjust temperature to ideal range");
              digitalWrite(RELAY_PIN_1, HIGH);
              delay(1000);
              digitalWrite(RELAY_PIN_1, LOW);
              exitOuterLoop = true;
            }
          }

          else
          {
            Serial.println("Not 10 minutes yet");
          }
        }
      }
      Serial.println("While loop is completed");
      Serial.println("Setting fan speed back to ventilation mode");
      analogWrite(inputFan_1, 0);
      analogWrite(exhaustFan_1, 0);
      sendLatestIntakeFanSpeed(0, 0);
      Serial.println("Updating latest system up time");
      lastTriggerTime = currentTime;
    }

    else if ((TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "HIGH" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "LOW" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW"))
    {
      int systemStartTime = millis();
      analogWrite(inputFan_1, 4095);
      analogWrite(exhaustFan_1, 4095);
      sendLatestIntakeFanSpeed(1700, 1700);
      bool exitOuterLoop = false;

      while (exitOuterLoop == false && (finalAverageInTemp < l_temp_threshold || finalAverageInTemp > u_temp_threshold))
      {
        int systemRunningDuration = millis();
        Serial.println("Turning on humidifier");
        digitalWrite(RELAY_PIN_1, HIGH);
        delay(1000);
        digitalWrite(RELAY_PIN_1, LOW);
        delay(20000);
        Serial.println("Turning off humidifier");
        digitalWrite(RELAY_PIN_1, HIGH);
        delay(1000);
        digitalWrite(RELAY_PIN_1, LOW);
        aInterTemp = 0;
        for (int a = 0; a < 5; a++)
        {
          inTemp = dht.readTemperature();
          aInterTemp += inTemp;
          delay(2000);
        }
        finalAverageInTemp = aInterTemp / 5;
        Serial.print("The current average temperature inside greenhouse is: ");
        Serial.print(finalAverageInTemp);
        Serial.println(" °C");

        if (finalAverageInTemp >= l_temp_threshold && finalAverageInTemp <= u_temp_threshold)
        {
          aInterHumidity = 0;
          for (int i = 0; i < 5; i++)
          {
            inHumidity = dht.readHumidity();
            aInterHumidity += inHumidity;
            delay(3000);
          }
          int finalAverageInHumidity = aInterHumidity / 5;
          Serial.print("The current average humidity inside greenhouse is: ");
          Serial.print(finalAverageInHumidity);
          Serial.println(" %");

          if ((finalAverageInHumidity >= l_humidity_threshold && finalAverageInHumidity <= u_humidity_threshold))
          {
            Serial.println("Task completed");
            exitOuterLoop = true;
          }

          else if (l_humidity_threshold >= finalAverageInHumidity)
          {
            digitalWrite(RELAY_PIN_1, HIGH);
            delay(1000);
            digitalWrite(RELAY_PIN_1, LOW);
            // Turn on humidifier for another 1 minute
            delay(60000);
            digitalWrite(RELAY_PIN_1, HIGH);
            delay(1000);
            digitalWrite(RELAY_PIN_1, LOW);
            Serial.println("The task are done, humidifier turned off");
            exitOuterLoop = true;
          }

          else
          {
            Serial.println("The humidity inside greenhouse is already more than enough, excess humidity will be removed through ventilation.");
            exitOuterLoop = true;
          }
        }

        else
        {
          // Temp still not in ideal range, check whether the fan has been running for 10minutes
          if (systemRunningDuration - systemStartTime >= 10 * 60 * 1000)
          {
            Serial.println("The fan has been running for more than 10 minutes and the temperature can't drop to ideal range");
            postTempCantAdjustEmail();
            Serial.println("Alert email sent");
            exitOuterLoop = true;
          }

          else
          {
            Serial.println("the temperature is still not ideal");
          }
        }
      }
      lastTriggerTime = currentTime;
    }

    // reduce fan speed to allow basic ventilation only and turn on humidifier until humidity reach ideal lv, if more than 20 minutes cant reach, force stop and send alert email
    else if ((TempThresholdState == "MEDIUM" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "MEDIUM" && ExTempCompareState == "HIGH" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW") ||
             (TempThresholdState == "MEDIUM" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "HIGH") ||
             (TempThresholdState == "MEDIUM" && ExTempCompareState == "LOW" && HumidThresholdState == "LOW" && ExHumidityCompareState == "LOW"))
    {
      Serial.println("Task Execution Started");
      int systemStartTime = millis();
      analogWrite(inputFan_1, 0);
      analogWrite(exhaustFan_1, 0);
      sendLatestIntakeFanSpeed(0, 0);
      bool exitOuterLoop = false;

      while (exitOuterLoop == false && (finalAverageInHumidity < l_humidity_threshold || finalAverageInHumidity > u_humidity_threshold))
      {
        int systemRunningDuration = millis();
        if (systemRunningDuration - systemStartTime < (1 * 60 * 1000))
        {
          // Turn on humidifier
          digitalWrite(RELAY_PIN_1, HIGH);
          delay(1000);
          digitalWrite(RELAY_PIN_1, LOW);
          delay(20000);
          digitalWrite(RELAY_PIN_1, HIGH);
          delay(1000);
          digitalWrite(RELAY_PIN_1, LOW);
          delay(5000);
          // Calculate average temperature
          aInterTemp = 0;
          for (int a = 0; a < 5; a++)
          {
            inTemp = dht.readTemperature();
            aInterTemp += inTemp;
            delay(2000);
          }
          finalAverageInTemp = aInterTemp / 5;
          Serial.print("The current average temperature inside greenhouse is: ");
          Serial.print(finalAverageInTemp);
          Serial.println(" °C");

          if (finalAverageInTemp >= l_temp_threshold && finalAverageInTemp <= u_temp_threshold)
          {
            Serial.println("Humidifier reached ideal range, task execution completed");
            exitOuterLoop = true;
          }

          else
          {
            Serial.println("Humidifier still in non-ideal range");
          }
        }

        else
        {
          // To quit while loop
          postHumidityCantAdjustEmail();
          Serial.println("Humidifier cannot be adjusted to ideal range, alert email sent succesfully");
          break;
        }
      }
      Serial.println("Latest system up time updated");
      lastTriggerTime = currentTime;
    }

    else
    {
      Serial.println("no action needed.");
    }
    Serial.println("---------------");
    aInterTemp = 0;
    aInterHumidity = 0;
    aExterTemp = 0;
    aExterHumidity = 0;
    inTemp = 0;
    exTemp = 0;
    inHumidity = 0;
    exHumidity = 0;
    analogWrite(inputFan_1, 0);
    analogWrite(exhaustFan_1, 0);
    sendLatestIntakeFanSpeed(0, 0);
    Serial.println("All values are reset");
    Serial.println("-----------EXECUTION OF TEMPERATURE AND HUMIDITY TASKS ENDS HERE---------------");
    delay(3000);
  }

  else
  {
    Serial.println("Still on cooldown");
    sendLatestIntakeFanSpeed(0, 0);
  }
}
