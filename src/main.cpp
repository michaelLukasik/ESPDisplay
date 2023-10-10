#include "BluetoothSerial.h" // Huge library, maybe swap to wired connection
#include "ELMduino.h" // Reminder: debugMode is OFF!
#include "bunnyBitmaps/bunArraysX.h" // Using XBM Version for now
#include "screenPositions.h" // Definitions for all the screen positons, minus some minor tweaks
#include "sprites.h" 
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

ELM327 myELM327;
uint8_t address[6] = { 0x66, 0x1E, 0x32, 0xF8, 0xC3, 0xA1 }; // ELM Address
TFT_eSPI display = TFT_eSPI();
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
TFT_eSprite accelTracker_0 = TFT_eSprite(&display);
TFT_eSprite accelTracker_1 = TFT_eSprite(&display); // Previous-1
TFT_eSprite accelTracker_2 = TFT_eSprite(&display); // Previous-2
TFT_eSprite accelTracker_3 = TFT_eSprite(&display); // Previous-3
TFT_eSprite accelCircles = TFT_eSprite(&display);
TFT_eSprite batterySprite = TFT_eSprite(&display);
TFT_eSprite fuelSprite = TFT_eSprite(&display);
TFT_eSprite oilSprite = TFT_eSprite(&display);
TaskHandle_t cycleScreen; 
BluetoothSerial SerialBT;

const int ADXL345 = 0x53; // Default address for the accelerometer
const int gee = 9.81; // For scaling G's in accelerometer reading

int bunFrame = 0; // Counter for bunny images
const unsigned char* buns[] = {
    bun1, bun2, bun3, bun4, bun5, bun6, bun7, bun8, bun9, bun10, 
};

typedef enum { state_mph, state_throttle, state_oil,
                state_voltage, state_fuel, state_efr} obd_pid_states;
obd_pid_states obd_state = state_mph;

volatile float relativeThrottle = 0;
volatile float cachedRelativeThrottle = -1;
float tempRelativeThrottle = 0;

volatile float MPH = 0;
volatile float cachedMPH = -1;
float tempMPH = 0;

volatile float EFR = 0; // Engine Fuel Rate
volatile float cachedEFR = -1;
float tempEFR = 0;

volatile float batVoltage = 0; 
volatile float cachedBatVoltage = -1;
float tempBatVoltage = 0;

volatile float fuelLevel = 0.;
volatile float cachedFuelLevel = -1.;
float tempFuelLevel = 0.;

volatile float oilTemp = 0.;
volatile float cachedOilTemp = -1.;
float tempOilTemp = 0.;

float accelXOffset = 0.35;
float accelYOffset = 0.35;
float AccelMaxX = 0;
float AccelMaxY = 0;
float AccelMaxZ = 0;

float accelTrackerXOff_1 = 0.;
float accelTrackerYOff_1 = 0.;
float accelTrackerXOff_2 = 0.;
float accelTrackerYOff_2 = 0.;
float accelTrackerXOff_3 = 0.;
float accelTrackerYOff_3 = 0.;


int frameInterval = 50;
volatile unsigned long time_now = millis();
int BTLock = 0;

float convertAccelMag(float accelReadingX, float accelReadingY){
  if(pow(pow(accelReadingX,2) + pow(accelReadingY,2), 0.5) / gee > 2.) {
    return 2.0;
  } 
  return (pow(pow(accelReadingX,2) + pow(accelReadingY,2), 0.5)) / gee ;
}

float convertAccelArg(float accelReadingX, float accelReadingY){
  return atan2(accelReadingY,accelReadingX);
}

