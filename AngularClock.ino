
/***************************************************

  This program is designed for the Wicked Device Angular Clock v1 

  For more information and to buy go to:

  ----> http://shop.wickeddevice.com/product/angular-clock-kit/


  Written by Ken Rother for Wicked Device LLC.

  MIT license, all text above must be included in any redistribution

 ****************************************************/
#define VERSION "V1.0"

#include <Time.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <MCP7941x.h>

MCP7941x rtc=MCP7941x();

#define ENCODER_DO_NOT_USE_INTERRUPTS
#define ENCA 3
#define ENCB 2

Encoder myEnc(ENCA,ENCB);

// Map Meters to Arduino Pins Meter1 is on the left of clock
#define METER1 9 //purple wire
#define METER2 6 //blue wire
#define METER3 5 //orange wire

// Associate METER1, METER2, METER3 with output functions -- SECONDS,MINUTES,HOURS,TEMPERATURE,HOURS2,HOURS3 
// (HOURS2,HOURS2 are for multitimezone clock)
#define HOURS METER1
#define MINUTES METER2
#define SECONDS METER3

#define TOTAL_METERS 10
#define MIN_ANALOG 0
#define MAX_ANALOG 255


// LED Pins
#define RED 11
#define GREEN 10

// Time delays
#define SECONDS_1 1000 //ms
#define FLASH_DELAY 200 //ms

#define LED_STARTUP_FLASHES 10

int oldMeterValue[TOTAL_METERS] = {0,};

tmElements_t tm;

// utility to print current time to console

void show_time_tm(char *msg){
  Serial.print(msg);
  Serial.print (F(" - "));
  //Serial.print(tm.Day, DEC);
  //Serial.print(F(":"));
  //Serial.print(tm.Month, DEC);
  //Serial.print(F(":"));
  //Serial.print(tm.Year+2000, DEC);
  //Serial.print(F("  "));
  Serial.print(tm.Hour, DEC);
  Serial.print(F(":"));
  Serial.print(tm.Minute, DEC);
  Serial.print(F(":"));
  Serial.print(tm.Second, DEC);
  Serial.println();
}

// Sweep meter over full range "count" times
void sweepMeters(int count){
  int value;
  int i;
  
  for(i=0;i<count;i++){
    
    for(value = MIN_ANALOG; value <=MAX_ANALOG; value++){
      analogWrite(METER1,value);
      analogWrite(METER2,value);
      analogWrite(METER3,value);
      delay(5);
    }
    for(value = MAX_ANALOG; value > MIN_ANALOG; value--){
      analogWrite(METER1,value);
      analogWrite(METER2,value);
      analogWrite(METER3,value);
      delay(5);
    }
  }
  analogWrite(METER1,0);
  analogWrite(METER2,0);
  analogWrite(METER3,0); 
}

/* 
  Adjust meter offset
  msg1 - Name of meter (Hour or Minutes)
  msg2 - Name of postion -- oclock for hours or minutes for Minutes
  meter - meter to adjust
  mpos - non adjusted postion of meter in time units (hours or minutes
  
  Adjust msg1 meter to mpos msg2 using knob
*/
  
void adjustMeter(char *msg1, char *msg2, int meter, int mpos) {
  int adjust = 0;
  int start;
  char newPos;
  int oldEncoderValue;
  int basePosition;
  
  // convert time units to meter position 0-255
  if(meter == HOURS)
      basePosition = map(mpos*60,0,720,0,255);
  else if (meter == MINUTES)
      basePosition = map(mpos,0,59,0,255);
   else{
       Serial.println(F("AdjustMeter -- Panic should never happen"));
       while(1);
   }
   
  Serial.print(F("Adjust "));
  Serial.print(msg1);
  Serial.print(F(" meter to "));
  Serial.print(mpos);
  Serial.print(F(" "));
  Serial.print(msg2);
  Serial.println(F(" position using knob on rear of clock"));
  Serial.println(F("type spacebar <return> when set"));
 
  start=-myEnc.read();
  analogWrite(meter,basePosition);
  
  // Adjust offset until serial character is received
  while(Serial.available())
      Serial.read();
  while(Serial.available() == 0){
    newPos = -myEnc.read();
    if (newPos != oldEncoderValue) { // encoder was turned
      oldEncoderValue = newPos;
      analogWrite(meter,basePosition+(newPos-start));
    }
    delay(5);
  }
  
  //Update offset storage
  setOffset(meter,mpos,(newPos-start));
  Serial.print(F("Offset for "));
  Serial.print(mpos);
  Serial.print(F(" "));
  Serial.print(msg2);
  Serial.print(F(" is "));
  Serial.println((int)getOffset(meter,mpos));
  Serial.println(F(""));  
}



