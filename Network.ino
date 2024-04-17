//Check the stored networks and scanned networks. try to connect all available network
//if connected storageIndex is properly set.
void connectWifi()
{
    //Serial.println("Connecting as wifi client...");
    int n = WiFi.scanNetworks();
    unsigned long tick = millis();
    storageIndex = -1;
    int j;
    //Serial.printf("Total Scanned network: %d\n", n);
    //Check if scannednetworks is available in our storage
    for (int i = 0; i < storedNetworks; i++)
    {
      for (j = 0; j < n; j++)
      {
        Serial.printf("Now checking the SSID %s at storage index %d with Scanned Network: %s\n",
              Networks[i].SSID, i, WiFi.SSID(j).c_str());

        if (WiFi.SSID(j).compareTo(String(Networks[i].SSID)) == 0)
        {
            storageIndex = i;
            break;
        }
      }
      if (j < n)  //storageIndex >= 0
      {
        if (tryConnect())
            break;
      }
    }

    DisplayLink();

    return;
}

void DisplayLink()
{
    if (bConnect)
    {           
      
      lcd.setCursor(15,0);
      lcd.write(byte(0));

    }
    else
    {
      lcd.setCursor(15,0);
      lcd.write(byte(1));
    }
}

//First 
bool tryConnect()
{
    int i = 0;
    bConnect = false;
    Serial.printf("Trying to connect with SSID: %s, Password: %s\n", Networks[storageIndex].SSID, Networks[storageIndex].Password);

    WiFi.begin(Networks[storageIndex].SSID, Networks[storageIndex].Password);

    int status = WiFi.status();
    int startTime = millis();
    while (status != WL_CONNECTED && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED && (millis() - startTime) <= WIFI_TIMEOUT * 1000)
    {
        delay(WHILE_LOOP_DELAY);
        status = WiFi.status();
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        bConnect = true;
    }

    Serial.printf("Connection status of SSID: %s is %d\n", Networks[storageIndex].SSID, WiFi.status());
    
    return bConnect;
}

//Server start when connection fails
void startServer()
{
    lcd.setCursor(0, 0); //First line
    lcd.print("Webserver Start");
    lcd.setCursor(0, 1); //Second line
    lcd.print(" IP:172.23.1.1  ");
    server.begin(); // Web server start
    Serial.println("HTTP server started");

    // Setup MDNS responder
    if (!MDNS.begin(HOSTNAME))
        Serial.println("Error setting up MDNS responder!");
    else
    {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }
    bServer = true;

}

//GET request API and get data as Json Object for weather
String GET_Request(const char* uri) 
{
  HTTPClient http;    
  WiFiClient wifiClient;

  http.begin(wifiClient,uri);
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) 
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else 
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}

//Get the data from network and store in rtc. Called at 2:02 everyday
bool AdjustDateTime()
{
  //Get the WAN Port. Most Service providers change IP address. Hence get them everyday
  if (!GetWANPort())
  {
    return false;  //Continue with our time. Return Elapsed time
  }

  //Serial.printf("Getting Time using WAN Port IP: %s\n",WANPortIP.c_str());
  long netlapse = millis();

  //Get DateTime
  jsonArray = GET_Request((TimeUri + WANPortIP).c_str());
  myObj = JSON.parse(jsonArray);
  if (JSON.typeof(myObj) == "undefined") 
  {
    return false; //Use the current data
  }
  String dt = (myObj["datetime"]);
  //If getting time from network is delayed we will discard the data
  if (millis()- netlapse < oneSecond * 2) 
  {
   
    rtc.adjust(DateTime(dt.c_str()));
    Serial.printf("Adjusted rtc. Total time elapsed %d\n", millis() - netlapse);
    //Serial.printf("Value obtained: %s\n", dt.c_str());
    //Serial.printf("Value used: %s\n", DateTime(dt.c_str()).timestamp().c_str());
    return true;
  }
    
  Serial.printf("Discarded data. Total time elapsed %d", millis() - netlapse); 
  return false;

}

bool SetDateTime(String TimeStamp)
{
  int index = TimeStamp.indexOf('.'); 
  String dt;
  String tm;

  //Avoid millisecond
  if (index >= 0) 
    TimeStamp = TimeStamp.substring(0,index);

  
  if (TimeStamp.length()!=TSLENGTH) //Check Length
    return false;

  index  = TimeStamp.indexOf('T');
  

  if (index != DATELENGTH)
    return false;

  dt = TimeStamp.substring(0,index);
  tm = TimeStamp.substring(index +1);
 
  //Serial.printf ("Date Value obtained: %s and Time Value obtained: %s\n",dt.c_str(),tm.c_str());

  if (tm.length()!= TIMELENGTH)
    return false;
    
  int yyyy = dt.substring(0,4).toInt();
  //Serial.printf("Year calculated: %d ",yyyy);
  if (yyyy < 1970)
    return false;

  uint8_t leap = (yyyy % 4 == 0) ? 1 : 0 ;
  //Serial.printf("Leap calculated: %d ",( uint8_t)leap);

  uint8_t mm = (uint8_t)dt.substring(5,7).toInt();
  //Serial.printf("Month calculated: %d ",( uint8_t)mm);

  if (mm < 1 || mm > 12)
    return false;

  uint8_t dd = (uint8_t)dt.substring(8).toInt();  
  //Serial.printf("Day calculated: %d\n",( uint8_t)dd);

  if (dd < 0)
    return false;

  if (mm == 2 &&  dd > MDATE[mm] + leap)
    return false;

  if (mm !=2 && dd > MDATE[mm])
    return false;

  uint8_t hh =(uint8_t)tm.substring(0,2).toInt();
  //Serial.printf("Hour calculated: %d ",( uint8_t)hh);

  if (hh < 0 || hh > 23)
    return false;
  
  uint8_t mn = (uint8_t)tm.substring(3,5).toInt();
  //Serial.printf("Minute calculated: %d ",( uint8_t)mn);

  if (mn < 0 || mn > 59)
    return false;

  uint8_t ss = (uint8_t)tm.substring(6).toInt();
  //Serial.printf("Seconds calculated: %d ",( uint8_t)ss);

  ss = (ss > 59 || ss < 0) ? 0 : ss;
  //Serial.printf("Seconds Used: %d ",ss);

  rtc.adjust(DateTime(yyyy,mm,dd,hh,mn,ss));
  //DateTime(dt.c_str(),tm.c_str());
  Serial.printf ("DateTime Value stored: %s\n",rtc.now().timestamp().c_str());


  return true;

}

//Get WAN Port Address Gateway address may change anytime. Better to get this every 10 mins
bool GetWANPort()
{
  //Serial.printf("Getting Gateway IP: %s\n",GatewayUri.c_str());
  
  jsonArray = GET_Request(GatewayUri.c_str());
  myObj = JSON.parse(jsonArray);

  if (JSON.typeof(myObj) == "undefined") 
  {
    return false; //Use the current data
  }
  WANPortIP = String(myObj["ip"]);
  //WANPortIP = WANPortIP.substring(1,WANPortIP.length()-1);

  //Serial.printf("Gateway Address found: %s\n",WANPortIP.c_str());
  return true;
} 

