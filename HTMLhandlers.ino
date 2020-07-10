//Root - Portal
void handleRoot()
{
    String Page = GetHeadPage();
    Page += ("<p><span>Set your <a href=\"/pref\">Preferences</a> here</span></p><h4>Available Networks</h4><div id=\"scnnet\">");
    
    int n = WiFi.scanNetworks();
    //Serial.println("scan done");
    if (n > 0)
    {
        for (int i = 0; i < n; i++)
        {
            Page += String(F("<div class=\"inf\"><a href=\"#p\" class=\"n\">")) + WiFi.SSID(i) + F("</a>&nbsp;<span class=\"q") + ((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F("\">") : F(" l\">")) + abs(WiFi.RSSI(i)) + " %";
            Page += F("</span></div>");
        }
    }
    else
        Page += F("<div class=\"inf\"><p>No WLAN found</p>");

    Page += F("</div>"); //Closing Div for wifi Networks info

    //Start of Form and SSID, Password fields
    Page += F("<form method=\"POST\" action=\"wifisave\"><table><tr>");
    if (storedNetworks >= STORAGE)
    {
      Page += F("<td colspan=\"2\"><h4 style=\"color:red;\">Please DELETE a Network to store new one</h4><td>");
      Page += F("</tr></table>");
   }
    else
    {
      Page += F("<td><span><label>SSID: </label><br /><input id=\"s\" class=\"txt\" name=\"s\" length=32 placeholder=\"SSID\"></span></td>"
                "<td><span><label>Password: </label><br /><input id=\"p\" class=\"txt\" name=\"p\" length=32 placeholder=\"password\"></span></td>");
      Page += F("</tr></table>");
      Page += GetSubmitPage();
    }

    Page += F("<h4>Stored Networks</h4><div id=\"strnet\"><table>");

    //Stored Network contents 
    for (int i = 0; i < storedNetworks; i++)
    {
        Page += F("<tr><td class=\"stbtn\"><label>");
        Page += Networks[i].SSID;
        Page += F("</label></td><td class=\"stbtn\"><button id=\"btn\" class=\"del\" name=\"del\" value=\"");
        Page += Networks[i].SSID;
        Page += F("\" type=\"Submit\">Delete</button></td></tr>");
    }
    Page += F("</table></div>");

    Page += F("<p>&nbsp;</p></form></div></body></html>");
    server.send(200, "text/html", Page); //Send the page to client
}

void storeEpoch(String st)
{
    char str[17];
    int index = st.indexOf('t');
    String temp = st.substring(index+1); 
    
    st = st.substring(0, index-3);
    st.toCharArray(str,sizeof(str));


    unsigned long epoch = strtoul(str,NULL,10);

    

    if (pref.DSTZone == 100000)//Time zone only used for factory reset. All other times it is ignored
    {
      temp.toCharArray(str,sizeof(str));
      int tz = atoi(str) * 60;//DStZone returned as minutes offset. convert to seconds
      tz = tz >0 ? tz : 0-tz;
      pref.DSTZone = tz; 
    }

    epoch += pref.DSTZone;
    server.send(200, "text/plain", "Clock Updated"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
    
    epoch += (millis()-tick)/1000; //adjust the clock for time elapsed
    
    currentHour = 25; //force display date
    rtc.adjust(DateTime(epoch));
    elapsedTime = millis();
    updateInterval = millis();
    InitialiseDST();
}

void handleTime()
{
  //Serial.printf("Received from client: %s\n",server.arg("time").c_str());
  tick = millis(); //keep track of time
  storeEpoch(server.arg("time"));
}

//WiFi Save
void handleWifiSave()
{
    //Serial.println("wifi save");

    String st;
    int offset = 0;
    if (server.hasArg("reset"))
    {
      sendResponse();
      //storeEpoch(server.arg("reset"));
      return;
    }
    if (server.hasArg("del")) //handle delete
    {
        st = server.arg("del");
        for (int i = 0; i < STORAGE; i++)
        {

            memset(Networks[i].SSID, '\0', sizeof(Networks[i].SSID));
            memset(Networks[i].Password, '\0', sizeof(Networks[i].Password));
            //Rearrange storage
            for (int j = i+1; j < STORAGE; j++)
            {
                memcpy(Networks[i].SSID, Networks[j].SSID, sizeof(Networks[j].SSID));
                memcpy(Networks[i].Password, Networks[j].Password, sizeof(Networks[j].Password));
                i++;
            }
            //Erase the last storage
            memset(Networks[i].SSID, '\0', sizeof(Networks[i].SSID));
            memset(Networks[i].Password, '\0', sizeof(Networks[i].Password));
            break;

        }
      }
      else
      {
        st = server.arg("s");
        st.trim();
        if (st != "")
        {
            //handle network storage
            //Pick the first empty storage
            for (int i = 0; i < STORAGE; i++)
            {
                if (Networks[i].SSID[0] == 0)
                {
                    offset = i;
                    break;
                }
            }
            server.arg("s").toCharArray(Networks[offset].SSID, sizeof(Networks[offset].SSID));
            server.arg("p").toCharArray(Networks[offset].Password, sizeof(Networks[offset].Password));
        }
      }
      sendResponse();

      //Serial.printf("storage offset to be stored: %d\n Original stored SSID length: %d\n", offset, strlen(Networks[offset].SSID));
      //storeEpoch(server.arg("save"));

      saveCredentials(false);    
}



void handlePrefSave()
{

    //Serial.println("Clock Preference");
    //Serial.printf("Clock ID: %s : Clock Password: %s \n", server.arg("cid").c_str(), server.arg("cp").c_str());
     
    if (server.hasArg("reset"))
    {
      sendResponse();
      //storeEpoch(server.arg("reset"));
      return;
    }
    else
    {
        //Get the values from webserver
        pref.ClockID = server.arg("cid").toInt();
        pref.Timeout = server.arg("to").toInt();
        pref.ServerOntime = server.arg("sot").toInt();
        server.arg("cp").toCharArray(pref.ClockPassword, sizeof(pref.ClockPassword));
        server.arg("hn").toCharArray(pref.Hostname, sizeof(pref.Hostname));

        //Serial.printf("WebServer Preferences: ClockID: %d WifiTimeout in seconds: %d Server On time: %d Clock Password: %s, Host Name: %s\n",
            //pref.ClockID, pref.Timeout, pref.ServerOntime, pref.ClockPassword, pref.Hostname);


        //Timezone
        if (server.arg("pl") == "-")
            pref.DSTZone = 0 - server.arg("tz").toInt() * 3600;
        else
            pref.DSTZone = server.arg("tz").toInt() * 3600;

  
        //User Preference
        pref.bAMPM = (server.arg("am") == "on");

        //Serial.printf("TimeZone and User Preferences: Timezone: %d  AM-PM or 24 hr?: %s, Is DST Observed?: %s\n",
            //pref.DSTZone, pref.bAMPM ? "AM/PM" : "24 Hr", pref.bDST ? "Yes" : "No");

        

        //Daylight Savings Time
        pref.bDST = (server.arg("dst") == "on");
        pref.DSTStartMonth = server.arg("sm").toInt();
        pref.DSTEndMonth = server.arg("em").toInt();
        pref.DSTStartDay = server.arg("sd").toInt();
        pref.DSTEndDay = server.arg("ed").toInt();
        pref.DSTStartWeek = server.arg("sw").toInt();
        pref.DSTEndWeek = server.arg("ew").toInt();
        pref.DSTStartTime = server.arg("st").toInt();
        pref.DSTEndTime = server.arg("et").toInt();

        //Serial.printf("Daylight Saving Timing Details: startMonth:%d, startDay:%d startWeek:%d, startTime:%d, EndMonth:%d, EndDay:%d EndWeek:%d, EndTime:%d\n",
            //pref.DSTStartMonth, pref.DSTStartDay, pref.DSTStartWeek, pref.DSTStartTime, pref.DSTEndMonth, pref.DSTEndDay, pref.DSTEndWeek, pref.DSTEndTime);

        sendResponse();
        //storeEpoch(server.arg("save"));

        saveCredentials(false);
    }
}

void handleNotFound()
{
    server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void sendResponse()
{
        server.sendHeader("Location", "/", true);
        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "-1");
        server.send(302, "text/plain", "");    
        server.client().stop(); // Stop is needed because we sent no content length
}

void handlePreference()
{
    String stub =   F("\" disabled></td>");
    String stubEn = F("\"></td>");
    String label = F("<td><span><label>");
    String optionWeek = F("<option value=\"0\">Sunday</option><option value=\"1\">Monday</option><option value=\"2\">Tuesday</option>"
        "<option value=\"3\">Wednesday</option><option value=\"4\">Thursday</option><option value=\"5\">Friday</option>"
        "<option value=\"6\">Saturday</option></select></td>");
    String replace = "value=\"";

    String Page = GetHeadPage();


    Page += F("<form method=\"POST\" action=\"prefsave\"><p></p><table><tr>"
        "<td><div class=\"btnchk\"><input type=\"checkbox\" name=\"am\" class=\"am\" ");
    Page += pref.bAMPM ? "checked=\"checked\"" : "";

    Page += F("> <label><span>Display AM/PM</span></label></div></td>"
        "<td><div class=\"btnchk\"><input type=\"checkbox\" name=\"dst\" class=\"dt\" ");
    Page += pref.bDST ? "checked=\"checked\"" : "";

    Page += F("><label><span>Enable Daylight Savings</span></label></div></td>"
        "</tr></table><p></p><table>");

    Page += F("<tr><th colspan=\"2\">Start of DST</th><th colspan=\"2\">End of DST</th></tr><tr>");
    
    Page += F("<td><label>Start Month: </label></td><td><input id=\"t\" name=\"sm\" type=\"number\" min=\"1\" max=\"12\" class=\"dst\" length=2 value=\"");
    Page += pref.DSTStartMonth;
    Page += pref.bDST ? stubEn : stub;

    Page += F("<td><label>End Month:&nbsp;</label></td><td><input id=\"t\" name=\"em\" type=\"number\" min = \"1\" max=\"12\" class=\"dst\" length=2 value=\"");
    Page += pref.DSTEndMonth;
    Page += pref.bDST ? stubEn : stub;

    Page += F("</tr><tr><td><label>Start Day:</label></td><td><select name=\"sd\" id=\"xt\" class=\"dst\"");
    Page += pref.bDST ? ">" : " disabled>";
    String str = optionWeek;
    str.replace(replace + String(pref.DSTStartDay) + "\"", replace + String(pref.DSTStartDay) + "\" selected");
    Page += str;

    Page += F("<td><label>End Day:</label></td><td><select name=\"ed\" id=\"xt\" class=\"dst\"");
    Page += pref.bDST ? ">" : " disabled>";
    str = optionWeek;
    str.replace(replace + String(pref.DSTEndDay) + "\"", replace + String(pref.DSTEndDay) + "\" selected");
    Page += str;

    Page += F("</tr><tr><td><label>Start Week:</label></td><td><input id=\"t\" name=\"sw\" type=\"number\" min=\"1\" max=\"31\" class=\"dst\" length=1 value=\"");
    Page += pref.DSTStartWeek;
    Page += pref.bDST ? stubEn : stub;

    Page += F("<td><label>End Week:&nbsp;</label></td><td><input id=\"t\" name=\"ew\" type=\"number\" min=\"1\" max=\"31\" class=\"dst\" length=1 value=\"");
    Page += pref.DSTEndWeek;
    Page += pref.bDST ? stubEn : stub;

    Page += F("</tr><tr><td><label>Start Time:</label></td><td><input id=\"t\" name=\"st\" type=\"number\" min=\"0\" max=\"12\" class=\"dst\" length=2 value=\"");
    Page += pref.DSTStartTime;
    Page += pref.bDST ? stubEn : stub;

    Page += F("<td><label>End Time:&nbsp;</label></td><td><input id=\"t\" name=\"et\" type=\"number\" min=\"0\" max=\"12\" class=\"dst\" maxlength=2 value=\"");
    Page += pref.DSTEndTime;
    Page += pref.bDST ? stubEn : stub;
       
    Page += F("</table><p><h4>Technical - Do not change if you are not sure</h4></p><table><tr>");
    Page += label;

    Page += F("Host Name: &nbsp;</label><br /><input id=\"cp\" name=\"hn\" class=\"txt\" length=13 value=\"");
    Page += pref.Hostname;
    Page += F("\" /> </span> </td>");
    Page += label;

    Page += F("Server On Time: </label><br /><input id=\"t\" class=\"sot\" name=\"sot\" type=\"number\" min=\"1\" max=\"30\" value= \"");
    Page += pref.ServerOntime;
    Page += F("\" > mins</span></td></tr><tr>");
    Page += label;

    Page += F("Clock Password: &nbsp;</label><br /><input id=\"cp\" name=\"cp\" class=\"txt\" length=32 value=\"");
    Page += pref.ClockPassword;
    Page += F("\" /></span></td>");
    Page += label;

    Page += F("Connection Time Out: </label><br /><input id=\"t\" class=\"to\" name=\"to\" type=\"number\" min=\"1\" max=\"30\" value=\"");
    Page += pref.Timeout;
    Page += F("\"> secs</span></td></tr><tr>");
    Page += label;

    Page += F("Time Zone:</label><br /><select name=\"pl\" id=\"t\" class=\"pl\" value=\" Time Zone:</label><br /><select name=\"pl\" id=\"t\" class=\"pl\" value=\"");
    Page += (pref.DSTZone > 0) ? "+" : "-";
    Page += F("\"><option>+</option><option >-</option></select>");

    Page += F("<input id=\"t\" name=\"tz\" type=\"number\" min=\"0\" max=\"23.5\" step=\"0.5\" class=\"tz\" maxlength=4 value=\"");
    Page += pref.DSTZone /3600;
    Page += F("\"></span></td>");
    Page += label;
    
    Page += F("Clock Id: </label><br /><input id=\"t\" name=\"cid\" type=\"number\" min=\"1\" max=\"254\" class=\"cl\" length=3 value=\"");
    Page += pref.ClockID;
    Page += F("\"></span></td></tr></table>");

    Page += GetSubmitPage();

    Page += F("<p>&nbsp;</p></form></div></body></html>");
    server.send(200, "text/html", Page); //Send the page to client

}

//Common Header page for Network and Preference
String GetHeadPage()
{
    String HTTP_HEAD;

    HTTP_HEAD = F("<!DOCTYPE html5><html lang=\"en\"><head>"
                    "<meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\" />"
                    "<script src=\"https://code.jquery.com/jquery-2.1.1.min.js\" type=\"text/javascript\"></script>"
                    "<script>$(document).ready(function(){var d = new Date();var tz = d.getTimezoneOffset();var dt=(Date.now()).toString()+\"t\" + tz.toString();$(\".n\").click(function(){$(\"#s\").val(this.innerText);$(\"#p\").focus();});"
                    "$(\".dt\").click(function(){$(\".dst\").prop(\"disabled\", !this.checked);});"
                    "$(\".sav\").click(function () {var d = new Date();var tz = d.getTimezoneOffset();$(\".sav\").val((Date.now()).toString() + \"t\" + tz.toString());});"
                    "$.get(\"time\",{[\"time\"]: dt}, function (result) {$(\"#retVal\").text(result);}); });</script>"
                    "<script>$(window).load(function () {var d = new Date();var tz = d.getTimezoneOffset(); $(\".pl\").val(tz < 0 ? \"+\" : \"-\"); $(\".tz\").val(tz < 0 ? 0 - tz / 60 : tz / 60);var time=d.getHours()+\":\"+d.getMinutes()+\":\"+d.getSeconds(); }); </script>"
                    "<title>WiFi Clock</title>"); 

    HTTP_HEAD += F("<style>body {background-color: #b4e0b4;}"
                    "h4 {margin:2px;} .container {margin: auto;width: 90%;min-width: 340px;background-color: #b5eafe;padding: 0px 10px;}"
                    "@media(min-width:1200px){.container {margin: auto;width: 30%;}}@media(min-width:768px) and (max-width:1200px) {.container {margin: auto;width: 40%;}}"
                    ".q {float: right;width: 64px;text-align: right;} #t{margin:2px;width:50px;padding:5px 0px;text-align:center;}"
                    ".l {background: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==') no-repeat left center; background-size: 1em;}"
                    "#scnnet{background-color: beige;padding: 5px;margin-bottom:5px;}"
                    "#strnet{background-color: bisque;padding: 5px;}"
                    ".inf {padding: 2px;}"
                    "table {width:95%;margin: auto; border - collapse: collapse;}"
                    "td.stbtn{border: 1px solid #808080;padding: 2px 10px;width:90%;}"
                    "#btn{border-radius: 4px;border: 0;color: red;cursor: pointer;display: inline-block;margin: 2px;padding: 5px;position: relative;background-color: beige;box-shadow: 0px 6px 5px -4px rgba(0,0,0,0.52);border: 0;}"
                    "#btn:hover {background: #b4e0b4;}"
                    "#btn:active, #btn:focus{background: #b5eafe;}"
                    "#btn:disabled {background-color:rgba(100,100,100,0.1);cursor:not-allowed;}"
                    ".txt{padding: 5px;width: 140px;}#xt {padding: 5px;width:90px;}</style></head>");

    HTTP_HEAD += F("<body><div class=\"container\"><h2>WiFi Clock</h2><p>You are connected to the clock: <b>");
    HTTP_HEAD += clockSSID;
    HTTP_HEAD += F("</b><br /><span id=\"retVal\"></span></p>");  //<br />Current Time: <span id=\"retVal\"></span> - Not used. Useless script still in though
    return HTTP_HEAD;
}

String GetSubmitPage()
{
    String HTTP_SUBMIT;
    HTTP_SUBMIT = F("<div style=\"text-align:center\"><p><span>"
        "<button id = \"btn\" class = \"sav\" name = \"save\" type = \"Submit\">Save and Restart</button>&nbsp;"
        "<button id = \"btn\" class = \"sav\" name = \"reset\" type = \"submit\">Restart without Save</button>"
        "</span></p></div>");
  
    return HTTP_SUBMIT;

}
