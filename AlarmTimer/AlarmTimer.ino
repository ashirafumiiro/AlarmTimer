/*
 * RTC and LCD are connected via i2c using pins A5(pin28) and A4(pin27) as
 * SCL and SDA respectively
 * PushButtons are on Arduino DigitalPins 2,3,4,5,6,7 i.e ATMEGA pins 4,5,6,11,12,13
 * LED is on Arduino digitalpin 12 or pin18 on ATMEGA
 * The LED is used to immitate a relay
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * NOTE: Manual Alarm will be made available by an interrupt.
 */

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <EEPROM.h>


#define RELAY_PIN 12
#define LEFT      3
#define RIGHT     4
#define UP        2
#define DOWN      5
#define SELECT    7
#define BACK      6


#define NUM_ALARMS      6
#define NUM_PAGES      7

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);
LiquidCrystal_I2C lcd(0x27, 20, 4);

String alarmsMenu[] = {"Current Time", "Alarm1", "Alarm2", "Alarm3", "Alarm4", "Alarm5", "Alarm6"}; // First item is for current Time

int alarmsTime[NUM_ALARMS][2] = {0};   // {HH,MM}
bool daysConfig[NUM_ALARMS][7] = {0};  // all seven days initialise as off.
                                  
                                
String daysLong[]  = {"Mon ", "Tue ", "Wed ", "Thur", "Fri ", "Sat ", "Sun "};

int currentPage = 0;
int prevPage = 0;
int horizontalPosition = 0;
int verticalPosition = 0;
int currentLevel = 0; // 0-scroll, 1-edit 

String prevDate = "";

// Contro ringing
bool isRing = false;
int ringInterval = 2000; // 2s
int ringNumber = 3;  //ring 3 times
int lastActiveAlarmDate = 0;
int lastActiveAlarmMins = 0;
int lastActiveAlarmHour = 0;
int lastActiveAlarm = 0;
long long startAlarmTime = 0;
bool pauseAlarm = true;
int elapsedRings = 0;
int testDay = 0;


void setup()
{
  lcd.init();
  lcd.backlight();
  rtc.begin();
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(9600);
  //Uncomment to initialize for first time, then re-appload
  //saveToEEPROM();

  
  //Read larms config from eeprom
  int eeAddress = 0;
  EEPROM.get(eeAddress, alarmsTime);
  eeAddress += sizeof(alarmsTime);
  EEPROM.get(eeAddress, daysConfig);
}

//////////////////////////////////   LOOP    ////////////////////////////////////////////////////////

void loop () {

    //Scrolling alarms and allow entering settings
    if(readButtonState(UP)){
      if(currentPage > 0)
        currentPage--;
    }

    if(readButtonState(DOWN)){
          if(currentPage < (NUM_PAGES-1))
            currentPage++;
    }

    if(readButtonState(SELECT)){
        if(currentPage == 0){  // Enter time and date settings
          settingTime();
        }
        else{
          settingAlarms();
        }
    }

    if(readButtonState(BACK)){
      currentPage = 0;
    }
    
    displayScreen();

    // Check for alarms
    Time now = rtc.getTime();
    int h = now.hour ;
    int m = now.min;
    int d = now.date;  // to help disable alarm
    int Month = now.mon;
    for(int i=0; i<NUM_ALARMS; i++){
       if(h==alarmsTime[i][0] && m==alarmsTime[i][1]){ // alarm time matches, proceed with other checks     
          if(daysConfig[i][getDayIndex(rtc.getDOWStr())-1]==true){  // Match day
            /*
             *  int lastActiveAlarmDate = 0;
                int lastActiveAlarmHour = 0
                int lastActiveAlarm = 0;
            */
            if((lastActiveAlarmDate != d)||(lastActiveAlarmHour!=h)||(lastActiveAlarm!=i)||(lastActiveAlarmMins!=m)){  // alarms has not yet rang today
              Serial.println(F("Activated alarm"));
              isRing = true;
              lastActiveAlarmDate = d;  // Alarm done for today
              lastActiveAlarmHour = h;
              lastActiveAlarm =i;
              lastActiveAlarmMins = m;
              startAlarmTime = millis();
              pauseAlarm = false;
              Serial.print(F("Started"));
            }   
          }
       }
    }

    if(isRing){
      if((millis() - startAlarmTime) > ringInterval){
        startAlarmTime = millis();  // reset interval
        if(!pauseAlarm)elapsedRings++;
        pauseAlarm = !pauseAlarm;
        if(elapsedRings >= ringNumber)
        {
          isRing = false;
          pauseAlarm = true;
          elapsedRings = 0;
        }       
      }
    }

    if(!pauseAlarm){
      digitalWrite(RELAY_PIN, 1);
    }
    else{
      digitalWrite(RELAY_PIN, 0);
    }
    
   
}


