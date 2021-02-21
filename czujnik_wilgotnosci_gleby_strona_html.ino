#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Wire.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const long utcOffsetInSeconds = 3600;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

#define SensorPin A0
int sensorValue = 0;
int calibrationLow = 2;
int calibrationHigh = 1024;
long lastMeasureTime = 0;
long measureTimeDelay = 1000 * 10;

IPAddress local_IP(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

const char* ssid     = "CGA2121_VwXTpLd";
const char* password = "8aHEtkWAHx7SGZUDf4";

// Set LED GPIO
const int ledPin = 2;
String ledState;
AsyncWebServer server(80);

// Replaces placeholder with LED state value
String processor(const String& var) {
  Serial.println(var);
  if (var == "STATE") {
    if (digitalRead(ledPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    Serial.println(ledState);
    return ledState;
  }
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  delay(200);
  pinMode(ledPin, OUTPUT);
  pinMode(SensorPin, INPUT);

  digitalWrite(ledPin, HIGH);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  // Connect to Wi-Fi network with SSID and password
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/chartist.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/chartist.min.css", "text/css");
  });
  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.js", "text/javascript");
  });
  server.on("/chartist.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/chartist.min.js", "text/javascript");
  });

  server.on("/calibrate_min", HTTP_GET, [](AsyncWebServerRequest * request) {
    calibrationLow = measureHumidity();
    AsyncWebServerResponse *response = request->beginResponse(200);
    request->send(response);
  });
  server.on("/calibrate_max", HTTP_GET, [](AsyncWebServerRequest * request) {
    calibrationHigh = measureHumidity();
    AsyncWebServerResponse *response = request->beginResponse(200);
    request->send(response);
  });

  server.on("/led_on", HTTP_POST, [](AsyncWebServerRequest * request) {
    digitalWrite(ledPin, LOW);
    AsyncWebServerResponse *response = request->beginResponse(200);
    request->send(response);
  });

  server.on("/led_off", HTTP_POST, [](AsyncWebServerRequest * request) {
    digitalWrite(ledPin, HIGH);
    AsyncWebServerResponse *response = request->beginResponse(200);
    request->send(response);
  });

  server.on("/reset_data", HTTP_POST, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    File soilHumidityDataFile = SPIFFS.open("/data/humidity_data.csv", "w");
    if (!soilHumidityDataFile) {
      Serial.println("Error opening file 'humidity_data.csv'");
      request->send(500);
    }
    soilHumidityDataFile.close();
    lastMeasureTime = measureTimeDelay;
    request->send(response);
  });

  server.on("/measure_humidity", HTTP_POST, [](AsyncWebServerRequest * request) {
    int humidity = measureHumidity();

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["soilHumidity"] = humidity;
    root.printTo(*response);
    request->send(response);
  });

  server.on("/humidity_data", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    bool fileExists = SPIFFS.exists("/data/humidity_data.csv");
    if (!fileExists) {
      Serial.println("File '/data/humidity_data.csv' does not exist.Creating");
      SPIFFS.open("/data/humidity_data.csv", "w").close();
    }
    File soilHumidityDataFile = SPIFFS.open("/data/humidity_data.csv", "r");
    if (!soilHumidityDataFile) {
      Serial.println("Error opening file 'humidity_data.csv'");
      request->send(500);
    } else {
      String fileData;
      while (soilHumidityDataFile.available()) {
        fileData = soilHumidityDataFile.readString();
      }

      DynamicJsonBuffer jsonBuffer;
      JsonObject &root = jsonBuffer.createObject();
      root["data"] = fileData;
      root.printTo(*response);
      request->send(response);
    }

    soilHumidityDataFile.close();
  });
  //  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
  //    request->send_P(200, "text/plain", getTemperature().c_str());
  //  });


  // Start server
  server.begin();
}

void loop() {
  bool shouldMeasure = millis() > measureTimeDelay + lastMeasureTime;

  if (shouldMeasure) {
    String time = getCurrentTime();
    lastMeasureTime = millis();
    int humidity = measureHumidity();
    File humidityWriteFile = SPIFFS.open("/data/humidity_data.csv", "a+");
    if (!humidityWriteFile) {
      Serial.println("Error opening file 'humidity_data.csv'");
    }

    humidityWriteFile.print(time);
    humidityWriteFile.print(",");
    humidityWriteFile.print(humidity);
    humidityWriteFile.print("\n");
    humidityWriteFile.close();
  }

}

String getCurrentTime() {
  timeClient.update();
  return String(timeClient.getEpochTime() * 1000);
}
int measureHumidity() {
  int result;
  int measureLoops = 10;
  int resultSum = 0;
  for (int i = 0; i < measureLoops; i++) {
    int value = analogRead(SensorPin);
    int mappedValue = map(value, calibrationLow, calibrationHigh, 0, 100);
    Serial.print("mapped value: ");
    Serial.println(mappedValue);
    resultSum += mappedValue;
  }

  result = resultSum / measureLoops;
  Serial.print("returned value: ");
  Serial.println(result);

  return result;
}
