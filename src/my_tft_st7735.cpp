#include "my_tft_st7735.h"
#include <Adafruit_GFX.h>    // Core graphics library

#define TFT_CS         15
#define TFT_RST        0                                            
#define TFT_DC         2
#define MY_SERIAL

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

WifiSignal wifisignal;
Envdata lastShowedData;

//DST
//const long utcOffsetInSeconds = -14400;
const long utcOffsetInSeconds = -18000;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
unsigned int batt_threshold = .3;
unsigned int temp_threshold = .1;
unsigned int hum_threshold = .1;
unsigned int press_threshold = 3;
unsigned long time_between_screen_updates = 1000;

char daysOfTheWeek[7][5] = {"S", "M", "T", "W", "T", "F", "S"};
//Month names
char months[12][20]={"Jan", "Feb", "Mar", "Apl", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool operator!=(const Envdata& a, const Envdata& b)
{
    return a.temp != b.temp ||
    a.hum != b.hum ||
    a.press != b.press ||
    a.alt != b.alt ||
    a.batt != b.batt ||
    a.wifi != b.wifi ||
    a.in_temp != b.in_temp ||
    a.in_hum != b.in_hum ||
    a.in_batt != b.in_batt ;
}

/*
 * 
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
  String lastUpdate
}
 */
void MyTft::showData(Envdata data, bool refresh){
  static unsigned long last_update = millis();
  if (millis() - last_update < time_between_screen_updates) {
    return;
  }
  last_update = millis();
  #ifdef MY_SERIAL
  Serial.print("Refresh...");
  Serial.println(refresh);
  #endif
  if (refresh || lastShowedData != data) {
    refreshing = refresh;
    lastShowedData = data;
    readNTPTime();
    outBattery();
    #ifdef MY_SERIAL
    Serial.println("Data is different");
    #else
    ;
    #endif
  } else {
    #ifdef MY_SERIAL
    Serial.println("Data is same as before");
    #else
    ;
    #endif
  }
  
  readNTPTime();
  showNTPTime();
  showNTPDate();
  showDayNightIcon();
  showWifiIcon();
  inBattery();
  showEnv();
  showNTPDay();
  refreshing = false;
  #ifdef MY_SERIAL
  Serial.print("Temp:");
  Serial.println(data.temp);
  Serial.print("Press:");
  Serial.println(data.press);
  Serial.print("Batt:");
  Serial.println(data.batt);
  Serial.print("Alt:");
  Serial.println(data.alt);
  Serial.print("Hum:");
  Serial.println(data.hum);
  Serial.print("wifi:");
  Serial.println(data.wifi);
  Serial.print("Last Update:");
  Serial.println(data.lastUpdate);
  #endif
}

String MyTft::getFullDateTime() {
  return getCurrentDate() + " " + getCurrentTime();
}

void MyTft::setWifi(ESP8266WiFiClass wifi){
  tftwifi = wifi;
  hasWifi = true;
}

void MyTft::displaySystemInfo() {
  clean();
  tft.setTextSize(1);
  writeText(3, 3, "System Details", ST77XX_YELLOW);
  writeText(3, 22, "Last upd:", ST77XX_RED);
  writeText(56, 22, const_cast<char*>(lastShowedData.lastUpdate.c_str()), ST77XX_WHITE);
  writeText(3, 41, "SSID:", ST77XX_RED);
  writeText(56, 41,  const_cast<char*>(tftwifi.SSID().c_str()), ST77XX_WHITE);
  writeText(3, 60, "IP:", ST77XX_RED);
  writeText(56, 60,  const_cast<char*>(tftwifi.localIP().toString().c_str()), ST77XX_WHITE);
  writeText(3, 79, "In Batt:", ST77XX_RED);
  writeText(56, 79,  const_cast<char*>(lastShowedData.in_batt.c_str()), ST77XX_WHITE);
  writeText(3, 98, "Out Batt:", ST77XX_RED);
  writeText(56, 98,  const_cast<char*>(lastShowedData.batt.c_str()), ST77XX_WHITE);
  writeText(3, 117, "Out WiFi:", ST77XX_RED);
  writeText(56, 117,  const_cast<char*>(lastShowedData.wifi.c_str()), ST77XX_WHITE);
}

void MyTft::icons() {
  //tft.drawRect(139, 3, 18, 10, ST77XX_BLUE);
  cleanRect(2, 2, 8, 8);
  tft.drawBitmap(10, 10, sun, 50, 50, ST77XX_RED);
}
void MyTft::outBattery() {
  int x=3,y=60,w=16,h=8;
  static double lastCurr = 0.00;
  double currVolt = lastShowedData.batt.toDouble();
  if( hitThreshold(lastCurr, currVolt, batt_threshold)) {
    //tft.drawRect(140, 4, 16, 8, ST77XX_BLUE);
    cleanRect(x, y, w, h);
    tft.drawBitmap(x, y, battery.getBatteryBmp(currVolt), w, h, ST77XX_WHITE);
  }
  
}
void MyTft::inBattery() {
  char buff[5];
  dtostrf(battery.getCurVolt(), 2, 2, buff);
  
  int x=3,y=4,w=16,h=8;
  static double lastCurr = 0.00;
  double currVolt = battery.getCurVolt();
  Serial.print("In battery:");
  Serial.println(buff);
  if( lastCurr != currVolt) {
    lastCurr = currVolt;
    //tft.drawRect(140, 4, 16, 8, ST77XX_BLUE);
    cleanRect(x, y, w, h);
    tft.drawBitmap(x, y, battery.getBatteryBmp(currVolt), w, h, ST77XX_ORANGE);
  }
}

void MyTft::drawWiFiStrength(ESP8266WiFiClass wifi) {
  wifisignal.begin(WiFi);
}

void MyTft::setBatteryObj(Battery _battery){
  battery = _battery;
}

void MyTft::begin(){
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  tft.setRotation(3);
  writeText(0, 0,"Display Ready...", ST77XX_WHITE, true);
  
  timeClient.begin();
}

void MyTft::cleanRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  tft.fillRect(x, y , w, h, ST77XX_BLACK);
}