///////////////////////////////////////////   END LOOP /////////////////////////////////////////////////


void displayTime(){
    Time now = rtc.getTime();
    int h = now.hour ;
    int m = now.min;
    int s = now.sec;
    int d = now.date;
    int Month = now.mon;
    int y = now.year;
    lcd.setCursor(0,0);
    showTime(h, m, s);
    if((prevDate != rtc.getDateStr())||(prevPage != currentPage)){
      lcd.setCursor(0,2);
      showDate(d, Month,y); 
      lcd.setCursor(0,3);
      lcd.print(rtc.getDOWStr());
      prevDate = rtc.getDateStr();
    }
    
}



void displayScreen(){
  if(currentPage == 0){
    if(prevPage) lcd.clear();  // clear screen if you are from other pages
      displayTime(); 
  }
  else{
    if(!prevPage) lcd.clear();  // clear page each you are from home page. alot of data defers
     
    if(prevPage != currentPage){
      displayAlarmData();
    }  
  }  

  prevPage = currentPage;
}

bool readButtonState(int buttonPin){
  if(digitalRead(buttonPin)){
    delay(199); // tuned delay to avoid multiple run of button
    return true;
  }
  return false;
}

void displayAlarmData(){
  int alarmIndex = currentPage-1;
  lcd.setCursor(0,0);
  lcd.print(alarmsMenu[currentPage]); 
  lcd.print(": "+prependZero(alarmsTime[alarmIndex][0])+":" + prependZero(alarmsTime[alarmIndex][1]));
  lcd.setCursor(0,1);
  lcd.print(F("Mon "));  lcd.print(daysConfig[alarmIndex][0]);
  lcd.setCursor(0,2);
  lcd.print(F("Tue "));  lcd.print(daysConfig[alarmIndex][1]);
  lcd.setCursor(0,3);
  lcd.print(F("Wed "));  lcd.print(daysConfig[alarmIndex][2]);
  // new column
  lcd.setCursor(7,1);
  lcd.print(F("Thu "));  lcd.print(daysConfig[alarmIndex][3]);
  lcd.setCursor(7,2);
  lcd.print(F("Fri "));  lcd.print(daysConfig[alarmIndex][4]);
  lcd.setCursor(7,3);
  lcd.print(F("Sat "));  lcd.print(daysConfig[alarmIndex][5]);
  //Last column
  lcd.setCursor(14,1);
  lcd.print(F("Sun "));  lcd.print(daysConfig[alarmIndex][6]);
  
}

String prependZero(int val){
  if (val < 10)
    return String("0")+String(val);
  
  return String(val);
}


