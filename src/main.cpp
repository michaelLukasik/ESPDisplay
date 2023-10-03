#include "BluetoothSerial.h"
#include "ELMduino.h" // Reminder: debugMode is OFF!
#include "bunArraysColor.h"
#include <Arduino_GFX_Library.h> // For the love of god cull this library
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>


BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
#define DEBUG_PORT Serial

/// Make sure these connections are correct ///
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_MISO   29
#define TFT_CS     15
#define TFT_DC     4
#define TFT_RESET  2
//#define I2C_SCL 22  Commented just because these happen to be defaults anyways
//#define I2c_SDA 21
const int ADXL345 = 0x53;

/// Display Settings 
// Display = ILI9341  ---> https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.h
// Dimensions = 240px L  x 320px H
Arduino_ESP32SPI bus = Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_ILI9341 display = Arduino_ILI9341(&bus, TFT_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

ELM327 myELM327;

// Counter for bunny images :0) //
int bunFrame = 0;

uint16_t* buns[] = {
    bun1, bun2, bun3, bun4, bun5, bun6, bun7, bun8, bun9, bun10, 
};

TaskHandle_t cycleScreen; 
uint8_t address[6] = { 0x66, 0x1E, 0x32, 0xF8, 0xC3, 0xA1 };

float X_out, Y_out, Z_out;  // Outputs from ADXL345 

uint32_t rpm  = 0;
float oilTemp = 0;

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

//task1: Cycle through Screen Updates
void cycleScreenCode ( void *pvParameters ){
for(;;){


  /* Get a new sensor event */ 
  sensors_event_t accelEvent;  
  accel.getEvent(&accelEvent);
    
  if (accelEvent.acceleration.x > AccelMaxX) AccelMaxX = accelEvent.acceleration.x; 
  if (accelEvent.acceleration.y > AccelMaxY) AccelMaxY = accelEvent.acceleration.y;
  if (accelEvent.acceleration.z > AccelMaxZ) AccelMaxZ = accelEvent.acceleration.z;
  
  Serial.print("Accel: "); 
  Serial.print((accelEvent.acceleration.x - accelXOffset) / 9.81); 
  Serial.print("  ");
  Serial.print((accelEvent.acceleration.y - accelYOffset) / 9.81); 
  Serial.print("  "); 
  Serial.print(accelEvent.acceleration.z / 9.81); 
  Serial.println();
  Serial.print("Accel Maximums: "); 
  Serial.print(AccelMaxX); 
  Serial.print("  ");
  Serial.print(AccelMaxY); 
  Serial.print("  "); 
  Serial.print(AccelMaxZ); 
  Serial.println();

  if (bunFrame > 9) {bunFrame = 0;}
  // Bunny Display
  display.draw16bitBeRGBBitmap(0, 273, buns[bunFrame], 64, 47);
  if (MPH <= 25) { delay(250);}
  else if (MPH > 25 and MPH <= 40) { delay(150);}
  else if (MPH > 40 and MPH <= 65) { delay(50);}
  else if (MPH > 65) { delay(10);}
  else { delay(10); }
  bunFrame +=1;
  
  // Update Mph next to the bun
  if(cachedMPH != MPH){
    display.fillRect(70, 273 , 170 , 30, BLACK);
    display.setCursor(75,282);
    display.setTextSize(3);
    display.setTextColor(GREEN);
    display.print(F("MPH: "));
    display.print(int(MPH));
    cachedMPH = MPH;
  }
  // Update pedal position value, bar (X=5,Y=50, H=200, W=30) fills from bottom with Green
  if(cachedRelativeThrottle != relativeThrottle){
    display.fillRect(5, 50 , 30 , 2*(100 - relativeThrottle), BLACK);
    display.fillRect(5, 50 + 2*(100 - relativeThrottle), 30 , (200*relativeThrottle)/100 , GREEN); // Draw Rectangle as a percentage of how pressed the pedal is
    display.drawRect(5, 50 , 30 , 200, RED);
    display.setCursor(15,140);
    display.setTextSize(1);
    display.setTextColor(PINK);
    display.print(int(relativeThrottle));
    cachedRelativeThrottle = relativeThrottle;
    }
  // Update Fuel Rate, this needs some testing
  if(cachedEFR != EFR){
  display.fillRect(40, 233 , 200 , 30, BLACK);
  display.setCursor(45,242);
  display.setTextSize(3);
  display.setTextColor(LIGHTGREY);
  display.print(F("EFR: "));
  display.print(EFR,1);
  display.setTextSize(1);
  display.print(F("l/h"));  
  cachedEFR = EFR;
    }
  } // End of core 0 loop code
}

void setup()
{
  DEBUG_PORT.begin(115200);

  /// Set up Display ////
  display.begin();
  display.fillScreen(BLACK);
  display.setCursor(20, 20);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print(F("Connecting!!!"));

  // Set up pedal bar
  display.setTextSize(1);
  display.setTextColor(PINK);
  display.setCursor(15,135);
  display.print(int(relativeThrottle));
  display.drawRect(5, 50 , 30 , 200, RED);
  display.setCursor(5,255);
  display.print(F("Thr.")); 

  //Set up EFR area 
  //display.drawRect(40, 233 , 200 , 32, RED);
  display.fillRect(40, 233 , 200 , 30, BLACK);
  display.setCursor(45,242);
  display.setTextSize(3);
  display.setTextColor(LIGHTGREY);
  display.print(F("EFR: "));
  display.print(EFR,1);
  display.setTextSize(1);
  display.print(F("l/h"));  

  /* Initialise the sensor */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
  }
  accel.setRange( ADXL345_RANGE_4_G);

  xTaskCreatePinnedToCore(
    cycleScreenCode, /* Function to implement the task */
    "cycleScreen", /* Name of the task */
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &cycleScreen,  /* Task handle. */
    0); /* Core where the task should run */ 

  ELM_PORT.begin("ml.ESP32",true);
  
  if (!ELM_PORT.connect(address)){
    DEBUG_PORT.println(F("Couldn't connect to OBD scanner - Phase 1"));
    display.fillRect(0, 0 , 240 , 40, BLACK);
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(F("Not connected (Code 1)"));
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(F("Reset the module dumbass"));
    display.setCursor(70,282);
    display.setTextSize(3);
    display.setTextColor(RED);
    display.print(F("MPH: N/A "));
    while(1);
  }

  if (!myELM327.begin(ELM_PORT, 0, 1000)){
    Serial.println(F("Couldn't connect to OBD scanner - Phase 2"));
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(F("Not connected (Code 2)"));
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(F("Reset the module dumbass"));
    while (1);
  }
  display.fillRect(0, 0 , 240 , 40, BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(NAVY);
  display.print(F("BT Connected!"));
}


void loop()
{
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

