#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ELMduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SD.h>
#include <time.h>

//---------------------------------------------------------------
//		CONSTANTES
//---------------------------------------------------------------
// const char* WIFI_SSID = "Celular";
// const char* WIFI_PASSWORD = "contra123";
const int WIFI_TIMEOUT_MS = 2000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;

const char* MQTT_URL = "broker.emqx.io";
const char* MQTT_USERNAME = "emqx";
const char* MQTT_PASSWORD = "public";
const int MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

BluetoothSerial SerialBT;
ELM327 myELM327;
uint8_t ELM_address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};
//---------------------------------------------------------------
//		FUNCIONES
//---------------------------------------------------------------

/// @brief Se conecta a una red en especifico
bool connectToWiFi(const char* wifi_ssid, const char* wifi_password)
{
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

/// @brief Muestra redes disponibles
void showNetworks()
{
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

/// @brief Selecciona una red de las disponibles
void selectNetwork(char* WIFI_SSID, char* WIFI_PASSWORD)
{
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

/// @brief Espera a la reconexión al servidor MQTT 
void MQTTreconnect()
{
  if (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTTclient.connect("ESP32Client",MQTT_USERNAME,MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(MQTTclient.state());
      //Serial.println(" try again in 5 seconds");
      //delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

/// @brief Inicializa el Bluetooth y se conecta al ELM327 
bool OBD2setup()
{
  Serial.println("\nConnecting to OBDII scanner..."); 
  SerialBT.begin("ESP32", true);
  SerialBT.setPin("1234");
  if (!SerialBT.connect(ELM_address)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 1");
    return false;
  }
  if (!myELM327.begin(SerialBT, false, 500)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    return false;
  }
  Serial.println("Connected to ELM327");
  return true;
}

/// @brief Setea la fecha y hora manualmente
void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst)
{
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

/// @brief Setup for SD Card
/// @return False if error
bool setupSD()
{
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
    return false;
  }
  if(SD.cardType() == CARD_NONE){
    Serial.println("No SD card attached");
    return false;
  }
  Serial.println("SD card detected");
  return true;
}

//---------------------------------------------------------------
//		PRINCIPAL
//---------------------------------------------------------------
bool is_connected;
char wifi_ssid[32];
char wifi_password[32];

void setup() {
  Serial.begin(9600);
  showNetworks();
  selectNetwork(wifi_ssid, wifi_password);
  is_connected = connectToWiFi(wifi_ssid, wifi_password);

  if (is_connected) {
    MQTTclient.setServer(MQTT_URL, MQTT_PORT);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
  } 
  else {
    setTime(2023,01,01,0,0,0,0);
    //esp_deep_sleep_start();		// El ESP se suspende
  }

  if (!setupSD()) {
    esp_deep_sleep_start();		// El ESP se suspende
  }
  else {
    SD.remove("/Data.csv");
    File file = SD.open("/Data.csv", FILE_WRITE);
    file.print("Timestamp, Estado\n");
    file.close();
    Serial.println("SD card ready");
  }
  
  if (!OBD2setup()) {
    esp_deep_sleep_start();		// El ESP se suspende
  }
}

unsigned long now, lastBT = millis(), lastWiFi = millis();
float rpm, throttle, fuelLevel, kph;
struct tm timeinfo;
char Timestamp[50];
bool encendido = true;

void loop() {
  now = millis();
  if (now - lastBT > 500) {
    lastBT = millis();
    Serial.print(lastBT);
    
    if (now - lastWiFi > 10*60*1000) {
      lastWiFi = millis();
      is_connected = connectToWiFi(wifi_ssid, wifi_password);
    }

    rpm = myELM327.rpm();
    Serial.println(myELM327.nb_rx_state);
    if (myELM327.nb_rx_state == ELM_SUCCESS && !encendido) {
      Serial.println("on");
      File file = SD.open("/Data.csv", FILE_APPEND);
      if(file) {
        getLocalTime(&timeinfo);
        sprintf(Timestamp,"%02d/%02d/%d %02d:%02d:%02d",timeinfo.tm_mday, timeinfo.tm_mon + 1, \
        timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        file.print(Timestamp);
        file.print(",ON\n");
        file.close();
        encendido = true;

        if (is_connected) {
          MQTTreconnect();
          if (MQTTclient.connected()) {
            MQTTclient.loop();
            MQTTclient.publish("auto/encendido","on");
          }  
        }
      }
    }

    else if ((myELM327.nb_rx_state == ELM_NO_DATA)&& encendido) {
      //myELM327.printError();
      Serial.println("off");
      File file = SD.open("/Data.csv", FILE_APPEND);
      if(file) {
        getLocalTime(&timeinfo);
        sprintf(Timestamp,"%02d/%02d/%d %02d:%02d:%02d",timeinfo.tm_mday, timeinfo.tm_mon + 1, \
        timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        file.print(Timestamp);
        file.print(",OFF\n");
        file.close();
        encendido = false;

        if (is_connected) {
          MQTTreconnect();
          if (MQTTclient.connected()) {
            MQTTclient.loop();
            MQTTclient.publish("auto/encendido","off");
          }
        }
      }
    }
  }
}