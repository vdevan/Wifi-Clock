void displayTime()
{
  String s;
  int hrs = getHours();
  if (hrs != currentHour)
  {
    //We will display the days and year first. This will ensure time cycles are not wasted 
    
      displayDate();
 
    //Initialise the DST settings. Once a year only
    if (runningYear != currentYear)
    {
      runningYear = currentYear;
      InitialiseDST();
    }
    
    currentHour = hrs;

 
    hrs += getDST(currentHour);
    
    String AM = hrs >= 12 ? " PM":" AM" ;  
    
    if (pref.bAMPM)
      hrs = hrs > 12 ? hrs-12 : hrs;
      
    //construct the first line of display
    s =  hrs < 10 ? "0" + String(hrs) : String(hrs);
      
    s+= ":";
    mins = getMinutes();
    s+= mins < 10 ? "0" + String(mins) : String(mins);
    s+= ":";
    secs = getSeconds();
    s += secs < 10 ? "0" + String(secs) : String(secs);
    s += AM;
    lcd.setCursor(pref.bAMPM ? 2 : 4,0);
    lcd.print(s);
    //if (bConnect)
        //Serial.printf("Current Time: %s\n", timeClient.getFormattedTime().c_str());
    //else
        //Serial.printf("Current Time: %s%d:%s%d:%s%d\n",hrs< 10 ? "0":"",hrs,mins< 10 ? "0":"",mins,secs< 10 ? "0":"",secs);
  }
  else
  {
    int minutes = getMinutes();
    if (minutes != mins)
    {
      mins = minutes;
      s = mins < 10 ? "0" + String(mins) : String(mins);
      s += ":";
      
      secs = getSeconds();
      s += secs < 10 ? "0" + String(secs) : String(secs);

      lcd.setCursor(pref.bAMPM ? 5 : 7,0);
      lcd.print(s);

      //Serial.printf("Time Now: %s%d:%s%d:%s%d\n",hrs< 10 ? "0":"",hrs,mins< 10 ? "0":"",mins,secs< 10 ? "0":"",secs);
      //if (mins % 5 == 0)
        //Serial.printf("Current Date: %s%d %s %s%d", currentDate < 10 ? "0":"",currentDate,MONTHS[currentMonth-1],currentYear < 10 ? "0":"",currentYear);

    }
    else
    {
      lcd.setCursor(pref.bAMPM ? 8 :10,0);
      secs = getSeconds();
      lcd.print(secs < 10 ? "0" + String(secs) : String(secs));
     }
  }
    
}

int getDay()
{
  return ((((epoch) / 86400L) + 4 ) % 7);
}

int getSeconds()
{
  return ((epoch) % 60);
}

int getMinutes()
{
  return ((epoch) % 3600) / 60;  
}

int getHours()
{
  return ((epoch) % 86400L) / 3600;  
}

unsigned long getEpochTime()
{
    return epoch; 

}

//Check if the time falls under DST
int getDST(int Hour)
{
  //Not yet reached DST month or past DST month
  if (currentMonth < currentDST.startMonth && currentMonth > currentDST.endMonth)
    return 0;

  //Current month is DST start month
  if (currentMonth == currentDST.startMonth)
  {
    if (currentDate < currentDST.startDate)
      return 0;
    if (currentDate > currentDST.startDate)
      return 1;
    if (currentDate == currentDST.startDate)
    {
      if (Hour < currentDST.startTime)
        return 0;
      else
        return 1;        
    }
  }
}

//Calendar month calculated from epoch
int GetMonth(int leap, int Day)
{
    int Month = 0;
    for (int i = 0; i < 13; i++)
    {
        if (DAYS[leap][i] >= Day)
            break;
        Month++;
    }
    return Month;
}


//Given date, month, year calculates the day and returns. Sunday is  0, Monday is 1 ...
int getWeekDay(int d, int m, int y)
{
  y-= m < 3;
  return (y + y/4 - y/100 + y/400 + MonthTable[m-1] + d) % 7;
}



//Called at the start of the year, will fill the DST structure
void InitialiseDST()
{
  //startAEST[] = {10,0,1,2};
  //0 = StartMonth, 1= StartDay 2= StartWeek 3 = StartTime


  //Get the weekday of the first date of DST Start month
  int firstDayOfWeek = getWeekDay(1,pref.DSTStartMonth,currentYear);
  
  //Now get the first DST Start day
  int firstDSTday = firstDayOfWeek > pref.DSTStartDay ? 8 - firstDayOfWeek + pref.DSTStartDay : pref.DSTStartDay- firstDayOfWeek + 1;

  
  currentDST.startMonth = pref.DSTStartMonth;
  if ((pref.DSTStartWeek-1) > 5) //Day is used
    currentDST.startDate = pref.DSTStartWeek;
  else
  {  
    currentDST.startDate = firstDSTday + (7*(pref.DSTStartWeek-1));
    if (currentDST.startDate > (30 + LastWeek[currentDST.startMonth]))
      currentDST.startDate = currentDST.startDate - 7;
  }
  currentDST.startTime = pref.DSTStartTime;

  //Get the weekday of the first date of DST End month
  firstDayOfWeek = getWeekDay(1,pref.DSTEndMonth,currentYear);
  
  //Now get the first DST End day
  firstDSTday = firstDayOfWeek > pref.DSTEndDay ? 8 - firstDayOfWeek + pref.DSTEndDay : pref.DSTEndDay - firstDayOfWeek + 1;

  currentDST.endMonth = pref.DSTEndMonth;
  if ((pref.DSTEndWeek-1) > 5) //Day is used
    currentDST.endDate = pref.DSTEndWeek;
  else
  {  
    currentDST.endDate = firstDSTday + (7*(pref.DSTEndWeek-1));
    if (currentDST.endDate >(30 + LastWeek[currentDST.endMonth]))
      currentDST.endDate = currentDST.endDate - 7;
  }
  currentDST.endTime = pref.DSTEndTime;

/*  Serial.printf("Year: %d startMonth:%s, startDate:%d, startTime:%d, EndMonth:%s, EndDate:%d, EndTime:%d\n", currentYear,
    MONTHS[currentDST.startMonth-1],currentDST.startDate,currentDST.startTime,
    MONTHS[currentDST.endMonth-1],currentDST.endDate,currentDST.endTime);*/
}

//Displays the date on the second line of LCD
void displayDate()
{
    unsigned long currentEpoch = getEpochTime(); //elapsed time since 1/1/1970
    int DaysElapsed = floor(currentEpoch / (60 * 60 * 24)); //total seconds in a day
    int YearElapsed = floor(DaysElapsed + 0.5) / 365.25;
    currentYear = 1970 + YearElapsed;

    int DayPosition = floor(DaysElapsed - (YearElapsed * 365.25) + 1.5);
    int isLeap = currentYear % 4 == 0 ? 1 : 0;
    currentMonth = GetMonth(isLeap, DayPosition);
    currentDate = DayPosition - DAYS[isLeap][currentMonth - 1];
    currentDay = getDay();
    

    lcd.setCursor(0, 1);
    lcd.print(String(WEEK[currentDay]) + (currentDate < 10 ? "0" + String(currentDate) : String(currentDate)) + String(MONTHS[currentMonth - 1]) + String(currentYear));
    //Serial.printf("Current Date: %s%d %s %s%d", currentDate < 10 ? "0":"",currentDate,MONTHS[currentMonth-1],currentYear < 10 ? "0":"",currentYear);


}