void cycleScreenCode ( void *pvParameters ){ //Cycle through Screen Updates on Core 0
for(;;){

  sensors_event_t accelEvent; // Get a new ADXL345 Event
  if(accel.getEvent(&accelEvent)){ 
    if (accelEvent.acceleration.x > AccelMaxX) AccelMaxX = accelEvent.acceleration.x; // Currently unused, but will add to display later.
    if (accelEvent.acceleration.y > AccelMaxY) AccelMaxY = accelEvent.acceleration.y; // Currently unused, but will add to display later.
    if (accelEvent.acceleration.z > AccelMaxZ) AccelMaxZ = accelEvent.acceleration.z; // Currently unused, but will add to display later.
     
    float accelTrackerXOff = ACCEL_CIRCLE_INNER_RADIUS*convertAccelMag(accelEvent.acceleration.x,accelEvent.acceleration.y)*cos(convertAccelArg(accelEvent.acceleration.x,accelEvent.acceleration.y));
    float accelTrackerYOff = ACCEL_CIRCLE_INNER_RADIUS*convertAccelMag(accelEvent.acceleration.x,accelEvent.acceleration.y)*sin(convertAccelArg(accelEvent.acceleration.x,accelEvent.acceleration.y));

    // Note: The X and Y offsets are such that they match the breadboard facing "forwards in the car". This makes the offsets look reversed here, but it is what it is.
    accelCircles.pushImage(0,0,ACCEL_CIRCLE_SPRITE_W, ACCEL_CIRCLE_SPRITE_H, accelCircles140);
    
    // Add Sprites smallest to largest to overwrite accordingly 
    accelTracker_3.pushToSprite(&accelCircles , (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE4_W/2) + accelTrackerYOff_3, 
                              (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE4_W/2) + accelTrackerXOff_3 , TFT_BLACK);
    accelTracker_2.pushToSprite(&accelCircles , (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE6_W/2) + accelTrackerYOff_2, 
                              (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE6_W/2) + accelTrackerXOff_2 , TFT_BLACK);
    accelTracker_1.pushToSprite(&accelCircles , (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE10_W/2) + accelTrackerYOff_1, 
                              (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE10_W/2) + accelTrackerXOff_1 , TFT_BLACK);
    accelTracker_0.pushToSprite(&accelCircles , (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE16_W/2) + accelTrackerYOff, 
                              (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE16_W/2) + accelTrackerXOff , TFT_BLACK);
    
    accelCircles.pushSprite(ACCEL_CIRCLE_SPRITE_POS_X, ACCEL_CIRCLE_SPRITE_POS_Y);
  
    // Update the locations of the sprites for next round
    accelTrackerXOff_3 = accelTrackerXOff_2; 
    accelTrackerYOff_3 = accelTrackerYOff_2;
    accelTrackerXOff_2 = accelTrackerXOff_1; 
    accelTrackerYOff_2 = accelTrackerYOff_1;
    accelTrackerXOff_1 = accelTrackerXOff;
    accelTrackerYOff_1 = accelTrackerYOff;
  }

  // Bunny Display
  if (MPH <= MPH_SLOW) { frameInterval = 250;}
  else if (MPH > MPH_SLOW and MPH <= MPH_MED) { frameInterval = 150;}
  else if (MPH > MPH_MED and MPH <= MPH_FAST) { frameInterval = 50;}
  else if (MPH > MPH_FAST) { frameInterval = 20;}
  if(millis() - time_now > frameInterval){
    display.drawXBitmap(BUNNY_POS_X, BUNNY_POS_Y, buns[bunFrame % 10], BUNNY_W, BUNNY_H , TFT_BLACK); // Draw over old bitmap with background color to "erase" and prime for next image
    bunFrame +=1;
    display.drawXBitmap(BUNNY_POS_X, BUNNY_POS_Y, buns[bunFrame % 10], BUNNY_W, BUNNY_H , TFT_WHITE);
    time_now = millis();
  }

  // Update Mph next to the bun
  if(cachedMPH != MPH){
    display.fillRect(MPH_RECT_POS_X, MPH_RECT_POS_Y , MPH_RECT_W , MPH_RECT_H, TFT_BLACK);
    display.setCursor(MPH_RECT_POS_X + 10, MPH_RECT_POS_Y + 9);
    display.setTextSize(3);
    display.setTextColor(TFT_GREEN);
    display.print(F("MPH: "));
    display.print(int(MPH));
    cachedMPH = MPH;
    }
  // Update pedal position value, bar fills from bottom with Green
  if(cachedRelativeThrottle != relativeThrottle){
    display.fillRect(REL_THROT_POS_X, REL_THROT_POS_Y , REL_THROT_W , REL_THROT_SCALE*(100 - relativeThrottle), TFT_BLACK);
    display.fillRect(REL_THROT_POS_X, REL_THROT_POS_Y + REL_THROT_SCALE*(100 - relativeThrottle), REL_THROT_W , (REL_THROT_SCALE*relativeThrottle) , TFT_GREEN); // Draw Rectangle as a percentage of how pressed the pedal is
    display.drawRect(REL_THROT_POS_X, REL_THROT_POS_Y , REL_THROT_W , REL_THROT_H, TFT_RED);
    display.setCursor(REL_THROT_POS_X + 10, REL_THROT_POS_Y+ 90);
    display.setTextSize(1);
    display.setTextColor(TFT_PINK);
    display.print(int(relativeThrottle));
    cachedRelativeThrottle = relativeThrottle;
    }
  // Update Fuel Rate, this needs some testing
  if(cachedEFR != EFR){
    display.fillRect(EFT_RECT_POS_X, EFT_RECT_POS_Y , EFT_RECT_W , EFT_RECT_H, TFT_BLACK);
    display.setCursor(EFT_RECT_POS_X + 5, EFT_RECT_POS_Y + 9);
    display.setTextSize(3);
    display.setTextColor(TFT_LIGHTGREY);
    display.print(F("EFR: "));
    display.print(EFR,1);
    display.setTextSize(1);
    display.print(F("l/h"));  
    cachedEFR = EFR;
    }

  if(cachedBatVoltage != batVoltage){
    display.fillRect(BAT_RECT_POS_X, BAT_RECT_POS_Y, BAT_RECT_W, BAT_RECT_H, TFT_BLACK);
    display.setCursor(BAT_RECT_POS_X + 2, BAT_RECT_POS_Y + 9);
    display.setTextSize(2);
    display.setTextColor(TFT_LIGHTGREY);
    display.print(batVoltage,1);
    display.setTextSize(1);
    display.setTextColor(TFT_LIGHTGREY);
    display.print('V');
    cachedBatVoltage = batVoltage;
    }

  if(cachedFuelLevel != fuelLevel){
    display.fillRect(FUEL_RECT_POS_X, FUEL_RECT_POS_Y, FUEL_RECT_W, FUEL_RECT_H, TFT_BLACK);
    display.setCursor(FUEL_RECT_POS_X + 2, FUEL_RECT_POS_Y + 4);
    display.setTextSize(2);
    display.setTextColor(TFT_LIGHTGREY);
    display.print(fuelLevel);
    display.setTextSize(1);
    display.setTextColor(TFT_LIGHTGREY);
    display.print('%');
    cachedFuelLevel = fuelLevel; 
    }
  
  if(cachedOilTemp !=  oilTemp){
    display.fillRect(OIL_RECT_POS_X, OIL_RECT_POS_Y, OIL_RECT_W, OIL_RECT_H, TFT_BLACK);
    display.setCursor(OIL_RECT_POS_X + 2, OIL_RECT_POS_Y + 4);
    display.setTextSize(2);
    display.setTextColor(TFT_LIGHTGREY);
    display.print(oilTemp);
    display.setTextSize(1);
    display.setTextColor(TFT_LIGHTGREY);
    display.print('o');
    cachedOilTemp = oilTemp; 
    }

  } // End of core 0 loop code
}

