#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ELMduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SD.h>
#include <time.h>
#include <esp_bt.h>
#include <Wire.h>
#include "RTClib.h"

/* Observaciones antes de la implementacion:
  - 
  - 
  -  */

//---------------------------------------------------------------
//		CONSTANTES
//---------------------------------------------------------------
const int WIFI_TIMEOUT_MS = 2000;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000;
const int   daylightOffset_sec = 3600;

const int LED_POWER = 4;
const int LED_CONNECTION = 21;
const int I2C_SDA = 15;
const int I2C_SCL = 22;
const int LINE_SIZE = 23;

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
//		VARIABLES GLOBALES
//---------------------------------------------------------------
bool attached_card;
char wifi_ssid[] = "RouterPortatil";
char wifi_password[] = "Buenaventura2023";
// char wifi_ssid[] = "CELULAR";
// char wifi_password[] = "holis123";
int sent_lines = 0;
int reconnect_time_ms;
bool mqtt_connected;
bool encendido = false;
bool BT_connected = true;
bool uploading = false;
struct tm timeinfo;
TwoWire I2C_rtc = TwoWire(0);
RTC_DS3231 myRTC;

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

/// @brief Se conecta al servidor MQTT 
bool MQTTinitialize()
{
  MQTTclient.disconnect();
  MQTTclient.setServer(MQTT_URL, MQTT_PORT);
  MQTTclient.setKeepAlive(5);
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (MQTTclient.connect("ESP32Client",MQTT_USERNAME,MQTT_PASSWORD,"vehiculos/placa/conectado",2,true,"N")) {
    Serial.println("connected");
    MQTTclient.publish("vehiculos/placa/conectado","Y",true);
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
  // Serial.println("\nConnecting to OBDII scanner..."); 
  SerialBT.begin("ESP32", true);
  SerialBT.setPin("1234");
  if (!SerialBT.connect(ELM_address)) {
    // Serial.println("Couldn't connect to OBD scanner - Phase 1");
    return false;
  }
  if (!myELM327.begin(SerialBT, false, 500)) {
    // Serial.println("Couldn't connect to OBD scanner - Phase 2");
    return false;
  }
  // Serial.println("Connected to ELM327");
  return true;
}

/// @brief Disconnects BT and unlinks device
void BTdisconnect()
{
  SerialBT.disconnect();
  SerialBT.unpairDevice(ELM_address);
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

/// @brief Gets MQTT message from current time and state of vehicle
void GetMQTTMessage(char* msg)
{
  // getLocalTime(&timeinfo);
  // DateTime dt = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
  //   timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  DateTime dt = myRTC.now();
  if (encendido) {
    sprintf(msg,"%d-%02d-%02dT%02d:%02d:%02d,Y",dt.year(), dt.month(), dt.day(), \
    dt.hour(), dt.minute(), dt.second());
  }
  else {
    sprintf(msg,"%d-%02d-%02dT%02d:%02d:%02d,N",dt.year(), dt.month(), dt.day(), \
    dt.hour(), dt.minute(), dt.second());
  }
}

/// @brief Saves data in csv file 
void SaveData(char* msg)
{
  File file = SD.open("/Data.csv", FILE_APPEND);
  file.println(msg);
  file.close();
}

/// @brief Deletes line
/// @param line_number 	Number of line in file (index from 0)
void DeleteLine(int line_number)
{
  File file_writer = SD.open("/Data.csv", "r+");
  file_writer.seek(line_number*LINE_SIZE);
  // Delete evereything but the EOL
  for(int i=0;i<LINE_SIZE-2;i++) {
    file_writer.print(" "); // all marked as deleted! yea!
  }
}

/// @brief  Reads a single line
/// @param line_number 	Number of line in file (index from 0)
/// @param line 		Char array where line is located
/// @return False if en of file is reached
bool ReadLine(int line_number, char* line)
{
  char rx_char;
  int id_char = 0;

  File file_reader = SD.open("/Data.csv", "r+");
  file_reader.seek(line_number*LINE_SIZE);

  if (!file_reader.available()) {
    return false;
  }
  else {
    while (id_char<25) {
      rx_char = file_reader.read();
      if (rx_char == '\r') {
        if (file_reader.read() == '\n') {
          line[id_char] = '\0';
          break;
        }
        else {
          return false;
        }
      } 
      line[id_char] = rx_char;
      id_char++;
    }
    file_reader.close();
    return true;
  }
    
}

/// @brief  Saves line number of Data file
/// @param line_number  Number of line (index from 0)
void SaveLineNum(int line_number)
{
  File file_writer = SD.open("/number.txt", "w+");
  file_writer.printf("%d",line_number);
  file_writer.close();
}

/// @brief Reads line number of Data file
/// @return Number of line (index from 0)
int ReadLineNum()
{
  char rx_char;
  int number = 0;

  File file_reader = SD.open("/number.txt", "r");
  while (file_reader.available()) {
    rx_char = file_reader.read();
    if (rx_char>='0' && rx_char<='9') {
      number = (rx_char - '0') + number*10;
    }
  }
  file_reader.close();
  Serial.print("Line number: ");
  Serial.println(number);
  return number;
}

/// @brief Uploads data in the SD card to the cloud
void UploadData()
{
  char line[25];
  int current_line = ReadLineNum();

  while (1) {
    // Reads current line
    if (!ReadLine(current_line,line)) {
      break;
    }
    Serial.print("Read line: ");
    Serial.print(line);

    MQTTclient.loop();

    // Deleted line
    if (line[0] == ' ') {
      Serial.println(" Ignoring...");
      current_line++;
      SaveLineNum(current_line);
      continue; 
    }

    // Publish within timeout
    unsigned long start = millis();
    bool published = false;
    while (millis() - start < 1000) {
      if (MQTTclient.publish("vehiculos/placa/encendido",line)) {
        published = true;
        break;
      }
    }

    // Deleting line if published
    if (published) {
      Serial.println(" Publishing...");
      DeleteLine(current_line);
      digitalWrite(LED_CONNECTION, LOW);
      delay(150);
      digitalWrite(LED_CONNECTION, HIGH);
      delay(100);
      current_line++;
      SaveLineNum(current_line);
    }
    else {
      Serial.println(" Lost connection...");
      return; // Keep the file if not published
    }
    
  }
  Serial.println("Deleting file...");
  SD.remove("/Data.csv");
  File file = SD.open("/Data.csv", FILE_APPEND);
  file.close();
  SaveLineNum(0);
  sent_lines = 0;
}

/// @brief Set digital pins to 0
void pullDownPins() 
{
  int pines[] = {2,12,13,14,15,22,25,26,27,32,33};
  for (int i=0; i<11; i++) {
    pinMode(pines[i], INPUT_PULLDOWN);
  }
}

/// @brief  Function for threaded BT communication with ELM327
/// @param parameter NONE
void BTthread(void * parameter)
{
  while (1) {
    if (!BT_connected) {
      BTdisconnect();
      BT_connected = OBD2setup();
    }
    else {
      myELM327.rpm(); // Asks for information
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        if (!encendido) {
          Serial.println("on");
        }
        BT_connected = true;
        encendido = true;
        reconnect_time_ms = 5*1000;
      }
      else if (myELM327.nb_rx_state == ELM_NO_DATA) {
        if (encendido) {
          Serial.println("off");
        }
        BT_connected = true;
        encendido = false;
        reconnect_time_ms = 30*1000;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        Serial.println("ELM327 disconnected");
        BT_connected = false;
        encendido = false;
        reconnect_time_ms = 30*1000;
        myELM327.printError();
      }
    }
  }
}

TaskHandle_t TaskBT;

//---------------------------------------------------------------
//		MAIN SETUP
//---------------------------------------------------------------
void setup() {
  pullDownPins();
  pinMode(LED_CONNECTION, OUTPUT);
  pinMode(LED_POWER, OUTPUT);
  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_CONNECTION, LOW);
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  Serial.begin(9600);

  // OBD2 port setup
  unsigned long timeout = 10000;
  unsigned long start = millis();
  bool error = true;
  while (millis() - start < timeout) {
    if (!OBD2setup()) {
      BTdisconnect();
      Serial.println("Reconnecting to ELM327...");
    }
    else {
      error = false;
      break;
    }
  }
  if (error) {
    Serial.println("Could not connect to ELM327, rebooting...");
    ESP.restart();
  }
  xTaskCreatePinnedToCore(BTthread,"TaskBT",10000,NULL,1,&TaskBT,1); 

  // Waiting for MQTT connection
  timeout = 10*1000;
  reconnect_time_ms = 30*1000;
  start = millis();
  while (millis() - start < timeout) {
    if (!WiFi.isConnected()) {
      connectToWiFi(wifi_ssid, wifi_password);
    }
    if (WiFi.isConnected()) {
      if (MQTTinitialize()) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
        digitalWrite(LED_CONNECTION, HIGH);
        reconnect_time_ms = 5*1000;
        break;
      }
    }
    delay(5*1000);
  }

  // RTC module
  I2C_rtc.begin(I2C_SDA, I2C_SCL, 100000);
  myRTC.begin(&I2C_rtc);
  if (MQTTclient.connected() && WiFi.isConnected()) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
    getLocalTime(&timeinfo);
    DateTime newDT = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    myRTC.adjust(newDT);
  }

  // SD card setup
  attached_card = SDsetup(); 
  if (attached_card) {
    Serial.println("SD card ready");
    if (WiFi.isConnected() && MQTTclient.connected()) {
      UploadData();
    }
  }
  else {
    Serial.println("No memory");
  }
}

