#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <time.h>
#define LED 2

char WIFI_SSID[32];
char WIFI_PASSWORD[32];
//const char* WIFI_SSID = "XD";
//const char* WIFI_PASSWORD = "travi.tum0r";
//const char* WIFI_PASSWORD = "mAdz1c/aRad.eper4";
const int WIFI_TIMEOUT_MS = 5000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;


//---------------------------------------------------------------
//		Funciones Wifi
//---------------------------------------------------------------

/// Muestra redes disponibles
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

/// Se conecta a una red en especifico
void connectToWiFi(char* WIFI_SSID, char* WIFI_PASSWORD) {
	WiFi.disconnect();
	Serial.printf("\nConnecting to %s ...",WIFI_SSID);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
	// Esperando a que se conecte o al timeout
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
		Serial.print(".");
		delay(100);
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Failed");
		esp_deep_sleep_start();		// El ESP se suspende 
	}
	else {
		Serial.println("Conected!");
	}
}

/// Selecciona una red de las disponibles
void selectNetwork(char* WIFI_SSID, char* WIFI_PASSWORD) {
	Serial.print("Select SSID: ");
	while (!Serial.available()) ;
	char SSID_id = Serial.read();
	String SSID_str = WiFi.SSID(SSID_id - '1'); 
	SSID_str.toCharArray(WIFI_SSID,SSID_str.length()+1);

	Serial.printf("%s selected \n",WIFI_SSID);

	Serial.print("Password: ");
	int i; char pw_char;
	for (i=0; i<32; i++) {
		while (!Serial.available()) ;
		pw_char = Serial.read();
		Serial.print(pw_char);
		if (pw_char=='\r') {
			Serial.read();
			WIFI_PASSWORD[i] = '\0';
			break;
		}
		WIFI_PASSWORD[i] = pw_char;
	}
	if (i==32)	Serial.println("\nCharacter limit for password surpassed");
}

/// Setea la fecha y hora manualmente
void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst){
  struct tm tm;
  tm.tm_year = yr - 1900;   // Set date
  tm.tm_mon = month-1;
  tm.tm_mday = mday;
  tm.tm_hour = hr;      // Set time
  tm.tm_min = minute;
  tm.tm_sec = sec;
  tm.tm_isdst = isDst;  // 1 or 0
  time_t t = mktime(&tm);
  Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}



//---------------------------------------------------------------
//		Funciones principales
//---------------------------------------------------------------
void setup() {
	Serial.begin(921600);
	pinMode(LED,OUTPUT);
	//pinMode(12,INPUT);
	showNetworks();
	selectNetwork(WIFI_SSID, WIFI_PASSWORD);
	connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
}

void loop() {
	struct tm timeinfo;
	char Timestamp[50];
	
	delay(900);
	digitalWrite(LED,HIGH);
	delay(100);
	digitalWrite(LED,LOW);

	getLocalTime(&timeinfo);
	sprintf(Timestamp,"%02d/%02d/%d %02d:%02d:%02d",timeinfo.tm_mday, timeinfo.tm_mon + 1, \
	 timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
	Serial.println(Timestamp);
}