void setup () {
  int i;

  Serial.begin(115200);
  Serial.print(F("Wicked Device Angular Clock "));
  Serial.println(VERSION);
  Serial.println("");
  Serial.println(F("Type any key and <return> to enter meter adjustment mode\n"));
 
  pinMode(RED,OUTPUT);
  pinMode(GREEN,OUTPUT);

  for(int i=0; i < LED_STARTUP_FLASHES; i++){ // Flash Leds
    digitalWrite(RED,HIGH);
    digitalWrite(GREEN,LOW);
    delay(FLASH_DELAY);
    digitalWrite(RED,LOW);
    digitalWrite(GREEN,HIGH);
    delay(FLASH_DELAY);
  }
  digitalWrite(RED,LOW); // Turn off leds
  digitalWrite(GREEN,LOW);
  
  sweepMeters(1);
  
  pinMode(ENCA,INPUT);
  pinMode(ENCB,INPUT);
  digitalWrite(ENCA,HIGH); // set pullups
  digitalWrite(ENCB,HIGH);

  Wire.begin();
  
  delay(SECONDS_1);
  
  rtc.getDateTime(&tm.Second,&tm.Minute,&tm.Hour,&tm.Wday,&tm.Day,&tm.Month,&tm.Year);
  
// If RTC does not have a valid time (year == 1) or TimeGood Flag is false, because reset was hit while time was beening adjusted
// Restore RTC to time saved in EEPROM. This will either be the time before adjustment started or time store at board setup
  if(!readTimeGood() || tm.Year==1){ 
    restoreTime();
    rtc.setDateTime(tm.Second,tm.Minute,tm.Hour,1,tm.Day,tm.Month,tm.Year);
    setTimeGood(true);
  }
  
  clearOffsets();
  
  // If a serial character received during setup djust offsets for hours 1 through 11
  if(Serial.available()){
    for(i=1; i<=11; i++)
      adjustMeter("Hours","oclock",HOURS,i);
      
    // and adjust minutes at 15,30, and 45 position
    adjustMeter("Minutes","minutes",MINUTES,15);
    adjustMeter("Minutes","minutes",MINUTES,30);
    adjustMeter("Minutes","minutes",MINUTES,45);
  }
  show_time_tm("Setup Complete");
}


int encoderValue = 0;
int oldEncoderValue = 0;
int oldSec = 0;
int lastSec = 0;
long adjustStart=0;
#define ADJUST_WAIT 10000

void loop () {
  static time_t adjustTime;
  
  long newPos = myEnc.read();
  if (newPos != encoderValue) { // encoder was turned
    encoderValue = newPos;
    encoderValue = -encoderValue; // flip encoder sign
  }   
  
  rtc.getDateTime(&tm.Second,&tm.Minute,&tm.Hour,&tm.Wday,&tm.Day,&tm.Month,&tm.Year);
  
    
  if(tm.Second != oldSec){ // only do stuff once per second
    //show_time_tm("New Second");

    if(encoderValue != oldEncoderValue){
      if(adjustStart == 0){ // Is this the start of a time adjustment
        saveTime();         // If so save current RTC time in EEPROM 
        setTimeGood(false); // Indicate current RTC time is no longer good
      }
      
      adjustStart = millis();
      //Serial.print(F("Encoder non zero ="));
      //Serial.println(encoderValue);
      if(encoderValue > oldEncoderValue){
        digitalWrite(GREEN,HIGH);
        digitalWrite(RED,LOW); 
      }
      else{
         digitalWrite(RED,HIGH);
         digitalWrite(GREEN,LOW);
      }
      adjustTime = makeTime(tm); // Convert current tm structure into seconds since epoch
      lastSec = tm.Second; 
      //Serial.print(adjustTime);
      adjustTime = adjustTime + (encoderValue - oldEncoderValue) * 15;
      oldEncoderValue = encoderValue;
      //Serial.print(F(" - "));
      //Serial.println(adjustTime);
      breakTime(adjustTime,tm);
      tm.Second = lastSec;
      show_time_tm("After adjust");
      rtc.setDateTime(tm.Second,tm.Minute,tm.Hour,1,tm.Day,tm.Month,tm.Year);
   }
   else{
      digitalWrite(GREEN,LOW);
      digitalWrite(RED,LOW);
   }

   if((adjustStart != 0) && ((millis()-adjustStart) > ADJUST_WAIT)){
      setTimeGood(true);
      adjustStart=0;
   }
   
   show_time_tm("Set Meters");
 
// do not show seconds if time adjustment in progress, 
// seconds bounce around too much
   if(encoderValue == oldEncoderValue){
       setMeter(SECONDS,map(tm.Second,0,59,0,255));
   }
 
       
   setMeter(MINUTES,map(tm.Minute,0,59,0,255)+getOffset(MINUTES,tm.Minute));

  
     int temp_hours;
     temp_hours =  tm.Hour;
     if(temp_hours >  11)
       temp_hours = temp_hours - 12;
     setMeter(HOURS,map(temp_hours*60+(tm.Minute/2),0,720,0,255)+getOffset(HOURS,temp_hours));


    //Serial.println(F("--- End of Meter update loop"));
    oldSec=tm.Second;
  }  
}