//---------------------------------------------------------------
//		MAIN LOOP
//---------------------------------------------------------------
unsigned long lastBT = 0, last_reconect_try = 0, lastPub = 0, lastMQTT = 0, lastTime = 0;
unsigned long BT_time_ms = 90;
char mqtt_message[25];

void loop() {
  // MQTT ping every 2 seconds
  if (millis() - lastMQTT > 2000) {
    // Serial.println("Looping MQTT...");
    lastMQTT = millis();
    MQTTclient.loop();
  }

  // Checks connection
  if (!MQTTclient.connected() || !WiFi.isConnected()) {
    digitalWrite(LED_CONNECTION, LOW);

    // Tries to reconnect once every reconnect time
    if (millis() - last_reconect_try > reconnect_time_ms) {
      last_reconect_try = millis();
      if (!WiFi.isConnected()) {
        connectToWiFi(wifi_ssid, wifi_password);
      }
      if (WiFi.isConnected()) {
        MQTTinitialize();
        if (MQTTclient.connected()) {
          digitalWrite(LED_CONNECTION, HIGH);
          configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
          if (attached_card) {
            Serial.println("Uploading data...");
            UploadData();
          }
        }
      }
    }
  }

  // Send MQTT message each 10 seconds
  if (millis() - lastPub > 10000) {
    lastPub = millis();
    GetMQTTMessage(mqtt_message);

    if (WiFi.isConnected() && MQTTclient.connected()) {
      Serial.print("Publicando: ");
      Serial.println(mqtt_message);
      MQTTclient.publish("vehiculos/placa/encendido",mqtt_message,true);
      if (BT_connected) {
        MQTTclient.publish("vehiculos/placa/BT","Y",true);
      }
      else {
        MQTTclient.publish("vehiculos/placa/BT","N",true);
      }
    }
    else if (attached_card) {
      Serial.print("Guardando en SD: ");
      Serial.println(mqtt_message);
      SaveData(mqtt_message);
    } 
    else {
      Serial.println("No hay conexion");
    }
  }

  // Time sinchronization
  if (millis() - lastTime > 24*60*60*1000) {
    lastTime = millis();
    if (WiFi.isConnected() && MQTTclient.connected()) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);	// Configura fecha y hora con WiFi
      getLocalTime(&timeinfo);
      DateTime newDT = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, \
        timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      myRTC.adjust(newDT);
    }
  }
}