void MyTft::readNTPTime() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    timeClient.update();
  }  
}

String MyTft::getCurrentTime() {
  readNTPTime();
  char buff[5];
  sprintf(buff, "%.2d:%.2d", timeClient.getHours(),timeClient.getMinutes());

  return buff;
}

String MyTft::getCurrentDate() {
  readNTPTime();
  unsigned long epochTime = timeClient.getEpochTime();
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int weekDay =timeClient.getDay();
  char buff[100];
  sprintf(buff, "%d %s %d", ptm->tm_mday, months[ptm->tm_mon], ptm->tm_year+1900);
  return buff;
}

void MyTft::showNTPDate() {
  const int x=94,y=120,w=100,h=13;
  static String lastDate = "";
  String currentDate = getCurrentDate();
  //String formattedDate= (String)daysOfTheWeek[timeClient.getDay()] + ", " + currentDate;
  if(refreshing || currentDate != lastDate ) {
    lastDate = currentDate;
    tft.setTextSize(1);
    //tft.drawRoundRect(x, y, w, h, 5, ST77XX_GREEN);
    cleanRect(x, y, w, h);
    writeText(x, y,  const_cast<char*>(currentDate.c_str()),ST77XX_YELLOW); 
    Serial.print("Date:");
    Serial.println(currentDate);
  }
}

void MyTft::showNTPDay() {
  const int x=98,y=93,w=60,h=16;
  int today = timeClient.getDay();
  for (int i=0; i< 7; i++) {
    int offset = x + (i*9);
    if (i == today) {
      writeText(offset, y,  daysOfTheWeek[i],ST77XX_WHITE);
    } else {
      writeText(offset, y,  daysOfTheWeek[i],ST77XX_RED);
    }
    
  }
}

void MyTft::showNTPTime() {
  const int x=98,y=104,w=60,h=16;
  static String lastTime = "";
  String currentTime = getCurrentTime();
  if(refreshing || lastTime != currentTime) {
    lastTime = currentTime;
    //tft.drawRoundRect(x, y-1, w, h, 5, ST77XX_RED);
    cleanRect(x, y-1, w, h);
    tft.setTextSize(2);
    writeText(x, y, const_cast<char*>(currentTime.c_str()),ST77XX_WHITE); 
    Serial.print("Time:");
    Serial.println(currentTime);
    //writeText(84,84, const_cast<char*>(timeClient.getFormattedTime().c_str()),ST77XX_WHITE); 
  }
}

void MyTft::showDayNightIcon() {
  static bool is_day = true;
  const int x=114,y=2,w=50,h=50;
  //tft.drawRoundRect(x, y, w, h, 5, ST77XX_RED);
  if ( timeClient.getHours() > 6 && timeClient.getHours() < 18) {
    if(refreshing || !is_day) {
      is_day = true;
      cleanRect(x, y, w, h);
      tft.drawBitmap(x, y, sun, w, h, ST77XX_RED);
    }
  } else {
    if (refreshing || is_day) {
      is_day = false;
      cleanRect(x, y, w, h);
      tft.drawBitmap(x, y, moon, w, h, ST77XX_RED);
    }
  }
}

