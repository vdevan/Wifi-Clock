/*************************************
 * Make sure ESP266 packages are installed in Arduino IDE. 
 * Select File/Preferences... and in additional manager add the 
 * URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * press OK. Also On the board manager load ESP866 Generic Module
 * see esp8266-v10.pdf for more details of ESP8266 
 * Before compiling, ensure all the files are in the same directory 
 * and the directory is named as NetClock
 *************************************/

#include <ESP8266WiFi.h>        //ESP8266 WiFi
#include <ESP8266WebServer.h>   //run Web server to select WiFi network
#include <ESP8266mDNS.h>        //Needed for Web server
#include <EEPROM.h>             //WiFi info are stored. Up to 6 networks can be stored
#include <LiquidCrystal.h>      //LCD display
#include <ESP8266HTTPClient.h>  //Used for accessing Network API
#include <Arduino_JSON.h>       //JSON library
#include <Wire.h>               //Needed for RTC communication
#include "RTClib.h"
//#include <ArduinoOTA.h>
//#include <eztime.h>
//WiFi Client Connection change if too low
#define WHILE_LOOP_DELAY        200L        //Connection Wait time
#define MAXNETWORK 6                           //stored network
#define WIFI_TIMEOUT    8                   //Time out in secs for Wifi connection. 
//#define TIME_CHECK 1800;                      //Adjust clock every 30 mins
//Header For EEPROM storage and retrieval
//Third cut = Lat/Lon as constant char
#define progId 0x1208
#define version 1020
//RTC Pins
#define RTC_SDA 4 //D2
#define RTC_SCL 5 //D1

//LCD requirements
#define REG_SEL 0  //D3
#define ENABLE 2  //D4
#define DATA4  14  //D5
#define DATA5  12  //D6
#define DATA6  13 //D7
#define DATA7  15 //D8 
#define RESET 3 //RX D9 -Config button. 
#define TSLENGTH 19 //Length of TimeStamp - 2024-01-07T19:00:05
#define DATELENGTH 10 //Length of Date - 2024-01-07
#define TIMELENGTH 8 //Length of Time - 19:00:05

const char WIFIPwd[9] = "password";  //Host password
const char HOSTNAME[11] = "netclock";   //use http://netclockXXX.local Replace XXX with clockID

