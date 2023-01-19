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

const int LED_POWER = 4;
const int LED_CONNECTION = 21;

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

BluetoothSerial SerialBT;
ELM327 myELM327;
uint8_t ELM_address[6]  = {0x01, 0x23, 0x45, 0x67, 0x89, 0xBA};


//---------------------------------------------------------------
//		VARIABLES GLOBALES
//---------------------------------------------------------------
bool attached_card;
// char wifi_ssid[32];
// char wifi_password[32];
//char wifi_ssid[] = "iPhone12ProJean";
//char wifi_password[] = "Artilugio2014";
//char wifi_ssid[] = "Lalorp";
//char wifi_password[] = "cristobal";
char wifi_ssid[] = "iPhone12ProJean";
char wifi_password[] = "Jean1976";
int reconnect_time_ms = 30*1000;
bool encendido = false;
bool mqtt_connected;
struct tm timeinfo;
char mqtt_message[25];


//---------------------------------------------------------------
//		FUNCIONES
//---------------------------------------------------------------

/// @brief Receives a string from the serial port
/// @param array Pointer to string
/// @param max_len Maximum string length
void receive_str(char* array, int max_len)
{
  int i; char rx_char;
  for (i=0; i<max_len; i++) {
    while (!Serial.available()) ;
    rx_char = Serial.read();
    Serial.print(rx_char);
    if (rx_char=='\r') {
      Serial.read();
      array[i] = '\0';
      break;
    }
    array[i] = rx_char;
  }
  Serial.print(Serial.read());
  if (i==max_len) Serial.println("Character limit surpassed");
}

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
	receive_str(WIFI_PASSWORD, 32);
}

/// @brief Se conecta al servidor MQTT 
bool MQTTinitialize()
{
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (MQTTclient.connect("ESP32Client",MQTT_USERNAME,MQTT_PASSWORD,"vehiculos/auto_prueba/conectado",2,true,"N")) {
    Serial.println("connected");
    digitalWrite(LED_CONNECTION, HIGH);
    MQTTclient.publish("vehiculos/auto_prueba/conectado","Y",true);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(MQTTclient.state());
    return false;
  }
}

/// @brief Inicializa el Bluetooth y se conecta al ELM327 
bool OBD2setup()
{
  // Serial.println("Connect to OBD2 port and turn on vehicle");
  // Serial.println("Press any key when ready");
  // while (!Serial.available()) ;
  // Serial.read();
  Serial.println("\nConnecting to OBDII scanner..."); 
  SerialBT.begin("ESP32", true);
  SerialBT.setPin("1234");
  if (!SerialBT.connect(ELM_address)) {
    Serial.println("Couldn't connect to OBD scanner - Phase 1");
    return false;
  }
  if (!myELM327.begin(SerialBT, false, 5000)) {
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
bool SDsetup()
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

/// @brief Saves data in csv file 
void SaveData()
{
  File file = SD.open("/Data.csv", FILE_APPEND);
  getLocalTime(&timeinfo);
  if (encendido) {
    sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,Y",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
    timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  else {
    sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,N",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
    timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  file.print(mqtt_message);
  file.print("\n");
  file.close();
}

/// @brief Uploads data in the SD card to the cloud
void UploadData()
{
  char rx_char;
  char line[25];
  int id_char;
  File file = SD.open("/Data.csv", FILE_READ);

  while (file.available()) {
    for (id_char=0; id_char<25; id_char++) {
      rx_char = file.read();
      if (rx_char == '\n') {
        line[id_char] = '\0';
        break;
      } 
      line[id_char] = rx_char;
    }
    //Serial.println(line);
    MQTTclient.loop();
    MQTTclient.publish("vehiculos/auto_prueba/encendido",line);
    Serial.print("Publicando: ");
    Serial.println(line);
    digitalWrite(LED_CONNECTION, LOW);
    delay(150);
    digitalWrite(LED_CONNECTION, HIGH);
    delay(100);
  }
  SD.remove("/Data.csv");
}

/// @brief Set digital pins to 0
void pullDownPins() 
{
  int pines[] = {2,12,13,14,15,22,25,26,27,32,33};
  for (int i=0; i<11; i++) {
    pinMode(pines[i], INPUT_PULLDOWN);
  }
}

//---------------------------------------------------------------
//		PRINCIPAL
//---------------------------------------------------------------
void setup() {
  pullDownPins();
  pinMode(LED_CONNECTION, OUTPUT);
  pinMode(LED_POWER, OUTPUT);
  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_CONNECTION, LOW);

  Serial.begin(9600);

  if (!OBD2setup()) {
    while (1) {
      digitalWrite(LED_POWER, LOW);
      delay(500);
      digitalWrite(LED_POWER, HIGH);
      delay(500);
    }
  }

  // showNetworks();
  // selectNetwork(wifi_ssid, wifi_password);
  connectToWiFi(wifi_ssid, wifi_password);
  if (WiFi.isConnected()) {
    MQTTclient.setServer(MQTT_URL, MQTT_PORT);
    MQTTclient.setKeepAlive(2);
    MQTTinitialize();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
  }
  else {
    setTime(2023,01,01,0,0,0,0);
  }

  attached_card = SDsetup(); 
  if (attached_card) {
    SD.remove("/Data.csv");
    File file = SD.open("/Data.csv", FILE_WRITE);
    // file.print("Timestamp, Encendido\n");
    file.close();
    Serial.println("SD card ready");
  }
}

unsigned long now, lastBT = 0, last_reconect_try = 0, lastPub = 0;
float rpm;

void loop() {
  now = millis();

  // If it is not connected
  if (!WiFi.isConnected() || !MQTTclient.connected()) {
      digitalWrite(LED_CONNECTION, LOW);

      // Tries to reconnect once every minute
      if (now - last_reconect_try > reconnect_time_ms) {
        last_reconect_try = millis();
        if (!WiFi.isConnected()) {
          connectToWiFi(wifi_ssid, wifi_password);
        }
        if (!MQTTclient.connected()) {
          configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
          MQTTinitialize();
          if (MQTTclient.connected()) {
            Serial.println("Uploading data...");
            UploadData();
          }
        }
      }
  }

  // Send MQTT message each 10 seconds
  if (now - lastPub > 10000) {
    lastPub = millis();
    getLocalTime(&timeinfo);
    if (encendido) {
      sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,Y",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else {
      sprintf(mqtt_message,"%d-%02d-%02dT%02d:%02d:%02d,N",timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    if (WiFi.isConnected() && MQTTclient.connected()) {
      Serial.print("Publicando: ");
      Serial.println(mqtt_message);
      MQTTclient.publish("vehiculos/auto_prueba/encendido",mqtt_message);
    }
    else if (attached_card) {
      Serial.print("Guardando en SD: ");
      Serial.println(mqtt_message);
      SaveData();
    } 
  }

  // Every second you send a BT message
  if (now - lastBT > 120) {
    lastBT = millis();
    MQTTclient.loop();

    // Asks for information
    rpm = myELM327.rpm();
    if (myELM327.nb_rx_state == ELM_SUCCESS && !encendido) {
      Serial.println("on");
      encendido = true;
      reconnect_time_ms = 5*1000;
    }

    else if (myELM327.nb_rx_state == ELM_NO_DATA && encendido) {
      Serial.println("off");
      encendido = false;
      reconnect_time_ms = 30*1000;
    }
  }
}