void MyTft::showWifiIcon() {
  static bool hasConIcon = false;
  static bool hasIcon = false;
  const int x=130,y=60,w=24,h=24;
  if (refreshing || hasWifi && !hasConIcon) {
    hasIcon = true;
    hasConIcon = true;
    tft.drawBitmap(x, y, wifi, w, h, ST77XX_GREEN);
  } else {
    if(refreshing || !hasIcon) {
      hasIcon = true;
      tft.drawBitmap(x, y, nowifi, w, h, ST77XX_GREEN);
    }
  }
}

void MyTft::clean() {
  tft.fillScreen(ST77XX_BLACK);
}
void MyTft::writeText(uint16_t x, uint16_t y, char *text, int color, bool clear) {
  if (clear) {
    clean();
  }
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

bool MyTft::hitThreshold(double val_a,double val_b, double threshold ) {
  return abs(val_a - val_b) > threshold;
}

bool MyTft::hitThreshold(String val_a,String val_b, double threshold ) {
  return hitThreshold(val_a.toDouble(), val_b.toDouble(), threshold);
}
void MyTft::showEnv() {
  tft.setTextSize(1);
  int x=3,y=20,w=80,h=10;
  int gap = 0;
  static String lastInTemp="";
  static String lastInHum="";
  static String lastOutTemp="";
  static String lastOutHum="";
  static String lastOutPress="";

  if(refreshing || hitThreshold(lastInTemp, lastShowedData.in_temp, temp_threshold)) {
    cleanRect(x, y + gap, w, h);
    lastInTemp = lastShowedData.in_temp;
    //tft.drawRoundRect(x, y-3, w, h, 5, ST77XX_RED);    
    writeText(x, y + gap, "T:" ,ST77XX_ORANGE);
    writeText(x + 15, y + gap, const_cast<char*>(lastShowedData.in_temp.c_str()) ,ST77XX_ORANGE); 
  }
  gap = 13;
  if(refreshing || hitThreshold(lastInHum, lastShowedData.in_hum, hum_threshold)) {
    cleanRect(x, y + gap, w, h);
    lastInHum = lastShowedData.in_hum;
    //tft.drawRoundRect(x, y-3, w, h, 5, ST77XX_RED);    
    writeText(x, y + gap, "H:" ,ST77XX_ORANGE);
    writeText(x + 15, y + gap, const_cast<char*>(lastShowedData.in_hum.c_str()) ,ST77XX_ORANGE); 
  }
  gap = 54;
  if(refreshing || hitThreshold(lastOutTemp, lastShowedData.temp, temp_threshold)) {
    cleanRect(x, y + gap, w, h);
    lastOutTemp = lastShowedData.temp;
    //tft.drawRoundRect(x, y-3, w, h, 5, ST77XX_RED);    
    writeText(x, y + gap, "T:" ,ST77XX_WHITE);
    writeText(x + 15, y + gap, const_cast<char*>(lastShowedData.temp.c_str()) ,ST77XX_WHITE); 
  }
  gap += 13;
  if(refreshing || hitThreshold(lastOutHum, lastShowedData.hum, hum_threshold)) {
    cleanRect(x, y + gap, w, h);
    lastOutHum = lastShowedData.hum;
    //tft.drawRoundRect(x, y-3, w, h, 5, ST77XX_RED);    
    writeText(x, y + gap, "H:" ,ST77XX_WHITE);
    writeText(x + 15, y + gap, const_cast<char*>(lastShowedData.hum.c_str()) ,ST77XX_WHITE); 
  }

  gap += 13;
  if(refreshing || hitThreshold(lastOutPress, lastShowedData.press, press_threshold)) {
    cleanRect(x, y + gap, w, h);
    lastOutPress = lastShowedData.press;
    //tft.drawRoundRect(x, y-3, w, h, 5, ST77XX_RED);    
    writeText(x, y + gap, "P:" ,ST77XX_WHITE);
    writeText(x + 15, y + gap, const_cast<char*>(lastShowedData.press.c_str()) ,ST77XX_WHITE); 
  }

}

void MyTft::block() {
  uint16_t color = 100;
  int i;
  int t;
  for(t = 0 ; t <= 4; t+=1) {
    int x = 0;
    int y = 0;
    int w = tft.width()-2;
    int h = tft.height()-2;
    for(i = 0 ; i <= 16; i+=1) {
      tft.drawRoundRect(x, y, w, h, 5, color);
      x+=2;
      y+=3;
      w-=4;
      h-=6;
      color+=1100;
    }
    color+=100;
  }
}
