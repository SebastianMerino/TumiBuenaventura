#include <Arduino.h>
#include <WiFiClientSecure.h>
#define LED 2

//#define WIFI_SSID "redpucp"
//#define WIFI_PASSWORD "C9AA28BA93"
//#define WIFI_SSID "URSULA"
//#define WIFI_PASSWORD "MAGALY57"
#define WIFI_SSID "XD"
#define WIFI_PASSWORD "travi.tum0r"
#define WIFI_TIMEOUT_MS 5000

void showNetworks() {
	int n = WiFi.scanNetworks();
	Serial.println("scan done");
	if (n == 0) {
		Serial.println("no networks found");
	} else {
	Serial.print(n);
	Serial.println(" networks found");
	}
	for (int i = 0; i < n; ++i) {
		// Print SSID and RSSI for each network found
		Serial.print(i + 1);
		Serial.print(": ");
		Serial.print(WiFi.SSID(i));
		Serial.print(" (");
		Serial.print(WiFi.RSSI(i));
		Serial.print(")");
		Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
		delay(10);
	}
}

void connectToWiFi() {
	Serial.println("");
	Serial.println("Connecting...");
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
	unsigned long start = millis();

	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
		Serial.print(".");
		delay(100);
	}

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Failed");
		esp_deep_sleep_start();
	}
	else {
		Serial.println("Conected!");
		Serial.println(WiFi.localIP());
	}
}


void setup() {
	Serial.begin(921600);
	pinMode(LED,OUTPUT);
	showNetworks();
	connectToWiFi();
}

void loop() {
	delay(900);
	digitalWrite(LED,HIGH);
	delay(100);
	digitalWrite(LED,LOW);
}