int MDATE[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
char WEEK[7][5] = { "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat " };
char MONTHS[13][4] = { "Mth","JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
static const int MonthTable[]{ 0,3,2,5,0,3,5,1,4,6,2,4 };
char AMPM[2][5] = {" AM "," PM "};
String ampm;
int weekDay = 0; 
bool bTimeCheck = true; //Check Internet for Time every day at 2:05 AM to 2:55 AM every 5 mins to adjust DST 

//Global Variables Used by Calendar
struct TIME
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hour;
    uint8_t date;
    uint8_t ampm;
}dtTime;


//LAN NOLANbyte array is used for displaying the signal 
// strength / connection status at the top right corner

// WiFi connected and signal strength
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

// LAN not available
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

struct NETWORK
{
    char SSID[33];
    char Password[33];
};

NETWORK Networks[MAXNETWORK];

struct HEADER
{
  uint16_t id;
  uint16_t ver; 
  uint8_t dateFormat;
  uint8_t Ampm;
} header;



//LCD  & HW variables
LiquidCrystal lcd(REG_SEL,ENABLE,DATA4,DATA5,DATA6,DATA7);

uint8_t storedNetworks = 0;
int storageIndex;

//WiFi, UDP, Server & DNS. Change constants as per requirement
String LCDSSID = "ESP_" + String(ESP.getChipId(), HEX);
ESP8266WebServer server(80);// Web server
IPAddress LCDIP(172, 23, 1, 1);  //172.23.1.1 Defalt server IP. Safe on class B Network
IPAddress netMask(255, 255, 0, 0); //by using 0 in 3rd octet, 254 network connectivity possible

//RTC Libs requirement
RTC_DS3231 rtc;

unsigned long timeZone = 0;



//Connection status of Server / client and WiFi retry 
bool bConnect = false;
bool bServer = false;
bool bLoc = false;

unsigned long lastConnectTry = 0;


unsigned long lastTime = 0;
unsigned long timeDelay = 300000; //5 minutes delay to check in case we could not reach the network
int oneSecond = 1000;
int today;
int secTick = 0;
String WANPortIP = "";
String dts;

//LCD Display variables
String Time = ""; //Display Time on First Line
String Date = ""; //Display Date on Second line


//parameters for getting time and Weather
String GatewayUri = "http://ipinfo.io/json";
String TimeUri = "http://worldtimeapi.org/api/ip/";


String jsonArray;
JSONVar myObj;

 // Starting point. After this program falls to loop
void setup() 
{
 
  delay(1000);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");
  pinMode(RESET, INPUT_PULLUP);


  //RTC Debug & testing
  /* Comment out after testing* /    
  rtc.adjust(DateTime(1593639258)); //Store the value in rtc for DST testing
  /* End of testing */

  //create the images for WiFi status
  lcd.createChar(0, LAN);
  lcd.createChar(1, NOLAN);

  WiFi.softAPConfig(LCDIP, LCDIP, netMask);
  WiFi.softAP(LCDSSID.c_str(), WIFIPwd);
  delay(500);
  Serial.print("ESP Device IP address: ");
  Serial.println(WiFi.softAPIP());
  lcd.begin(16,2);
  lcd.clear();
  server.on("/", handleRoot);
  server.on("/wifisave", handleWifiSave);
  server.onNotFound(handleNotFound);
 
  if (loadCredentials()) // Load WLAN credentials from EPROM if not valid or no network stored then we need to start webserver
    connectWifi();

/*
  while (!rtc.begin())
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Getting RTC Time");
    lcd.setCursor(0,1);
    lcd.print("Please Wait...");
  }*/

  rtc.begin();

 

  if (bConnect)
  {
    if (AdjustDateTime())
    {
      Serial.printf("Date Time adjusted from Network. Current Date Time is: %s\n", rtc.now().timestamp().c_str());
    }
    else
    {
      Serial.printf("Stored RTC Value used. Current Date Time is: %s\n", rtc.now().timestamp().c_str());
    }
  }
  
  if (header.Ampm)
  {
    rtc.now().twelveHour();
    //f ("12 hour display selected %d\n", (int)(rtc.now().twelveHour()));
    //Serial.printf ("TimeStamp: %s\n",rtc.now().timestamp().c_str());
  }
  Date = rtc.now().timestamp(DateTime::TIMESTAMP_DATE);
  GetRTCTime();
  SetWeekDay(rtc.now().day(),rtc.now().month(),rtc.now().year());
  today = rtc.now().day();
  lcd.clear();
  
  Serial.printf("Today is: %d weekDay is: %d Week is: %s Month is: %d\n",today,weekDay,WEEK[weekDay],rtc.now().month());

  DisplayDateTime(true); //We will display second line

  //Serial.printf("Time of the hour: %d\n", rtc.now().hour());
  //Serial.printf("Is this PM: %d\n",rtc.now().isPM());
  //Serial.printf("Header info on AMPM: %d, String AMPM = %s\n",header.Ampm,ampm);
  DisplayLink();
  
  secTick = millis();
  lastTime = millis();

}

//Monitor the config pin and call Config function if required
void MonitorConfigPin()
{
    if (digitalRead(RESET) == HIGH)
        return;
    int elapse = 0;
    Serial.printf("Detected Config Request\n");
    while (digitalRead(RESET) == LOW)
    {
      elapse++;
      if (elapse > 2)
      {
        lcd.setCursor(0, 1); //Calendar line
        lcd.print("Server Start Req");
      }
      if (elapse > 10)
      {
        lcd.setCursor(0, 1); //Calendar line
        lcd.print("  Factory Reset ");
      }
      delay(1000);
    }

    if (elapse > 10)
    {
        WiFi.disconnect();
        header.id=0;
        header.ver=0;
        EraseStoredValue();
    }
    
    if (elapse >2)
    {
        bConnect = false;
        startServer();
    }

}

//Given date, month, year calculates the day and returns. Sunday is  0, Monday is 1 ...
void SetWeekDay(int d, int m, int y)
{
  y-= m < 3;
  weekDay = (y + y/4 - y/100 + y/400 + MonthTable[m-1] + d) % 7;
}


void DisplayDateTime(bool bDate)
{
  if (bDate)
  {
    lcd.clear();
    String dt = Date;  //2024-01-15
    switch(header.dateFormat)
    {
      case 0:    //Jan 10, 2024
        dt = String(MONTHS[rtc.now().month()]) + " ";
        dt += String(rtc.now().day()) + ", ";
        dt += String(rtc.now().year());
        break;
      case 1:    //10 Jan, 2024
        dt = String(rtc.now().day()) + " ";
        dt += String(MONTHS[rtc.now().month()]) + ", ";
        dt += String(rtc.now().year());
        break;
      case 2:    //15-1-2024  
        dt = String(rtc.now().day()) + "-";
        dt += String(rtc.now().month()) + "-";
        dt += String(rtc.now().year());
        break;
      case 3:    //15-1-2024  
        dt = String(rtc.now().month()) + "-";
        dt += String(rtc.now().day()) + "-";
        dt += String(rtc.now().year());
        break;         
        case 4:   //2024-01-15 
          break;
    }
    //Serial.printf("Date display: %s\n",dt.c_str());
    DisplayLink();
    lcd.setCursor(0, 1);
    lcd.print(WEEK[weekDay] + dt);
  }
  
  lcd.setCursor(0,0);
  if (header.Ampm)
    lcd.print (Time + ampm);
  else
    lcd.print (Time);

}




bool GetRTCTime()
{
  uint8_t t = rtc.now().isPM();
  ampm = AMPM[t];
  Time = rtc.now().timestamp(DateTime::TIMESTAMP_TIME);

  if (t == 1 && header.Ampm)
  {
    Time = String(rtc.now().twelveHour()) + Time.substring(Time.indexOf(':'));
  }
  return true;
}

void loop() 
{
  // put your main code here, to run repeatedly:
  //Server already running? 

  if (bServer)
  {
    server.handleClient();
    MDNS.update();
  }
  MonitorConfigPin();

  if (millis() - secTick > oneSecond)
  {
    GetRTCTime();
    DisplayDateTime(false);
    secTick = millis();
  }

  //Set the week day and refresh second line 
  if ((int)(rtc.now().day()) != today)
  {
    today = (int)rtc.now().day();
    Date = rtc.now().timestamp(DateTime::TIMESTAMP_DATE);
    SetWeekDay(rtc.now().day(),rtc.now().month(),rtc.now().year());
    bTimeCheck = true; //need to check net time at 2:00 AM possible DST
    DisplayDateTime(true);
    if (!bConnect) //Check everyday for connectivity, if network went down
    {
        connectWifi();
    }

  }

  if (rtc.now().hour() == 2)//Check Network Time at 2:00 AM in the intervels of 5 minutes until connected
  {
    if (bConnect && bTimeCheck)
    {
      //We will check every 5 minutes 
      if (rtc.now().minute() >= 5)
      {
        if (millis() - lastTime > timeDelay)
        {
          //Serial.printf("Entering time check Time now: %s\n", Time.c_str());
          if (AdjustDateTime())
          {
            bTimeCheck=false;
          }
          else
          {
            lastTime = millis();
          }
        }
      }
    }
  }
}

