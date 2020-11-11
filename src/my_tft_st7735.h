#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include "wifi_signal.h"
#include "battery.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "resources.h"

struct Envdata {
  String temp;
  String hum;
  String press;
  String alt;
  String batt;
  String wifi;
  String in_temp;
  String in_hum;
  String in_batt;
  String lastUpdate;
};

class MyTft{
  public:
    void writeText(uint16_t x, uint16_t y, char *text, int color, bool clear=false);
    void begin();
    void clean();
    void drawWiFiStrength(ESP8266WiFiClass wifi);
    void showData(Envdata data, bool refresh=false);
    void displaySystemInfo();
    void setWifi(ESP8266WiFiClass wifi);
    String getFullDateTime();
    void setBatteryObj(Battery _battery);
    
  private:
    void showNTPTime();
    void showNTPDate();
    void showNTPDay();
    bool refreshing=false;
    bool hasWifi;
    ESP8266WiFiClass tftwifi;
    Battery battery;
    void block();
    void inBattery();
    void cleanRect(uint16_t x, uint16_t y, uint16_t sx, uint16_t sy);
    String getCurrentTime();
    void readNTPTime();
    void icons();
    void showDayNightIcon();
    void showWifiIcon();
    String getCurrentDate();
    void outBattery();
    void showTemp();
    void showEnv();
    bool hitThreshold(String val_a,String val_b, double threshold);
    bool hitThreshold(double val_a,double val_b, double threshold);
    
};