#include <Arduino.h>
#include <BluetoothSerial.h>
// #include <ELMduino.h>

// BluetoothSerial ELM_PORT;
// ELM327 myELM327;

// uint32_t rpm = 0;
// uint8_t address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};

// void setup() {
//   Serial.begin(115200);
//   ELM_PORT.begin("ArduHUD", true);
//   ELM_PORT.setPin("1234");

//   if (!ELM_PORT.connect(address)) {
//     Serial.println("Couldn't connect to OBD scanner - Phase 1");
//     while(1);
//   }

//   if (!myELM327.begin(ELM_PORT, false, 2000)) {
//     Serial.println("Couldn't connect to OBD scanner - Phase 2");
//     while (1);
//   }

//   Serial.println("Connected to ELM327");
// }

// int encendido = 0;
// void loop() {
//   float tempRPM = myELM327.rpm();
//   if (myELM327.nb_rx_state == ELM_SUCCESS) {
//     //rpm = (uint32_t)tempRPM;
//     //Serial.print("RPM: "); Serial.println(rpm);
//     if (encendido==0) {
//       Serial.println("ON");
//       encendido = 1;
//     }
//   }
//   else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
//     //myELM327.printError();
//     if (encendido==1) {
//       Serial.println("OFF");
//       encendido = 0;
//     }
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