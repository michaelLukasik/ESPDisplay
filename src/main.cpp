#include "BluetoothSerial.h" // Huge library, maybe swap to wired connection
#include "ELMduino.h" // Reminder: debugMode is OFF!
#include "bunArraysX.h" // Using XBM Version for now
#include "accelTracker.h" // BMP for acceleration tracking sprites
#include "screenPositions.h" // Definitions for all the screen positons, minus some minor tweeks
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

ELM327 myELM327;
uint8_t address[6] = { 0x66, 0x1E, 0x32, 0xF8, 0xC3, 0xA1 }; // ELM Address
TFT_eSPI display = TFT_eSPI();
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
TFT_eSprite accelTracker = TFT_eSprite(&display);
TFT_eSprite accelCircles = TFT_eSprite(&display);
TaskHandle_t cycleScreen; 
BluetoothSerial SerialBT;

const int ADXL345 = 0x53; // Default address for the accelerometer
const int gee = 9.81; // For scaling G's in accelerometer reading

int bunFrame = 0; // Counter for bunny images
const unsigned char* buns[] = {
    bun1, bun2, bun3, bun4, bun5, bun6, bun7, bun8, bun9, bun10, 
};

volatile float relativeThrottle = 0;
volatile float cachedRelativeThrottle = -1;
float tempRelativeThrottle = 0;

volatile float MPH = 0;
volatile float cachedMPH = -1;
float tempMPH = 0;

volatile float EFR = 0; // Engine Fuel Rate
volatile float cachedEFR = -1;
float tempEFR = 0;

float accelXOffset = 0.35;
float accelYOffset = 0.35;
float AccelMaxX = 0;
float AccelMaxY = 0;
float AccelMaxZ = 0;

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

  /* Get a new sensor event */ 
  sensors_event_t accelEvent;  
  accel.getEvent(&accelEvent);
    
  if (accelEvent.acceleration.x > AccelMaxX) AccelMaxX = accelEvent.acceleration.x; // Currently unused, but will add to display later.
  if (accelEvent.acceleration.y > AccelMaxY) AccelMaxY = accelEvent.acceleration.y; //
  if (accelEvent.acceleration.z > AccelMaxZ) AccelMaxZ = accelEvent.acceleration.z; //
  
  float accelTrackerXOff = ACCEL_CIRCLE_INNER_RADIUS*convertAccelMag(accelEvent.acceleration.x,accelEvent.acceleration.y)*cos(convertAccelArg(accelEvent.acceleration.x,accelEvent.acceleration.y));
  float accelTrackerYOff = ACCEL_CIRCLE_INNER_RADIUS*convertAccelMag(accelEvent.acceleration.x,accelEvent.acceleration.y)*sin(convertAccelArg(accelEvent.acceleration.x,accelEvent.acceleration.y));

  // Note: The X and Y offsets are such that they match the breadboard facing "forwards in the car". This makes the offsets look reversed here, but it is what it is.
  accelCircles.pushImage(0,0,ACCEL_CIRCLE_SPRITE_W, ACCEL_CIRCLE_SPRITE_H, accelCircles140);
  accelTracker.pushToSprite(&accelCircles , (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE_W/2) + accelTrackerYOff, 
                            (ACCEL_CIRCLE_SPRITE_W/2 - ACCEL_TRACKER_SPRITE_W/2) + accelTrackerXOff , TFT_BLACK);
  accelCircles.pushSprite(ACCEL_CIRCLE_SPRITE_POS_X, ACCEL_CIRCLE_SPRITE_POS_Y);

  if (bunFrame > 9) {bunFrame = 0;}
  // Bunny Display
  display.drawXBitmap(BUNNY_POS_X, BUNNY_POS_Y, buns[bunFrame], BUNNY_W, BUNNY_H , TFT_WHITE);
  if (MPH <= 25) { frameInterval = 250;}
  else if (MPH > 25 and MPH <= 40) { frameInterval = 150;}
  else if (MPH > 40 and MPH <= 65) { frameInterval = 50;}
  else if (MPH > 65) { frameInterval = 20;}
  if(millis() - time_now > frameInterval){
    display.drawXBitmap(BUNNY_POS_X, BUNNY_POS_Y, buns[bunFrame], BUNNY_W, BUNNY_H , TFT_BLACK); // Draw over old bitmap with background color to "erase" and prime for next image
    bunFrame +=1;
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
  // Update pedal position value, bar (X=5,Y=50, H=200, W=30) fills from bottom with Green
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

  // Set Up Acceleration Circle
  accelCircles.createSprite(ACCEL_CIRCLE_SPRITE_W ,ACCEL_CIRCLE_SPRITE_H );
  accelCircles.pushImage(0,0,ACCEL_CIRCLE_SPRITE_W ,ACCEL_CIRCLE_SPRITE_H , accelCircles140);
  accelTracker.createSprite(ACCEL_TRACKER_SPRITE_W ,ACCEL_TRACKER_SPRITE_H);
  accelTracker.pushImage(0,0,ACCEL_TRACKER_SPRITE_W, ACCEL_TRACKER_SPRITE_H, accelTracker16);


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

  if(BTLock == 0){
    display.fillScreen(TFT_BLACK);
    display.fillRect(BT_CONNECTED_RECT_POS_X, BT_CONNECTED_RECT_POS_Y , BT_CONNECTED_RECT_W , BT_CONNECTED_RECT_H, TFT_BLACK);
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(TFT_NAVY);
    display.print(F("BT Connected!"));
  }

  xTaskCreatePinnedToCore(
    cycleScreenCode, /* Function to implement the task */
    "cycleScreen", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &cycleScreen,  /* Task handle. */
    0); /* Core where the task should run */

  while(BTLock == 1){/* Crashes if it actually enters the loop, keeping this lock condition atleast allows you to bugfix for now*/}
  
  Serial.println(F("Exiting Setup ")); 
}

void loop() // Use Core 1 to Query the ELM Device
{

  Serial.println(F("Entering Loop"));

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


  // ==================== Speed Stuff ===================== //
  tempMPH = myELM327.mph();

  if (myELM327.nb_rx_state == ELM_SUCCESS){
    MPH = tempMPH;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
    Serial.println(F("nb_rx_state != ELM Getting Msg "));
  }

// ==================== Oil Temp Stuff ===================== //
//  float tempOilTemp = myELM327.oilTemp();
// 
//  if (myELM327.nb_rx_state == ELM_SUCCESS){
//    oilTemp = (float)tempOilTemp;
//    Serial.print("Oil Temp: "); Serial.println(oilTemp);
//  }
//  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
//    //myELM327.printError();
//  }

// ==================== Relative Throttle Stuff ===================== //
  tempRelativeThrottle = myELM327.relativeThrottle();
  
  if (myELM327.nb_rx_state == ELM_SUCCESS){
   relativeThrottle = tempRelativeThrottle;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
    Serial.println(F("nb_rx_state != ELM Getting Msg "));
  }


// ==================== Fuel Rate Stuff ===================== //
  tempEFR = myELM327.fuelRate();
  
  if (myELM327.nb_rx_state == ELM_SUCCESS){
   EFR = tempEFR;
   Serial.println(tempEFR);
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
    Serial.println(F("EFR ERROR "));
  }
}

