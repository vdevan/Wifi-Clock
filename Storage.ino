/** Load WLAN credentials from EEPROM 
* There are 512 byte storage buffer available. We will use 
* this to store up to 6 WiFi configurations 32 byte for SSID
* and 32 byte for password. That will occupy 33*6*2 = 396 bytes in total
* to ensure we have the right data we will store header info hich will be
* a Prog ID as UINT16 and a version info as float. This will be the first
* header information along with UINT8 for stored networks Header will 
* occupy 8 + 4 + 1 = 13 or 14 with end bytes. Thus we will use 396+14 = 410 bytes
* ***/

//If valid data exists in EEPROM read it and return true. else return false
bool loadCredentials()
{
  EEPROM.begin(512);
  int offset = 0;
  EEPROM.get(offset, header);
  offset += sizeof(header);

  if ((header.id != progId) || (header.ver != version))
  {
    Serial.println("Valid header not found");
    for (int i=0;i<512; i++)
      EEPROM.write(i, '\0'); //Erase all values first
    
    header.id = progId;
    header.ver = version;
    header.dateFormat = 0;
    header.Ampm = false;
    Serial.println("Erasing the stored contents");
    saveCredentials();
    return true;
  }     

  storedNetworks = 0;
  for (int i = 0; i < MAXNETWORK; i++)
  {
    EEPROM.get(offset, Networks[i]);
    offset += sizeof(NETWORK);
    Serial.printf("READ From Storage[%d]: SSID: %s, Password: %s\n", i + 1, Networks[i].SSID, Networks[i].Password);
    if (strlen(Networks[i].SSID) > 0)
      storedNetworks++;
  }
  EEPROM.end();
  Serial.printf("Program ID: %d\nProgram Version: %d\nStored Network: %d\n",header.id,header.ver,storedNetworks );

  return true;   
}

void EraseStoredValue()
{
    for (int i = 0; i < MAXNETWORK; i++)
    {
        memset(Networks[i].SSID, '\0', sizeof(Networks[i].SSID));
        memset(Networks[i].Password, '\0', sizeof(Networks[i].Password));
    }
    
    Serial.println("Erasing the stored contents");
    saveCredentials();


}

/** Store WLAN credentials to EEPROM */
void saveCredentials()
{
    EEPROM.begin(512);
    int offset = 0;
    EEPROM.put(offset, header);
    offset += sizeof(header);

    for (int i = 0; i < MAXNETWORK; i++)
    {
        EEPROM.put(offset, Networks[i]);
        offset += sizeof(NETWORK);
    }
    EEPROM.commit();

    Serial.println("Saving Network Configuration");
    Serial.println("Values in header");
    Serial.printf("Header ID: %d",header.id);
    Serial.printf(" Header Version: %d",header.ver);
    Serial.printf(" Header AM/PM: %d",header.Ampm);
    Serial.printf(" Header ID: %d\n",header.dateFormat);
 
    EEPROM.end();
    delay(5000);
    ESP.reset();
}

