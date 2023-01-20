#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ELMduino.h>

// BluetoothSerial ELM_PORT;
// ELM327 myELM327;

// uint32_t rpm = 0;
// uint8_t address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};

// unsigned long start_ms;
// void setup() {
//   Serial.begin(115200);
//   ELM_PORT.begin("ESP32", true);
//   ELM_PORT.setPin("1234");
//   Serial.println("Connecting to OBD scanner...");
//   ELM_PORT.connect(address);
//   if (!ELM_PORT.connected(1000)) {
//     Serial.println("Couldn't connect to OBD scanner - Phase 1");
//     esp_deep_sleep_start();
//   }

//   if (!myELM327.begin(ELM_PORT, false, 10000)) {
//     Serial.println("Couldn't connect to OBD scanner - Phase 2");
//     esp_deep_sleep_start();
//   }

//   Serial.println("Connected to ELM327");
//   start_ms = millis();
// }

// int encendido = 0;
// //unsigned long lastBT, now = millis(); 
// void loop() {
//   myELM327.rpm();
//   if (myELM327.nb_rx_state == ELM_SUCCESS) {
//     Serial.println("Message received");
//     // Serial.printf("Supported PIDs 01-20: %d\n", myELM327.supportedPIDs_1_20());
//     // Serial.printf("Supported PIDs 21-40: %d\n", myELM327.supportedPIDs_21_40());
//     // Serial.printf("Supported PIDs 41-60: %d\n", myELM327.supportedPIDs_41_60());
//     // Serial.printf("Supported PIDs 61-80: %d\n", myELM327.supportedPIDs_61_80());
//     // if (encendido==0) {
//     //   Serial.println("ON");
//     //   encendido = 1;
//     // }
//   }
//   else if (myELM327.nb_rx_state == ELM_GETTING_MSG) {
//     Serial.println("Getting msg ...");
//     //myELM327.printError();
//     // if (encendido==1) {
//     //   Serial.println("OFF");
//     //   encendido = 0;
//     // }
//   }
//   else {
//     myELM327.printError();
//   }
//   delay(100);
  
//   if (millis() - start_ms > 20000) {
//     Serial.println("Waiting...");
//     delay(3000);
//     start_ms = millis();
//   }
// }

BluetoothSerial SerialBT;
#define DEBUG_PORT Serial
#define ELM_PORT   SerialBT

uint8_t ELM_address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  DEBUG_PORT.begin(115200);
  ELM_PORT.begin("ESP32test", true);
  ELM_PORT.setPin("1234");

  DEBUG_PORT.println("Attempting to connect to ELM327...");

  if (!ELM_PORT.connect(ELM_address))
  {
    DEBUG_PORT.println("Couldn't connect to OBD scanner");
    while(1);
  }

  DEBUG_PORT.println("Connected to ELM327");
  DEBUG_PORT.println("Ensure your serial monitor line ending is set to 'Carriage Return'");
  DEBUG_PORT.println("Type and send commands/queries to your ELM327 through the serial monitor");
  DEBUG_PORT.println();
}


void loop()
{
  if(DEBUG_PORT.available())
  {
    char c = DEBUG_PORT.read();

    DEBUG_PORT.write(c);
    ELM_PORT.write(c);
  }

  if(ELM_PORT.available())
  {
    char c = ELM_PORT.read();

    if(c == '>')
      DEBUG_PORT.println();

    DEBUG_PORT.write(c);
  }
}