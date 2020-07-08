/** Load WLAN credentials from EEPROM */

void loadCredentials()
{
    EEPROM.begin(512);
    char valid[6];
    int offset = 0;
    EEPROM.get(offset, pref);
    offset += sizeof(PREF);
    EEPROM.get(offset, valid);
    offset += sizeof(valid);

    storedNetworks = 0;
    for (int i; i < STORAGE; i++)
    {
        EEPROM.get(offset, Networks[i]);
        offset += sizeof(NETWORK);
        if (strlen(Networks[i].SSID) > 0)
            storedNetworks++;
    }
    EEPROM.end();

    
    //Restore Preferences to factory default
    if (String(valid) == String("ERASE"))
        loadDefaults();
    else
    {
      if(String(valid) != String("VALID")) //First time? Prepare the memory
      {
        WiFi.disconnect();
        EraseStoredValue();    
      }
    }

    
      /*  Serial.printf("WebServer Preferences: ClockID: %d WifiTimeout in seconds: %d Server On time: %d Clock Password: %s, Host Name: %s\n",
        pref.ClockID, pref.Timeout, pref.ServerOntime, pref.ClockPassword, pref.Hostname);

    Serial.printf("TimeZone and User Preferences: Timezone: %d  AM-PM or 24 hr?: %s, Is DST Observed?: %s\n",
        pref.DSTZone, pref.bAMPM ? "AM/PM" : "24 Hr", pref.bDST ? "Yes" : "No");
    Serial.printf("Daylight Saving Timing Details: startMonth:%d, startDay:%d startWeek:%d, startTime:%d, EndMonth:%d, EndDay: %d EndWeek:%d, EndTime:%d\n",
        pref.DSTStartMonth,pref.DSTStartDay, pref.DSTStartWeek, pref.DSTStartTime, pref.DSTEndMonth,pref.DSTEndDay, pref.DSTEndWeek, pref.DSTEndTime);
    

    for (int i = 0; i < STORAGE; i++)
    {
        Serial.printf("READ From Storage[%d]: SSID: %s, Password: %s\n",
            i + 1, Networks[i].SSID, Networks[i].Password);
    }*/
}

void EraseStoredValue()
{
    for (int i = 0; i < STORAGE; i++)
    {
        memset(Networks[i].SSID, '\0', sizeof(Networks[i].SSID));
        memset(Networks[i].Password, '\0', sizeof(Networks[i].Password));
    }
    loadDefaults();
    Serial.println("Erasing the stored contents");
    saveCredentials(true);
}

/** Store WLAN credentials to EEPROM */
void saveCredentials(bool bErase)
{
    EEPROM.begin(512);
    int offset = 0;
    EEPROM.put(offset, pref);
    offset += sizeof(PREF);
    EEPROM.put(offset, (bErase) ? "ERASE" : "VALID");
    offset += 6;

    for (int i = 0; i < STORAGE; i++)
    {
        EEPROM.put(offset, Networks[i]);
        offset += sizeof(NETWORK);
    }
    EEPROM.commit();

    Serial.println("Saving Network Configuration");

    EEPROM.end();
    delay(5000);
    ESP.reset();
}

void loadDefaults()
{
    //Load defaults for first time
    memcpy(pref.ClockPassword, CLOCKPASSWORD, sizeof(CLOCKPASSWORD));
    memcpy(pref.Hostname, HOSTNAME, sizeof(HOSTNAME));
    pref.Timeout = WIFI_TIMEOUT; // will be used at HTML page defaults.This will ensure always server on
    pref.ServerOntime = SERVER_ON_TIME;
    pref.bAMPM = true;
    pref.bDST = true;
    pref.ClockID = 1;
    pref.DSTEndMonth = 4;
    pref.DSTEndDay = 0;
    pref.DSTEndTime = 2;
    pref.DSTEndWeek = 1;
    pref.DSTStartMonth = 10;
    pref.DSTStartDay = 0;
    pref.DSTStartTime = 2;
    pref.DSTStartWeek = 1;
    pref.DSTZone = 100000; //invalid time that needs to set for first time
    
}
