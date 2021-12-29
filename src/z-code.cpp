#include <Wire.h>
#include <PxMatrix.h>
#include "Arduino.h"
#include "Time.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include "EEPROM.h"
#include <LEDMatrixDriver.hpp>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "Parola_Fonts_data.h"

//Headers
#include "mainpage.h"

#define USE_UI_CONTROL 0

#if USE_UI_CONTROL
#include <MD_UISwitch.h>
#endif

#define DEBUG 0

#if DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   18
#define DATA_PIN  23
#define CS_PIN    5

#define PUSHBUTTON 34
#define BUZZER 13

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
const uint8_t SPEED_IN = A5;
const uint8_t SPEED_DEADBAND = 5;

uint8_t scrollSpeed = 50;    // default frame delay value
textEffect_t scrollEffect = PA_SCROLL_LEFT;
textPosition_t scrollAlign = PA_LEFT;
uint16_t scrollPause = 0; // in milliseconds
const MD_MAX72XX::fontType_t *pFont = myNumbers;


const char* ssid = "Shantimaster";
const char* password = "kirikiri";

WebServer server(80);
WiFiMulti wifiMulti;
struct tm timeinfo;

#define  BUF_SIZE  95
char newMessage[BUF_SIZE] = { "" };
String message = "";
String alarmDays = "0000000";
String alarmTime = "00:00";
int brightness = 5;


void beepOk();
void beepAlarm();
void readEeprom();
String parsePage();
void dataHandler();
String ipToString();
boolean isAlarmTime();
void setClock();
void waitForIt();
String daysToString(String days[7]);
void showIp();
String getDate(boolean justTime);