void setup(){

  Serial.begin(115200);
  
  /// Set up Display ////
  display.begin();
  display.fillScreen(TFT_BLACK);
  display.setCursor(20, 20);
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE);
  display.print(F("Connecting!!!"));

  // Set up pedal bar
  display.setTextSize(1);
  display.setTextColor(TFT_PINK);
  display.setCursor(REL_THROT_POS_X + 5, REL_THROT_POS_Y + 85);
  display.print(int(relativeThrottle));
  display.drawRect(REL_THROT_POS_X, REL_THROT_POS_Y , REL_THROT_W  , REL_THROT_H, TFT_RED);
  display.setCursor(REL_THROT_POS_X, REL_THROT_POS_Y + REL_THROT_H + 5);
  display.print(F("Thr.")); 

  //Set up EFR area 
  display.fillRect(EFT_RECT_POS_X, EFT_RECT_POS_Y , EFT_RECT_W , EFT_RECT_H, TFT_BLACK);
  display.setCursor(EFT_RECT_POS_X + 5, EFT_RECT_POS_Y + 9);
  display.setTextSize(3);
  display.setTextColor(TFT_LIGHTGREY);
  display.print(F("EFR: "));
  display.print(EFR,1);
  display.setTextSize(1);
  display.print(F("l/h"));  

  //Set up Oil Temp Area
  oilSprite.createSprite(OIL_SPRITE_W, OIL_SPRITE_H);
  oilSprite.setSwapBytes(true);
  oilSprite.pushImage(0,0,OIL_SPRITE_W, OIL_SPRITE_H, oil24);
  oilSprite.pushSprite(OIL_SPRITE_POS_X,OIL_RECT_POS_Y);

  // Set Up Acceleration Tracker and Background sprite (accelCircles)
  accelCircles.createSprite(ACCEL_CIRCLE_SPRITE_W ,ACCEL_CIRCLE_SPRITE_H );
  accelCircles.setSwapBytes(true); // Change Endian-ness
  accelCircles.pushImage(0,0,ACCEL_CIRCLE_SPRITE_W ,ACCEL_CIRCLE_SPRITE_H , accelCircles140);

  accelTracker_0.createSprite(ACCEL_TRACKER_SPRITE16_W ,ACCEL_TRACKER_SPRITE16_H);
  accelTracker_1.createSprite(ACCEL_TRACKER_SPRITE10_W ,ACCEL_TRACKER_SPRITE10_H);
  accelTracker_2.createSprite(ACCEL_TRACKER_SPRITE6_W ,ACCEL_TRACKER_SPRITE6_H);
  accelTracker_3.createSprite(ACCEL_TRACKER_SPRITE4_W ,ACCEL_TRACKER_SPRITE4_H);
  
  accelTracker_0.pushImage(0,0,ACCEL_TRACKER_SPRITE16_W, ACCEL_TRACKER_SPRITE16_H, accelTracker16);
  accelTracker_1.pushImage(0,0,ACCEL_TRACKER_SPRITE10_W, ACCEL_TRACKER_SPRITE10_H, accelTracker10);
  accelTracker_2.pushImage(0,0,ACCEL_TRACKER_SPRITE6_W, ACCEL_TRACKER_SPRITE6_H, accelTracker6);
  accelTracker_3.pushImage(0,0,ACCEL_TRACKER_SPRITE4_W, ACCEL_TRACKER_SPRITE4_H, accelTracker4);
  
  //Set up Battery Voltage area
  batterySprite.createSprite(BOLT_SPRITE_W, BOLT_SPRITE_H);
  batterySprite.setSwapBytes(true); // Change Endian-ness
  batterySprite.pushImage(0,0,BOLT_SPRITE_W, BOLT_SPRITE_H, bolt24);
  batterySprite.pushSprite(BOLT_SPRITE_POS_X, BOLT_SPRITE_POS_Y);

  //Set up Fuel Level area
  fuelSprite.createSprite(FUEL_SPRITE_W, FUEL_SPRITE_H);
  fuelSprite.setSwapBytes(true); // Change Endian-ness
  fuelSprite.pushImage(0, 0, FUEL_SPRITE_W, FUEL_SPRITE_H, fuel24);
  fuelSprite.pushSprite(FUEL_SPRITE_POS_X, FUEL_SPRITE_POS_Y); 

  /* Initialise the sensor */
  if(!accel.begin()){
    Serial.println(F("No ADXL345 found with default connection pins"));
    while(1);
  }
  accel.setRange(ADXL345_RANGE_2_G);

  SerialBT.begin("ml.ESP32",true);
  
  if (!SerialBT.connect(address)){ // Error finding the OBD2 Scanner
    Serial.println(F("Couldn't connect to OBD scanner - Phase 1"));
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.print(F("Not connected (Code 1)"));
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.print(F("Reset the ESP"));
    display.setCursor(70,282);
    display.setTextSize(3);
    display.setTextColor(TFT_RED);
    display.print(F("MPH: N/A "));
    BTLock = 1; // Turn on BT Lock, this is really only for debugging and should be removed soon.
    Serial.print(F("BT Lock Status: "));
    Serial.println(BTLock);
  }

  if (!myELM327.begin(SerialBT, 0, 1000)){
    Serial.println(F("Couldn't connect to OBD scanner - Phase 2"));
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.print(F("Not connected (Code 2)"));
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(TFT_WHITE);
    display.print(F("Reset the module "));
    BTLock = 1; 
  }

  if(BTLock == 0){
    display.fillScreen(TFT_BLACK); 

    oilSprite.createSprite(OIL_SPRITE_W, OIL_SPRITE_H);
    oilSprite.setSwapBytes(true); // Change Endian-ness
    oilSprite.pushImage(0, 0, OIL_SPRITE_W, OIL_SPRITE_H, oil24);
    oilSprite.pushSprite(OIL_SPRITE_POS_X, OIL_SPRITE_POS_Y); 

    batterySprite.createSprite(BOLT_SPRITE_W, BOLT_SPRITE_H);
    batterySprite.setSwapBytes(true); // Change Endian-ness of battery sprite
    batterySprite.pushImage(0,0,BOLT_SPRITE_W, BOLT_SPRITE_H, bolt24);
    batterySprite.pushSprite(BOLT_SPRITE_POS_X, BOLT_SPRITE_POS_Y);

    fuelSprite.createSprite(FUEL_SPRITE_W, FUEL_SPRITE_H);
    fuelSprite.setSwapBytes(true); // Change Endian-ness
    fuelSprite.pushImage(0, 0, FUEL_SPRITE_W, FUEL_SPRITE_H, fuel24);
    fuelSprite.pushSprite(FUEL_SPRITE_POS_X, FUEL_SPRITE_POS_Y);

    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(TFT_NAVY);
    display.print(F("BT Connected!"));
    display.setTextColor(TFT_PINK);
    display.setCursor(REL_THROT_POS_X + 1, REL_THROT_POS_Y + REL_THROT_H + 5);
    display.print(F("Thr.")); 
  }

  xTaskCreatePinnedToCore(
    cycleScreenCode, /* Function to implement the task */
    "cycleScreen", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &cycleScreen,  /* Task handle. */
    0); /* Core where the task should run */

  //while(BTLock == 1){}
  
  Serial.println(F("Exiting Setup ")); 
}

