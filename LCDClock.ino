/*
 Name:		LCDClock.ino
 Created:	6/30/2020 11:52:03 AM
 Author:	vdevan
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <WiFiUdp.h>

#include <Wire.h>
#include "RTClib.h"

//Constants and definitions
//WiFi Client Connection change if too low
#define WHILE_LOOP_DELAY        200L        //Connection Wait time
#define WIFI_CONNECTION_MONITOR 1200000L     //20 minutes to keep checking for WIFI availability
#define STORAGE 6                           //stored network
#define WIFI_TIMEOUT    8                   //Time out in secs for Wifi connection. Will affect clock performance if long
#define SERVER_ON_TIME  10                  //Server On Time in mins after reset before reverting to clock. 0 is indefinite 
#define CLOCK_ID    1                       //default clock id.
#define UPDATEINTERVAL  1800000L             //in milli seconds. This will update with NTP Server every 30 minutes
#define DIGITAL_UPDATE  60000               //Minute update for Digital clock
#define LCD_UPDATE      1000                //Seconds update for LCD Display
#define NTP_PACKET_SIZE 48                  //Default NTP Packet size
#define NTP_DEFAULT_LOCAL_PORT 1337         //NTP Default local port


//RTC Pins
#define RTC_SDA 4 //D2
#define RTC_SCL 5 //D1
RTC_DS3231 rtc;

//Wifi Clock requirements
#define REG_SEL 0  //D3
#define ENABLE 2  //D4
#define DATA4  14  //D5
#define DATA5  12  //D6
#define DATA6  13 //D7
#define DATA7  15 //D8 
#define RESET 3 //RX D9 -Config button. 

//For NTP Requirements
WiFiUDP ntpUDP;
const char* poolServerName = "pool.ntp.org"; // Default time server
int    port = NTP_DEFAULT_LOCAL_PORT;
const unsigned long seventyYears = 2208988800UL; //1900 - 1970 seconds
byte packetBuffer[NTP_PACKET_SIZE];

//Calendar details
char WEEK[7][5] = { "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat " };
char MONTHS[12][6] = { " JAN ", " FEB ", " MAR ", " APR ", " MAY ", " JUN ", " JUL ", " AUG ", " SEP ", " OCT ", " NOV ", " DEC " };
static const int MonthTable[]{ 0,3,2,5,0,3,5,1,4,6,2,4 };
static const int LastWeek[]{1,-2,1,0,1,0,1,1,0,1,0,1};

const char CLOCKPASSWORD[13] = "password";  //Host password
const char HOSTNAME[13] = "wificlock001";   //use http://wificlockXXX.local Replace XXX with clockID


//WiFi, UDP, Server & DNS. Change constants as per requirement
String clockSSID = "ESP_" + String(ESP.getChipId(), HEX);
ESP8266WebServer server(80);// Web server
IPAddress clockIP; //(172, 23, 1, 1);  //172, 23, clockID, 1
IPAddress netMsk(255, 255, 0, 0); //by using 0 in 3rd octet, 254 clocks are possible in the same network

unsigned long serverTimeout;

//Multi connect storage
struct PREF
{
    char    ClockPassword[13];
    char    Hostname[13];
    int     DSTZone;
    uint8   ClockID;
    uint8   Timeout;
    uint8   ServerOntime;
    uint8   DSTStartMonth;
    uint8   DSTEndMonth;
    uint8   DSTStartDay;
    uint8   DSTEndDay;
    uint8   DSTStartWeek;
    uint8   DSTEndWeek;
    uint8   DSTStartTime;
    uint8   DSTEndTime;
    bool    bAMPM;
    bool    bDST;
} pref;

struct NETWORK
{
    char SSID[33];
    char Password[33];
};

NETWORK Networks[STORAGE];

struct DST
{
  int startMonth;
  int endMonth;
  int startDate;
  int endDate;
  int startTime; 
  int endTime;
} currentDST;

int storageIndex;
int storedNetworks;

//time requirements
unsigned long tick;
uint8_t secs = 0;
uint8_t mins = 0;
uint8_t currentHour = 25;
uint8_t currentMonth = 0;
uint8_t currentDate = 0;
uint8_t currentDay = 0;
uint8_t runningYear=0;
uint16_t currentYear=0;

//LCD  & HW variables
LiquidCrystal lcd(REG_SEL,ENABLE,DATA4,DATA5,DATA6,DATA7);

// LAN Not available
byte LAN[] = 
{
  B00001,
  B00001,
  B00011,
  B00011,
  B00111,
  B00111,
  B11111,
  B11111
};

// LAN available
byte NOLAN[] = 
{
  B00011,
  B00011,
  B00101,
  B00101,
  B01001,
  B01001,
  B10001,
  B11111
};



//Connection status of Server / client and WiFi retry 
bool bConnect;
bool bServer;
unsigned long lastConnectTry = 0;
unsigned long updateInterval = 0;
unsigned long elapsedTime = 0;    //should be used for secs instead of delay(1);

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.println();
    Serial.println("Configuring access point...");
    pinMode(RESET, INPUT_PULLUP);

    lcd.createChar(0, LAN);
    lcd.createChar(1, NOLAN);
    loadCredentials(); // Load WLAN credentials from EPROM

    clockIP = IPAddress(172, 23, pref.ClockID, 1);
   

    if (!rtc.begin())
        Serial.println("Couldn't find RTC");
    
    //RTC Debug & testing
    /* Comment out after testing* /    
    rtc.adjust(DateTime(1593639258)); //Store the value in rtc for DST testing
 
    /* End of testing */
    
    
    WiFi.softAPConfig(clockIP, clockIP, netMsk);
    WiFi.softAP(clockSSID.c_str(), pref.ClockPassword);
    delay(500);
    Serial.print("Clock IP address: ");
    Serial.println(WiFi.softAPIP());

    lcd.begin(16,2);
    lcd.clear();

    ntpUDP.begin(NTP_DEFAULT_LOCAL_PORT);

    server.on("/", handleRoot);
    server.on("/wifisave", handleWifiSave);
    server.on("/pref", handlePreference);
    server.on("/prefsave", handlePrefSave);
    server.on("/time",handleTime);
    server.onNotFound(handleNotFound);

    bServer = false;
    Serial.printf("Stored Networks found: %d\n", storedNetworks);
    Serial.printf("Time now is: %d:%d:%d\n", (rtc.now()).hour(), (rtc.now().minute()), (rtc.now().second()));

    //Need to go through the entire Networks for stored connections
    if (storedNetworks > 0); // Request WLAN connect if there is a SSID
        connectWifi();

    //Server will be started at the power-on or config button used
    if (!bConnect)
        startServer();


    //bReset = false;    
    elapsedTime=millis();
    updateInterval = millis();

}



