#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <WiFiClientSecure.h>
#include <time.h>
// #include <SPI.h>

char WIFI_SSID[32];
char WIFI_PASSWORD[32];
//char* WIFI_SSID = "Celular";
//char* WIFI_PASSWORD = "contra123";
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

/// @brief Se conecta a una red en especifico
bool connectToWiFi(const char* wifi_ssid, const char* wifi_password) {
	WiFi.disconnect(true,true);
  delay(10);
	Serial.printf("\nConnecting to %s ...",wifi_ssid);
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifi_ssid,wifi_password);
	// Esperando a que se conecte o al timeout
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
		Serial.print(".");
		delay(100);
	}
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("Failed");
		return false; 
	}
	else {
		Serial.println("Conected!");
    return true;
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

void setup(){
  Serial.begin(921600);

  showNetworks();
	selectNetwork(WIFI_SSID, WIFI_PASSWORD);
  bool is_connected = connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

  if (is_connected)
  	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
  else {
    setTime(2022,12,26,13,15,0,0);
  }

  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  File file = SD.open("/prueba2.csv", FILE_WRITE);
  file.print("Estado, Timestamp\n");
  file.close();
}
bool encendido = true;
int i = 1;
void loop(){
  //char row[50];
  struct tm timeinfo;
  char Timestamp[50];

	getLocalTime(&timeinfo);
	sprintf(Timestamp,"%02d/%02d/%d %02d:%02d:%02d",timeinfo.tm_mday, timeinfo.tm_mon + 1, \
	 timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  File file = SD.open("/prueba.csv", FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  else {
    if (encendido)
      file.print("ON,");
    else 
      file.print("OFF,");
    encendido = !encendido;
    file.print(Timestamp);
    file.print("\n");
    file.close();
    delay(500);
    Serial.printf("\rLinea escrita %d",i);
    i++;
  }
}