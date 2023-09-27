#include "BluetoothSerial.h"
#include "ELMduino.h" // Reminder: debugMode is OFF!
#include "bunArrays.h"
#include <Arduino_GFX_Library.h>

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

 // 1064181 bytes
/// Display Settings 
// Display = ILI9341  ---> https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.h
// Dimensions = 240px L  x 320px H
Arduino_ESP32SPI bus = Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_ILI9341 display = Arduino_ILI9341(&bus, TFT_RESET);

ELM327 myELM327;


// Counter for bunny images :0) //
uint8_t bunFrame = 0;

const uint8_t *buns[] = {
    bun1, bun2, bun3, bun4, bun5, bun6, bun7, bun8, bun9, bun10, 
};

TaskHandle_t cycleScreen;
String MACadd = "66:1E:32:F8:C3:A1"; 
uint8_t address[6] = { 0x66, 0x1E, 0x32, 0xF8, 0xC3, 0xA1 };


uint32_t rpm  = 0;
float oilTemp = 0;

volatile float relativePedalPosition = 0;
float tempRelativePedalPosition = 0;

volatile float MPH = 0;
float tempMPH = 0;


//task1: Cycle through bunny images
void cycleScreenCode ( void *pvParameters ){
for(;;){
  if (bunFrame > 9) {bunFrame = 0;}
  // Bunny Display
  display.drawBitmap(0, 273, buns[bunFrame], 64, 47, WHITE, BLACK);
  if (MPH <= 25) { delay(250);}
  else if (MPH > 25 and MPH <= 40) { delay(150);}
  else if (MPH > 40 and MPH <= 65) { delay(50);}
  else if (MPH > 65) { delay(10);}
  else { delay(10); }
  bunFrame +=1;
  
  // Update Mph next to the bun
  display.fillRect(70, 273 , 170 , 30, BLACK);
  display.setCursor(70,282);
  display.setTextSize(3);
  display.setTextColor(GREEN);
  display.print(F("MPH: "));
  display.print(int(MPH));

  // Update pedal position value
  display.fillRect(70, 233 , 170 , 30, BLACK);
  display.setCursor(70,242);
  display.setTextSize(2);
  display.setTextColor(GREEN);
  display.print(F("Pedal: "));
  display.print(int(relativePedalPosition));

  } 
}

void setup()
{
  /// Set up Display ////
  display.begin();
  display.fillScreen(BLACK);
  display.setCursor(20, 20);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("Connecting!!!");

  xTaskCreatePinnedToCore(
      cycleScreenCode, /* Function to implement the task */
      "cycleScreen", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &cycleScreen,  /* Task handle. */
      0); /* Core where the task should run */ 

  DEBUG_PORT.begin(115200);
  ELM_PORT.begin("ml.ESP32",true );
  
  if (!ELM_PORT.connect(address))
  {
    DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
    display.fillRect(0, 0 , 240 , 40, BLACK);
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("Not connected (Code 1)");
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("Reset the module dumbass");
    display.setCursor(70,282);
    display.setTextSize(3);
    display.setTextColor(RED);
    display.print("MPH: N/A ");
    while(1);
  }

  if (!myELM327.begin(ELM_PORT, 0, 1000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    display.setCursor(20, 40);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("Not connected (Code 2)");
    display.setCursor(20, 80);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("Reset the module dumbass");
    while (1);
  }
  display.fillRect(0, 0 , 240 , 40, BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(NAVY);
  display.print("BT Connected!");
}


void loop()
{
  // ==================== Engine RPM Stuff ===================== //
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

// ==================== Relative Pedal Position Stuff ===================== //
  tempRelativePedalPosition = myELM327.relativePedalPos();
  
  if (myELM327.nb_rx_state == ELM_SUCCESS){
   relativePedalPosition = tempRelativePedalPosition;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG){
    Serial.println(F("nb_rx_state != ELM Getting Msg "));
  }
}