#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h> 
#include <WiFiManager.h> 
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


#include <Time.h>
#include <EEPROM.h>
#include "my_tft_st7735.h"
#include <Adafruit_I2CDevice.h>

ADC_MODE(ADC_VCC);

const int time_between_reads = 1000; // 1 sec

#define MY_SERIAL
#define LOG_SHIFT_REGISTER
#define THINGSPEAK

#ifdef THINGSPEAK
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include "secrets.h"
#else
const char *ssid = "weather_station";
const char *password = "anothaone";
#endif


ESP8266WebServer server(80);
int wifi_setup = 0;
bool show_info = false;
bool listening=false;
bool cleanScreen=false;

// Define Connections to 74HC165
 
// PL pin 1
int load = D3; 
// CE pin 15
//int clockEnablePin = D3; // D3
// Q7 pin 7
int dataIn = D0;
// CP pin 2
int clockIn = D6;

MyTft mytft;
Battery battery;

// this is the valid one
IPAddress dns(192,168,2,1);


// for test only
//IPAddress dns(192,168,2,2);

void ICACHE_RAM_ATTR configWifi();

Envdata latest;
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;
float temperature, humidity, pressure, altitude;

void toggleShowInfo();
void connectWifi();
void listenServer();

void setup() {
  #ifdef MY_SERIAL
  Serial.begin(115200);
  Serial.setTimeout(2000);
  #endif
  
  mytft.begin();
  bme.begin(0x76);
  battery.begin(3.22);
#ifdef THINGSPEAK
  ;
#else
  listenServer();
#endif
  #ifdef MY_SERIAL
  Serial.println("All setup");
  #endif
  mytft.setBatteryObj(battery);
  pinMode(load, OUTPUT);
  pinMode(clockIn, OUTPUT);
  pinMode(dataIn, INPUT);
  connectWifi();
  mytft.clean();
}

void readShiftRegister() {
  /*
  static unsigned long last_read = millis();
  if (millis() - last_read < 200) {
    return;
  }
  last_read = millis();
  */
  digitalWrite(load, LOW);
  delayMicroseconds(5);
  digitalWrite(load, HIGH);
  delayMicroseconds(5);
 
  // Get data from 74HC165
  digitalWrite(clockIn, HIGH);
  //digitalWrite(clockEnablePin, LOW);
  byte incoming = shiftIn(dataIn, clockIn, LSBFIRST);
  #ifdef LOG_SHIFT_REGISTER
  if(incoming != B11111111) {
    if (incoming == B10111111) { // Wifi setting
      wifi_setup = 1;
    } else if (incoming == B11011111){ // DST toggle #TODO
    	;
    } else if(incoming == B11101111) { //show info
        show_info = 1;
    }
    Serial.print("Pin States:\r\n");
    Serial.println(incoming, BIN);
    delay(1000);
  }
  #endif

}

void handleSentVar() {
  if (server.hasArg("temp") &&
      server.hasArg("press") &&
      server.hasArg("batt") &&
      server.hasArg("alt") &&
      server.hasArg("hum")
      ) { // this is the variable sent from the client

        /*
         * struct Envdata {
            String temp;
            String hum;
            String press;
            String alt;
            String batt;
            String int_temp;
            String int_hum;
            String int_batt;
            }
            */
        latest.temp = server.arg("temp");
        latest.hum = server.arg("hum");
        latest.press = server.arg("press");
        latest.alt = server.arg("alt");
        latest.batt = server.arg("batt");
        latest.wifi = server.arg("wifi");
        latest.lastUpdate = mytft.getFullDateTime();
    #ifdef MY_SERIAL
    Serial.println("Received....");
    #endif
    server.send(200, "text/html", "Data received");
  }
}
void wifiSaved() {
  wifi_setup = 0;
  mytft.clean();
  mytft.writeText(0, 1, "WiFi Saved successfully" , ST77XX_WHITE);
}

void ICACHE_RAM_ATTR toggleShowInfo() {
  show_info = !show_info;
  cleanScreen = !show_info;
  #ifdef MY_SERIAL
  Serial.print("Toggle Show Info ");
  Serial.println(show_info);
  #endif
}

void setupWifi() {
  wifi_setup = 3;
  mytft.clean();
  mytft.writeText(2, 2, "Configuring WiFi Access" , ST77XX_WHITE);
  mytft.writeText(2, 15, "WiFi is required for some features. Reset if you wish to cancel\r\n" , ST77XX_YELLOW);

  //WiFi.disconnect(true);
  server.close();
  listening = false;
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(wifiSaved);
  mytft.writeText(2, 42, "Steps:\r\n - Use a device to connect to AP OnDemandAP \r\n - use the browser to navigate to http://192.168.4.1\r\n -Click Configure WiFi\r\n - Select your WiFi SSID in the list\r\n - Provide details and click save" , ST77XX_WHITE);

  if (!wifiManager.startConfigPortal("OnDemandAP")) {
    #ifdef MY_SERIAL
    Serial.println("Waiting to be configured");
    #endif
  }
}

