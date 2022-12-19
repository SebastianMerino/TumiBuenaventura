#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

void setup() {
	Serial.begin(921600);
	// put your setup code here, to run once:
	SerialBT.begin("ESP32");
	Serial.println("\nDevice connected!");
}

void loop() {
	if (Serial.available())
		SerialBT.write(Serial.read());
	if(SerialBT.available())
		Serial.write(SerialBT.read());
	delay(20);
}