//Monitor the config pin and call Config function if required
void MonitorConfigPin()
{
    if (digitalRead(RESET) == HIGH)
        return;
    int elapse = 0;
    //Serial.println("Detected Config Request");
    while (digitalRead(RESET) == LOW)
    {
      elapse++;
      if (elapse > 2)
      {
        lcd.setCursor(0, 1); //Calendar line
        lcd.print(" Settings Req. ");
      }
      if (elapse > 10)
      {
        lcd.setCursor(0, 1); //Calendar line
        lcd.print(" Factory Reset ");
      }
      delay(1000);
    }

    if (elapse > 10)
    {
        WiFi.disconnect();
        EraseStoredValue();
    }
    
    if (elapse >2)
    {
        bConnect = false;
        startServer();
    }

}

void loop()
{
    //Displaytime
    if (millis() - elapsedTime > LCD_UPDATE)//change to LCD_UPDATE after testing
    {
        elapsedTime = millis();
        displayTime();
    }
  
    //try reconnecting to wiFi after 10 minutes
    if ((storedNetworks > 0 && !bServer && (millis() - lastConnectTry) > WIFI_CONNECTION_MONITOR)||(WiFi.status() != WL_CONNECTED && bConnect)) //600000- 10 mins
    {
        bConnect = false;
        //Serial.println("Trying to reconnect Connect to LAN...");
        connectWifi();
        lastConnectTry = millis();
    }

    //If we have a wifi connection then stop the web server
    if (bConnect && bServer)
    {
        server.stop();
        MDNS.close();
        displayDate();
        bServer = false;
    }

    //Server already running? 
    if (bServer)
    {
        //Handle Server time-out here in case we came here because of a reset button
        if (pref.ServerOntime > 0 && (millis() - serverTimeout) > pref.ServerOntime * 60000)
        {
            bServer = false;
            displayDate();
            MDNS.close();
            server.stop();
        }
        else
        {
            server.handleClient();
            MDNS.update();
        }
    }
    MonitorConfigPin();
    if (millis() - updateInterval > UPDATEINTERVAL)
      NTPUpdate();
 
}