void listenServer() {
  WiFi.mode(WIFI_AP_STA);
#ifdef THINGSPEAK
  ;
#else
  WiFi.softAP(ssid, password);
#endif
  #ifdef MY_SERIAL
  Serial.print("Waiting");
  Serial.println(".");
  #endif
  IPAddress myIP = WiFi.softAPIP();
  server.on("/data/", HTTP_GET, handleSentVar); // when the server receives a request with /data/ in the string then run the handleSentVar function
  server.begin();
  listening = true;
  #ifdef MY_SERIAL
  Serial.print("Listening on IP:");
  Serial.print(myIP);
  #endif
}

void connectWifi() {
  mytft.writeText(10, 0, "Waiting wifi",ST77XX_WHITE ,true);
  WiFi.begin();
  #ifdef MY_SERIAL
  Serial.println("");
  Serial.print("Waiting wifi");
  #endif
  while (WiFi.status() != WL_CONNECTED && !wifi_setup) {
    Serial.print(".");  
    readShiftRegister();
    delay(1000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    #ifdef MY_SERIAL
    Serial.println(WiFi.localIP()); 
    #endif
  }
  mytft.setWifi(WiFi);
}


void showInfo() {
  mytft.displaySystemInfo();
}
void readInternalData() {
  static unsigned long last_read = millis();
  if (millis() - last_read < time_between_reads) {
    return;  
  }
  Serial.println(millis() - last_read);
  last_read = millis();
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  latest.in_temp = temperature;
  latest.in_hum = humidity;
  

  #ifdef MY_SERIAL
  Serial.print("Int Temp:");
  Serial.println(temperature);
  Serial.print("Int Hum:");
  Serial.println(humidity);
  Serial.print("Int Press:");
  Serial.println(pressure);
  Serial.print("Int Altitude:");
  Serial.println(altitude);
  #endif
}

void lowBattery() {
  static unsigned long last_beep = millis( );
  /* Measure once every four seconds. */
  if (battery.lowBattery(latest.batt.toDouble())) {
    if( millis() - last_beep > 3000ul ){
      last_beep = millis( );
      //shortBeep();
    }
  }
}
#ifdef THINGSPEAK
String getUrl() {
	//https://api.thingspeak.com/channels/{{CHANNEL}}/feeds.json?api_key={{TS_APIKEY}}&results=10
	String url = "/channels/";
	url += TS_CHANNEL;
	url += "/feeds.json?api_key=";
	url += TS_READ_APIKEY;
	url += "&results=1";
#ifdef MY_SERIAL
	Serial.print("Url:");
	Serial.println(url);
#endif
	return url;
}
void readThingSpeak() {
	static unsigned long  last_read = millis() + 60000;
	static String last_entry_id = "";

	if (millis() - last_read > 60000) { //read every minute

		std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
		client->setInsecure();
		HTTPClient https;
		const char * host = "https://api.thingspeak.com";
		if (https.begin(*client, String(host) + getUrl())) {
		int httpCode = https.GET();

			// httpCode will be negative on error
			if (httpCode > 0) {
				last_read = millis();
			  // HTTP header has been send and Server response header has been handled
			  Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
			  // file found at server?
			  if (httpCode == HTTP_CODE_OK) {
				String payload = https.getString();
				DynamicJsonDocument doc(1024);
				deserializeJson(doc, payload);
				JsonObject obj = doc.as<JsonObject>();
				String new_entry_id = obj["feeds"][0]["entry_id"];
				if (new_entry_id != last_entry_id) {
					last_entry_id = new_entry_id;

					latest.temp = obj["feeds"][0]["field1"].as<String>();
					latest.hum = obj["feeds"][0]["field5"].as<String>();
					latest.press = obj["feeds"][0]["field2"].as<String>();
					latest.alt = obj["feeds"][0]["field4"].as<String>();
					latest.batt = obj["feeds"][0]["field3"].as<String>();
					latest.wifi = obj["feeds"][0]["field6"].as<String>();
					latest.lastUpdate = obj["feeds"][0]["created_at"].as<String>();
				}
				Serial.println(last_entry_id);
			  }
			} else {
			  Serial.printf("[HTTPS] GET... failed, error: %s\n\r", https.errorToString(httpCode).c_str());
			}

			https.end();
		}
	}
}
#endif
void loop() {

  readShiftRegister();
  lowBattery();
  static bool refresh = true;
  

  readInternalData();
  Serial.println("Passed");
  delay(1000);
  if (show_info) {
#ifdef THINGSPEAK
	readThingSpeak();
#else
    server.handleClient();
#endif
    showInfo();
    delay(5000);
    show_info = false;
    mytft.clean();
    refresh = true;
  } else if (cleanScreen) {
    cleanScreen = false;
    mytft.clean();
  }
  if( wifi_setup == 0) {
#ifdef THINGSPEAK
	readThingSpeak();
#else
	if (listening == false) {
      ESP.reset(); // reseting because could not bring server to live after setup wifi
    }
    server.handleClient();
#endif
  } else if (wifi_setup != 3){ // in pogress
    setupWifi();
  }

  mytft.showData(latest, refresh);
  if(refresh) {
    refresh = false; 
  }
  /*Serial.println(timeClient.getFormattedTime());
  Serial.print("LOcal IP:");
  Serial.println(WiFi.localIP());
  Serial.print("AP IP:");
  Serial.println(WiFi.softAPIP()); 
  Serial.print("Wifi status:");
  Serial.println(WiFi.status() == WL_NO_SSID_AVAIL);
  */
}
