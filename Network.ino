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
            //Serial.printf("Now checking the SSID %s at storage index %d with Scanned Network: %s\n",
                //Networks[i].SSID, i, WiFi.SSID(j).c_str());

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

    if (bConnect)
    {           
        lcd.setCursor(15,0);
        lcd.write(byte(0));
        NTPUpdate(false);
    }
    else
    {
       lcd.setCursor(15,0);
       lcd.write(byte(1));
       epoch += (millis()-tick)/1000;
       elapsedTime = millis();
       updateInterval = millis();
    }

    return;
}

//First 
bool tryConnect()
{
    int i = 0;
    bConnect = false;
    //Serial.printf("Trying to connect with SSID: %s, Password: %s\n", Networks[storageIndex].SSID, Networks[storageIndex].Password);

    WiFi.begin(Networks[storageIndex].SSID, Networks[storageIndex].Password);

    int status = WiFi.status();
    int startTime = millis();
    while (status != WL_CONNECTED && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED && (millis() - startTime) <= pref.Timeout * 1000)
    {
        delay(WHILE_LOOP_DELAY);
        status = WiFi.status();
    }

    if (WiFi.status() == WL_CONNECTED)
        bConnect = true;

    //Serial.printf("Connection status of SSID: %s is %d\n", Networks[storageIndex].SSID, WiFi.status());
    return bConnect;
}

//Server start when connection fails
void startServer()
{
    server.begin(); // Web server start
    Serial.println("HTTP server started");

    // Setup MDNS responder
    if (!MDNS.begin((char*)pref.Hostname))
        Serial.println("Error setting up MDNS responder!");
    else
    {
        Serial.println("mDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }
    bServer = true;
    serverTimeout = millis();
    epoch = (rtc.now()).unixtime();
        
    //lcd.setCursor(0, 1);
    //lcd.print(" Settings Req. ");
}

void sendNTPPacket() 
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    ntpUDP.beginPacket(poolServerName, 123); //NTP requests are to port 123
    ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();
}

bool NTPUpdate(bool bUpdate)
{
    if (bConnect)
    {
      sendNTPPacket();
      // Wait till data is there or timeout...
      byte timeout = 0;
      int cb = 0;
      do 
      {
          delay(10);
          cb = ntpUDP.parsePacket();
          if (timeout > 100) 
              return false; // timeout after 1000 ms
          timeout++;
      } while (cb == 0);
  
  
      ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
  
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      epoch = secsSince1900 - seventyYears;
      epoch += pref.DSTZone;
      
      if (bUpdate)
        rtc.adjust(DateTime(epoch)); //Store the value in rtc
    }
    else
      epoch = (rtc.now()).unixtime();
        
    elapsedTime = millis();
    updateInterval = millis();

}