void setup()
{
  Serial.begin(115200);

  // button and buzzer
  pinMode(PUSHBUTTON, INPUT);
  ledcSetup(0, 1000, 8);
  ledcAttachPin(BUZZER, 0);
  ledcWrite(0, 1000); //50

  // signal user when starting up
  beepOk();
  
  //Eeprom
  if (!EEPROM.begin(20)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
  // data from eeprom
  readEeprom();
  Serial.println("Eeprom ready"); 
  
  // display  
  P.begin();
  P.setIntensity(map(brightness, 1, 9, 0, 15));
  P.setFont(pFont);
  
  //wifi
  WiFi.mode(WIFI_MODE_STA);
  wifiMulti.addAP(ssid, password);
  while (wifiMulti.run()!= WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
   
  //Home page. Contents of 'page' is in mainpage.h
  server.on("/", []() {
   server.send(200, "text/html", parsePage());
  });
   //POST data handler
  server.on("/setAlarm", HTTP_POST, dataHandler);
  server.begin();  
  Serial.println("HTTP server started");
  
  Serial.println("Reporting wifi values");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: \n");
  Serial.println(ipToString()); 
  
  showIp();
  setClock(); // reset time  
}



int clock_live = 900;
boolean alarmOn = false;
int minute = -1;
String lmsg = "";
void loop()
{

  if(digitalRead(PUSHBUTTON) == HIGH) {
    showIp();
  }
  
  String msg = ""; 
  msg = getDate(true);
  if (alarmDays[timeinfo.tm_wday] == '1') {    
    msg += " #";
  }
  msg.toCharArray(newMessage, msg.length() + 1);
  
  if (minute != timeinfo.tm_min) {
    alarmOn = isAlarmTime();
    minute = timeinfo.tm_min;
    lmsg.toCharArray(newMessage, lmsg.length() + 1);  
    P.displayText(newMessage, PA_CENTER, 150, clock_live , PA_PRINT, PA_SCROLL_DOWN);    
    waitForIt();
    msg.toCharArray(newMessage, msg.length() + 1);  
    P.displayText(newMessage, PA_CENTER, 150, clock_live , PA_SCROLL_UP, PA_NO_EFFECT);
    waitForIt();
  } else {
    P.displayText(newMessage, PA_CENTER, 150, clock_live, PA_PRINT, PA_NO_EFFECT);
    waitForIt();
    lmsg = msg;
  }  
  server.handleClient();

    while (alarmOn) {
      if(digitalRead(PUSHBUTTON) == HIGH) {
        alarmOn = false;
        beepOk();
      }      
      msg = getDate(true);
      msg.toCharArray(newMessage, msg.length() + 1);  
      P.displayText(newMessage, PA_CENTER, 40, clock_live / 2 , PA_SCROLL_UP, PA_FADE);
      waitForIt();
      beepAlarm();
      server.handleClient();
      lmsg = msg;
    }

  
}

void beepOk(){
  ledcAttachPin(BUZZER, 0);
  ledcWriteTone(0, 880);
  delay(200);
  ledcWriteTone(0, 1661);
  delay(200);
  ledcDetachPin(BUZZER);
}

void beepAlarm(){
  ledcAttachPin(BUZZER, 0);
  ledcWriteTone(0, 880);
  delay(400);
  ledcWriteTone(0, 1661);
  delay(400);
  ledcWriteTone(0, 2489);
  delay(400);
  ledcWriteTone(0, 3135);
  delay(1000);
  
  ledcDetachPin(BUZZER);
  
}

void waitForIt()
{
  while (!P.displayAnimate())
  {
    // waste time
  }
}

boolean isAlarmTime(){
  int h = alarmTime.substring(0,2).toInt();
  int m = alarmTime.substring(3,5).toInt();
  return m == timeinfo.tm_min && h == timeinfo.tm_hour && alarmDays[timeinfo.tm_wday] == '1';
}

String parsePage() {
  String ret = page;
  String pDays = alarmDays;
  ret.replace("%ALARM%", alarmTime);
  ret.replace("%D1%",alarmDays[0] == '1' ? "checked" : "");
  ret.replace("%D2%",alarmDays[1] == '1' ? "checked" : "");
  ret.replace("%D3%",alarmDays[2] == '1' ? "checked" : "");  
  ret.replace("%D4%",alarmDays[3] == '1' ? "checked" : "");
  ret.replace("%D5%",alarmDays[4] == '1' ? "checked" : "");
  ret.replace("%D6%",alarmDays[5] == '1' ? "checked" : "");
  ret.replace("%D7%",alarmDays[6] == '1' ? "checked" : "");
  ret.replace("%BRIGHTNESS%", String(brightness));
  String s = "";
  if (alarmOn) {
    s = " <div><input type='hidden' name='stop_alarm' value = '1' /><button class='button' type='submit' id='stopAlarm'>Stop alarm</button></div>\r\n ";    
  }
  ret.replace("%ALARM_SWITCH%",s);  
  return ret;
}


void dataHandler(){  
  String alarmStop = server.arg("stop_alarm");
  if (alarmStop == "1"){
    alarmOn = false;
    beepOk();
    server.sendHeader("Location","/");  //redirect client to home page
    server.send(303);    
    return;
  }
  String msg = server.arg("alarmTime");   //POST data
  String days[7] = {
    server.arg("d1"),
    server.arg("d2"),
    server.arg("d3"),
    server.arg("d4"),
    server.arg("d5"),
    server.arg("d6"),
    server.arg("d7")
  };
  String bright = server.arg("brightness");

  EEPROM.writeString(0, msg);  
  EEPROM.writeString(5, daysToString(days));
  EEPROM.writeInt(12, bright.toInt());
  EEPROM.commit();
  beepOk();
  String m = "    OK    ";
  m.toCharArray(newMessage, m.length() + 1);
  P.displayClear();
  P.displayText(newMessage, PA_CENTER, 50, 1000, PA_PRINT, PA_FADE);
  waitForIt();
  readEeprom();
  P.setIntensity(map(brightness, 1, 9, 0, 15));  
  
  server.sendHeader("Location","/");  //redirect client to home page
  server.send(303);                   //redirect http code
}

String daysToString(String days[7]) {
  String ret = "0000000";
  for (int i = 6; i >= 0; i--) {
        ret[i] = days[i] == "on" ? '1' : '0';
  }
  return ret;
}

void setClock() {
  // should be 3600 for DST!
  configTime(0, 0, "europe.pool.ntp.org", "pool.ntp.org", "time.nist.gov");
  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }
   //Serial.println(F("\nDone! "));
  getLocalTime(&timeinfo);
}

String getDate(boolean justTime) { 
  getLocalTime(&timeinfo);
  //Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
  String timeValue = asctime(&timeinfo);
  char day[3];
  char weekDay[10];
  char month[10];
  char year[5];
  char hour[3];
  char minute[3];
  strftime(weekDay, 10, "%A", &timeinfo);
  strftime(month, 10, "%m", &timeinfo);
  strftime(day, 3, "%d", &timeinfo);
  strftime(hour, 3, "%H", &timeinfo);
  strftime(minute, 3, "%M", &timeinfo);
  strftime(year, 5, "%Y", &timeinfo);
  char test [30];  
  char time_val [5];  
  sprintf(test, "%s, %s-%s-%s",weekDay, day, month, year);
  String line = test;
  sprintf(time_val, "%s:%s", hour, minute);
  if (justTime) {    
    return time_val;
  }  
  return line;
}

void readEeprom() {
  alarmTime =  EEPROM.readString(0).substring(0,5); //address to 4
  alarmDays =  EEPROM.readString(5).substring(0,7); //address to 11
  brightness = EEPROM.readInt(12);

  Serial.print("Stored alarm: ");
  Serial.println(alarmTime);
  Serial.print("Stored alarm days: ");
  Serial.println(alarmDays);
  Serial.print("Stored brightness: ");
  Serial.println(brightness);  
  
}

void showIp() {
  alarmOn = false; 
  beepOk();
  P.displayClear();
  String m = ipToString();
  m.toCharArray(newMessage, m.length() + 1);  
  P.displayText(newMessage, PA_CENTER, 150, 2000 , PA_SCROLL_LEFT, PA_SCROLL_LEFT);    
  waitForIt();  
}



String ipToString(){
  IPAddress ip = WiFi.localIP();
  return String(ip[0]) + String(".") +\
  String(ip[1]) + String(".") +\
  String(ip[2]) + String(".") +\
  String(ip[3])  ; 
}