void loop() // Use Core 1 to Query the ELM Device
{
 switch (obd_state)
 {
  // ==================== Speed Stuff ===================== //
  case state_mph:
  {    
    tempMPH = myELM327.mph();
    if (myELM327.nb_rx_state == ELM_SUCCESS){
      MPH = tempMPH;
      Serial.println(MPH);
      obd_state = state_throttle;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      myELM327.printError();
      obd_state = state_throttle;
    }
    break;
  }

//================= Relative Throttle Stuff ===================== //
  case state_throttle:
  {
    tempRelativeThrottle = myELM327.relativeThrottle();
  
    if (myELM327.nb_rx_state == ELM_SUCCESS){
      relativeThrottle = tempRelativeThrottle;
      Serial.println(relativeThrottle);
      obd_state = state_voltage;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      myELM327.printError();
      obd_state = state_voltage;
    }
    break;
  }
// ==================== Battery Stuff ===================== //
  case state_voltage:
  {
    tempBatVoltage =  myELM327.batteryVoltage();

    if (myELM327.nb_rx_state == ELM_SUCCESS){
      batVoltage = tempBatVoltage;
      Serial.println(batVoltage);
      obd_state = state_fuel;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      myELM327.printError();
      obd_state = state_fuel;
    }
    break;
  }
// ==================== Fuel Level Stuff ===================== // 
  case state_fuel:
  {
    tempFuelLevel = myELM327.fuelLevel();
    
    if (myELM327.nb_rx_state == ELM_SUCCESS){
      fuelLevel = tempFuelLevel;
      Serial.print("Fuel: "); Serial.println(fuelLevel);
      obd_state = state_efr;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      myELM327.printError();
      obd_state = state_efr;
    }
    break;
 }
// ==================== Fuel Rate Stuff ===================== //
  case state_efr:
  {
    tempEFR = myELM327.fuelRate();
    if (myELM327.nb_rx_state == ELM_SUCCESS){
      EFR = tempEFR;
      Serial.print("EFR: "); Serial.println(EFR);
      obd_state = state_oil;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      Serial.println(F("EFR ERROR"));
      obd_state = state_oil;
    }
    break;
  }

// ==================== Oil Temp Stuff ===================== //
  
  case state_oil:
  {
    float tempOilTemp = myELM327.oilTemp();
    if (myELM327.nb_rx_state == ELM_SUCCESS){
      oilTemp = (float)tempOilTemp;
      Serial.print("Oil Temp: "); Serial.println(oilTemp);
      obd_state = state_mph;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
      myELM327.printError();
      obd_state = state_mph;
    }
    break;
  }

// ==================== Engine RPM Stuff ===================== //  Lets do some red line stuff
//  uint32_t tempRPM = myELM327.rpm();
///
//  if (myELM327.nb_rx_state == ELM_SUCCESS){
//   rpm = (uint32_t)tempRPM;
//    Serial.print("RPM: "); Serial.println(rpm);
//  }
//  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
//    //myELM327.printError();
//  }

 } // End switch statement
}