/* 
  when returning a meter to 0, if deflection is >25%, motion is ugly and loud 
  so keep track of position of each meter and if being set to 0 and 
  current defelection is >25% move meter down to 0 slowly
*/

#define MINSOFT 64    // 25% anything > let down slowly
#define METER_DEC 5   //step amount to move meter down
#define METER_WAIT 10 // time (ms) between each down step

void setMeter(int meter, int value){
  int meterTemp;
  
  //Serial.print(F("Set Meter "));
  //Serial.print(meter);
  //Serial.print(" to ");
  //Serial.println(value); 
  
  if((value == 0) and (oldMeterValue[meter] >= MINSOFT)){
    for(meterTemp=oldMeterValue[meter]; meterTemp > 0; meterTemp -= METER_DEC){
      analogWrite(meter,meterTemp);
      delay(METER_WAIT);
    }
  }
  
  if(value > 255){ // this can happen because of something wonky during calibration?
    analogWrite(meter,255);
  }
  else{
    analogWrite(meter,value);
  }
  
  oldMeterValue[meter] = value;
}


// EEPROM Memory Locations used to store last know good time
#define YEAR_MEM 10
#define MONTH_MEM 11
#define DAY_MEM 12
#define WDAY_MEM 13
#define HOUR_MEM 14
#define MINUTE_MEM 15
#define SECOND_MEM 16

// Flag indicating EEPROM time is valid
#define TIME_GOOD 20

// Memory locations used to hold meter offset adjustments
#define METER_OFFSETS 30 //next 16 locations   0-11 hours, 0,15,30,45 minutes
#define OFFSETS_USED 16

// Flag indicating meter offsets have been initialized
#define OFFSETS_GOOD 22

/*
  EEPROM routines to handle meter offsets
  Offsets are used for beter needle postioning on Hour and Minute clock faces
*/
 
void clearOffsets(){
  int i;
  
  if(EEPROM.read(OFFSETS_GOOD) == 0x55)
     return;
  
  for(i=0;i<OFFSETS_USED;i++)
    EEPROM.write(METER_OFFSETS+i,0);
  
  EEPROM.write(OFFSETS_GOOD,0x55);
}

int mapMinsToOffset(int mins){
  int offset;
   if(mins >= 45)
       offset = 15;
   else if (mins >= 30)
       offset = 14; 
   else if (mins >= 15)
       offset = 12;
   else
       offset = 0;
    
    return(offset);
}
    


void setOffset(int meter, int pos, char offset){
  if(meter == HOURS){
    EEPROM.write(METER_OFFSETS+pos,offset);
  }
  else{ // minutes
    EEPROM.write(METER_OFFSETS+mapMinsToOffset(pos),offset);
  }    
        
}

char getOffset(int meter, int pos){
  int retValue;
  
  if(meter == HOURS){
      retValue = EEPROM.read(METER_OFFSETS+pos);
  }
  else{ // meter == MINUTES
    retValue = EEPROM.read(METER_OFFSETS+mapMinsToOffset(pos));
  }
  if(EEPROM.read(OFFSETS_GOOD) != 0x55)
     retValue=0;
  return(retValue);
}


/*
  EEPROM Routines used to hold time during adjustment process
  When Adjustment starts current time is stored in EEPROM
  If processor is reset within 10 seconds of time adjustment clock
  will restore time from EEPROM and save to RTC
*/

void setTimeGood(boolean state){
  EEPROM.write(TIME_GOOD,state);
}

boolean readTimeGood(){
  return(EEPROM.read(TIME_GOOD));
}

// Save time in current tm structure to EEPROM
void saveTime(){
  EEPROM.write(YEAR_MEM,tm.Year-30);
  EEPROM.write(MONTH_MEM,tm.Month);
  EEPROM.write(DAY_MEM,tm.Day);
  EEPROM.write(DAY_MEM,tm.Wday);
  EEPROM.write(HOUR_MEM,tm.Hour);
  EEPROM.write(MINUTE_MEM,tm.Minute);
  EEPROM.write(SECOND_MEM,tm.Second);
  
}

// Restore time from EEPROM into tm structure
void restoreTime(){
  tm.Year=EEPROM.read(YEAR_MEM);
  tm.Month=EEPROM.read(MONTH_MEM);
  tm.Day=EEPROM.read(DAY_MEM);
  tm.Wday=EEPROM.read(WDAY_MEM);
  tm.Hour=EEPROM.read(HOUR_MEM);
  tm.Minute=EEPROM.read(MINUTE_MEM);
  tm.Second=EEPROM.read(SECOND_MEM);
}

