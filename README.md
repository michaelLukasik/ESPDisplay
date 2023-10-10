# ESPDisplayProject to display additional OBD2 Data while diriving

ESP32 Based project to query OBD2 data through Vehicle PID's and display the results onto an OLED screen interfaced via SPI 

The project uses the [ELMduino library provided here](https://github.com/PowerBroker2/ELMduino), which uses ISO-TP protocal via quereying the Vehichle's CAN bus and reconfigures the message to be parsable on the user end. All of the work on this parsing algorithm was provided by the library's creator. 

Currently, the information is sent via Bluetooth (as that was the cheapest ELM device I could get my hands on 😎 ), and suffers from PID "misses" occasionally. A wired connection may be better in the future. 

![Display_Example1](https://github.com/michaelLukasik/ESPDisplay/assets/138163589/ca17e95f-131b-4d0a-8719-301346527a57)

Here is an example image of how the display looks, with MPH, Fuel Consumption (EFR), Throttle Position, Battery Voltage, Oil Temperature, and Accelerometer Output dispalyed. The rabbit bitmap image runs at a speed corresponding to how fast you are driving, utilizing the ESP32's Dual Core to bypass the PID misses and keep the animations smooth. 
