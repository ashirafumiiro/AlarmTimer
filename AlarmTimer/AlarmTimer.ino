/*
 * RTC and LCD are connected via i2c using pins A5(pin28) and A4(pin27) as
 * SCL and SDA respectively
 * PushButtons are on Arduino DigitalPins 2,3,4,5,6,7 i.e ATMEGA pins 4,5,6,11,12,13
 * LED is on Arduino digitalpin 12 or pin18 on ATMEGA
 * The LED is used to immitate a relay
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
int timeArray[3] = {0};  // to store hour, min, sec when editing time
int dateArray[3] = {0};  // to store day/month/year while editing date

int alarmsHours[NUM_ALARMS] = {1,2,3,4,5,6};
int alarmsMins[NUM_ALARMS] =  {1,2,3,4,5,6};
byte daysConfig[NUM_ALARMS][7] = {//M, T, W, T, F, S, S
                                  { 0, 1, 1, 0, 1, 0, 1 },  // Alarm 1
                                  { 1, 0, 1, 1, 0, 0, 1 },
                                  { 0, 1, 1, 0, 1, 1, 0 },
                                  { 1, 0, 1, 1, 0, 0, 1 },
                                  { 0, 1, 1, 0, 1, 0, 1 }, 
                                  { 1, 1, 0, 0, 0, 0, 0 }   // Alarm 6
                                  
                                };  // all seven days initialise as off.
String daysLong[]  = {"Mon ", "Tue ", "Wed ", "Thur", "Fri ", "Sat ", "Sun "};

int currentPage = 0;
int prevPage = 0;
int horizontalPosition = 0;
int verticalPosition = 0;
int currentLevel = 0; // 0-scroll, 1-edit 

String prevDate = "";


void setup()
{
  lcd.init();
  lcd.backlight();
  rtc.begin();
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(9600);
  Serial.println(rtc.getDOWStr());
}

//////////////////////////////////   LOOP    ////////////////////////////////////////////////////////

void loop () {
  
//    if(readButtonState(LEFT)){
//     Serial.println("LEFT");
//      
//    }
//
//    if(readButtonState(RIGHT)){
//      Serial.println("RIGHT");
//    }

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
    }

//    if(readButtonState(BACK)){
//      Serial.println("BACK");
//    }
    displayScreen();
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
    int h = alarmsHours[currentPage-1];
    int m = alarmsMins[currentPage-1];
    lcd.setCursor(0,0);
    lcd.print(alarmsMenu[currentPage]);  
    displayAlarmData();
    
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
  lcd.print(": "+prependZero(alarmsHours[0])+":" + prependZero(alarmsMins[0]));
  lcd.setCursor(0,1);
  lcd.print(F("Mon "));  lcd.print(0);
  lcd.setCursor(0,2);
  lcd.print(F("Tue "));  lcd.print(0);
  lcd.setCursor(0,3);
  lcd.print(F("Wed "));  lcd.print(0);
  // new column
  lcd.setCursor(7,1);
  lcd.print(F("Thu "));  lcd.print(0);
  lcd.setCursor(7,2);
  lcd.print(F("Fri "));  lcd.print(0);
  lcd.setCursor(7,3);
  lcd.print(F("Sat "));  lcd.print(0);
  //Last column
  lcd.setCursor(14,1);
  lcd.print(F("Sun "));  lcd.print(0);
  
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


void settingAlarms(int alarmNumber){

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

    if(back){
      
    }

    if(back){
      
    }

    if(back){
      
    }

    if(back){
      
    }

    if(back){
      
    }

    if(back){
      
    }
      
  }
  
}