void settingTime(){
    Time now = rtc.getTime();
    int h = now.hour ;
    int mins = now.min;
    int s = now.sec;
    int d = now.date;
    int mon = now.mon;
    int y = now.year;
    int dayOfWeekIndex = getDayIndex(rtc.getDOWStr());
    lcd.clear();
    lcd.setCursor(0,0);
    showDate(d, mon, y);
    lcd.setCursor(0,3);
    showTime(h, mins, s);
    lcd.setCursor(0,1);
    lcd.print(daysLong[dayOfWeekIndex-1]);
    lcd.setCursor(7,0);
    lcd.blink();
    bool settingMode = true;
    bool btnPressed = false;

    int setting = 1; // 1-day, 2-month, 3-year, 4-day of week 5-hour, 6-min, 7-seconds
    
    while(settingMode){
      // readbuttons to determine action
      bool left = readButtonState(LEFT);
      bool right = readButtonState(RIGHT);
      bool up = readButtonState(UP);
      bool down = readButtonState(DOWN);
      bool select = readButtonState(SELECT);
      bool back = readButtonState(BACK);

      if(back){  //cancel settings
        settingMode = false;
        prevPage = -1;   // enable refreshing screen
        currentPage = 0;
        lcd.noBlink();
      }

      if(select){  //select settings
        if(setting >4){  // already in time settings. Save settings and quit
          rtc.setDOW(dayOfWeekIndex);     // Set Day-of-Week to SUNDAY
          rtc.setTime(h, mins, s);     // Set the time to 12:00:00 (24hr format)
          rtc.setDate(d, mon, y);   // Set the date to January 1st, 2014  

          settingMode = false;
          prevPage = -1;   // enable refreshing screen
          currentPage = 0;
          lcd.noBlink();
        }
        else {// jump from date setting to time setting
          setting = 5;
          btnPressed = true;
        }
        
      }

      if(left){
        if(setting > 1) // move backward
          setting--;
        btnPressed = true;
      }

      if(right){
        if(setting < 7) // move forward
          setting++;
        btnPressed = true;
      }

      if(up){   // Increment current value
        switch(setting){
          case 1:
            if((mon==1 || mon==3 || mon==5 || mon==7 || mon==8 || mon==10 || mon== 12)&& d < 31)  // months with 31 days
              d++;
            if((mon==4 || mon==6 || mon==9 || mon==11)&& d < 30)  // months with 30 days
              d++;
            if((mon==2) && d < 29){  // leap years are not considered. user must be careful when setting this
              d++;
            }
            break;
          case 2:
            if(mon<12)
              mon++;
            break;
          case 3:
            y++;
            break;
          case 4:
            if(dayOfWeekIndex < 7)
              dayOfWeekIndex++;
            break;
          case 5:
            h++;
            if(h==24) h=0; // reset to 0 after 23
            break;
          case 6:
            mins++;
            if(mins == 60)mins=0;
            break;
          case 7:
            s++;
            if(s == 60)s=0;
            break;
        }
        btnPressed = true;
      }

      if(down){   // Decrement current value
        switch(setting){
          case 1:
            if(d>1)d--;
            break;
          case 2:
            if(mon>1)mon--;
            break;
          case 3:
            if(y>1)y--;
            break;
          case 4:
            if(dayOfWeekIndex>1)dayOfWeekIndex--;
            break;
          case 5:
            if(h>0)h--;
            break;
          case 6:
            if(mins>0)mins--;
            break;
          case 7:
            if(s>0)s--;
            break;
        }
        btnPressed = true;
      }
      
      if(btnPressed){
        lcd.setCursor(0,0);
        showDate(d, mon, y);
        lcd.setCursor(0,3);
        showTime(h, mins, s);
        lcd.setCursor(7,0);
        switch(setting){
          case 1:
            lcd.setCursor(7,0);
            break;
          case 2:
            lcd.setCursor(10,0);
            break;
          case 3:
            lcd.setCursor(15,0);
            break;
          case 4:
            lcd.setCursor(0,1);
            lcd.print(daysLong[dayOfWeekIndex-1]);
            break;
          case 5:
            lcd.setCursor(7,3);
            break;
          case 6:
            lcd.setCursor(10,3);
            break;
          case 7:
            lcd.setCursor(13,3);
            break;
            
        }
        btnPressed = false;
      }
    }
    
}

void showDate(int d, int m, int y){
      lcd.print(F("Date: "));
      lcd.print(prependZero(d));
      lcd.print('/');
      lcd.print(prependZero(m));
      lcd.print('/');
      lcd.print(y); 
}

void showTime(int h, int m, int s){
  lcd.print(F("Time: "));
    lcd.print(prependZero(h));
    lcd.print(':');
    lcd.print(prependZero(m));
    lcd.print(':');
    lcd.print(prependZero(s));
}

//String daysLong[]  = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
int getDayIndex(String dayStr){
  if(dayStr == "Monday")
    return 1;
  if(dayStr == "Tuesday")
    return 2;
  if(dayStr == "Wednesday")
    return 3;
  if(dayStr == "Thursday")
    return 4;
  if(dayStr == "Friday")
    return 5;
  if(dayStr == "Saturday")
    return 6;
  if(dayStr == "Sunday")
    return 7;

}


void settingAlarms(){

  int alarmIndex = currentPage-1;
  
  bool settingMode = true;
  bool btnPressed = false;

  int setting = 1; // 1-hour, 2-min, 3-mon, 4-tue  5-wed, 6-thur, 7-fri, 8-sat, 9-san

  lcd.blink();
  lcd.setCursor(9,0);
  while(settingMode){
    // readbuttons to determine action
    bool left = readButtonState(LEFT);
    bool right = readButtonState(RIGHT);
    bool up = readButtonState(UP);
    bool down = readButtonState(DOWN);
    bool select = readButtonState(SELECT);
    bool back = readButtonState(BACK);

    if(back){
      prevPage++;
      lcd.noBlink();
      settingMode = false;
    }

    if(select){
      if(setting < 3){ // go set days
        setting = 3;
        btnPressed = true;
      }
      else{ //Already setting days, finish
        // Save data to EEPROM
        saveToEEPROM();
        prevPage++;
        lcd.noBlink();
        settingMode = false;
      }
      
      
    }

    if(up){
        switch(setting){
          case 1:
            alarmsTime[alarmIndex][0]++;
            if(alarmsTime[alarmIndex][0]==24) alarmsTime[alarmIndex][0]=0; // reset to 0 after 23
            break;
          case 2:
            alarmsTime[alarmIndex][1]++;
            if(alarmsTime[alarmIndex][1] == 60)alarmsTime[alarmIndex][1]=0;
            break;
          case 3:
            daysConfig[alarmIndex][0] = !daysConfig[alarmIndex][0]; // change values between 1 and 0
            break;
          case 4:
            daysConfig[alarmIndex][1] = !daysConfig[alarmIndex][1];
            break;
          case 5:
            daysConfig[alarmIndex][2] = !daysConfig[alarmIndex][2];
            break;
          case 6:
            daysConfig[alarmIndex][3] = !daysConfig[alarmIndex][3];
            break;
          case 7:
            daysConfig[alarmIndex][4] = !daysConfig[alarmIndex][4];
            break;
          case 8:
            daysConfig[alarmIndex][5] = !daysConfig[alarmIndex][5];
            break; 
          case 9:
            daysConfig[alarmIndex][6] = !daysConfig[alarmIndex][6];
            break;
        }
        btnPressed = true;    
    }

    if(down){
        switch(setting){
          case 1:
            if(alarmsTime[alarmIndex][0]>0)alarmsTime[alarmIndex][0]--;
            break;
          case 2:
            if(alarmsTime[alarmIndex][1]>0)alarmsTime[alarmIndex][1]--;
            break;
          case 3:
            daysConfig[alarmIndex][0] = !daysConfig[alarmIndex][0]; // change values between 1 and 0
            break;
          case 4:
            daysConfig[alarmIndex][1] = !daysConfig[alarmIndex][1];
            break;
          case 5:
            daysConfig[alarmIndex][2] = !daysConfig[alarmIndex][2];
            break;
          case 6:
            daysConfig[alarmIndex][3] = !daysConfig[alarmIndex][3];
            break;
          case 7:
            daysConfig[alarmIndex][4] = !daysConfig[alarmIndex][4];
            break;
          case 8:
            daysConfig[alarmIndex][5] = !daysConfig[alarmIndex][5];
            break; 
          case 9:
            daysConfig[alarmIndex][6] = !daysConfig[alarmIndex][6];
            break;
        }
        btnPressed = true;
    }

    if(left){
       if(setting > 1) // move backward
          setting--;
          btnPressed = true;
     }

     if(right){
       if(setting < 9) // move forward
         setting++;
         btnPressed = true;
     }

    if(btnPressed){
        updateAlarmSettingScreen(alarmIndex,setting);
        switch(setting){
          case 1:
            lcd.setCursor(9,0);
            break;
          case 2:
            lcd.setCursor(12,0);
            break;
          case 3:
            lcd.setCursor(4,1);
            break;
          case 4:
            lcd.setCursor(4,2);
            break;
          case 5:
            lcd.setCursor(4,3);
            break;
          case 6:
            lcd.setCursor(11,1);
            break;
          case 7:
            lcd.setCursor(11,2);
            break;
          case 8:
            lcd.setCursor(11,3);
            break; 
          case 9:
            lcd.setCursor(18,1);
            break; 
        }
        btnPressed = false;
      }
      
  }
  
}

void updateAlarmSettingScreen(int alarmIndex, int setting){
  switch(setting){
          case 1:
          case 2:
            lcd.setCursor(8,0);
            lcd.print(prependZero(alarmsTime[alarmIndex][0])+":" + prependZero(alarmsTime[alarmIndex][1]));
            break;
          case 3:
            lcd.setCursor(4,1);
            lcd.print(daysConfig[alarmIndex][0]);
            break;
          case 4:
            lcd.setCursor(4,2);
            lcd.print(daysConfig[alarmIndex][1]);
            break;
          case 5:
            lcd.setCursor(4,3);
            lcd.print(daysConfig[alarmIndex][2]);
            break;
          case 6:
            lcd.setCursor(11,1);
            lcd.print(daysConfig[alarmIndex][3]);
            break;
          case 7:
            lcd.setCursor(11,2);
            lcd.print(daysConfig[alarmIndex][4]);
            break;
          case 8:
            lcd.setCursor(11,3);
            lcd.print(daysConfig[alarmIndex][5]);
            break; 
          case 9:
            lcd.setCursor(18,1);
            lcd.print(daysConfig[alarmIndex][6]);
            break; 
        }
}

void saveToEEPROM(){
  int eeAddress = 0;
  EEPROM.put(eeAddress, alarmsTime); // first stores the two element array of time and min
  eeAddress += sizeof(alarmsTime);
  EEPROM.put(eeAddress, daysConfig);
}
