#include <LittleFS.h>
#include <Arduino.h>
#include "FS.h"


#include <WiFi.h>
#include <WiFiMulti.h>

#include <WebServer.h>
#include <ESPmDNS.h>

#include <WebSocketsServer.h>

#include <ArduinoOTA.h>

#include <Wire.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"
#include <dht.h>

#include "time.h"

#include "EPD.h"
#include <SPI.h>
#include "EPD_1in54_V2.h"
#include "GUI_Paint.h"
#include "DEV_Config.h"
#include <stdio.h>
#include "icons.h"
#include <stdlib.h>

#define DHT11_PIN 3 // will need to change this for esp32
#define SOIL_PIN 2 // will need to change this for esp32

#define AL_ADDR 0x10 // I2C Address

WiFiMulti wifiMulti;       // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

WebServer server(80);       // Create a webserver object that listens for HTTP request on port 80 
WebSocketsServer webSocket(81);    // create a websocket server on port 81

const char *ssid = "ESP32 Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "";   // The password required to connect to it, leave blank for an open network

const char* mdnsName = "potplant"; // Domain name for the mDNS responder

// Possible values: .125(1/8), .25(1/4), 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark
// glass.
float gain = .125;
// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light. 
int integration_time = 100;
int button_state;

unsigned long lastlight = 0;  // will store last time light was read
int lightinterval = 10*1000; // interval at which to read light (milliseconds)

Epd epd;
unsigned char image[1024];
Paint paint(image, 0, 0);

unsigned long time_start_ms;
unsigned long time_now_s;
#define COLORED     0
#define UNCOLORED   1

const unsigned char* faces[5] = {happy_1, happy_2, neutral_3, sad_4, sad_5};

void set_background();
void set_time(int hh, int mm);
void set_date(int dd, int mm, int yy);
void set_message(char* buf, int num_chars);
void set_moisture(int moisture);
void set_light(int light);
void set_temp(int temp);
void draw_bitmap(Paint* paint, const unsigned char* bitmap,
                int x0, int y0, int width, int height, int colored);


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10*3600; //australia is gmt +10
const int   daylightOffset_sec = 3600;

unsigned long lastscreen = 0;  // will store last time screen was updated
int screeninterval = 5*1000; // interval at which to update display (milliseconds)

unsigned long lastpoll = 0;  // will store last time data was polled
int pollinginterval = 0.1*1000; // interval at which to poll data (milliseconds

const char *ssid = "ESP32 Access Point"; // The name of the Wi-Fi network that will be created
const char *password = "";   // The password required to connect to it, leave blank for an open network

const char *OTAName = "ESP32";           // A name and a password for the OTA service
const char *OTAPassword = "esp32";

const char* mdnsName = "potplant"; // Domain name for the mDNS responder

void setup() {
      // the pins with LEDs connected are outputs

  Serial.begin(115200);        // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi("iBone", "12345678");                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  
  startOTA();                  // Start the OTA service
  
  startFileSystem();               // Start the SPIFFS and list all contents

  startWebSocket();            // Start a WebSocket server
  
  startMDNS(mdnsName);                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler

    // Init and get time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  int state = 1;

  DEV_Module_Init();
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_Clear();
  DEV_Delay_ms(500);

  UBYTE *BlackImage;
  /* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
  UWORD Imagesize = ((EPD_1IN54_V2_WIDTH % 8 == 0)? (EPD_1IN54_V2_WIDTH / 8 ): (EPD_1IN54_V2_WIDTH / 8 + 1)) * EPD_1IN54_V2_HEIGHT;
  if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
      printf("Failed to apply for black memory...\r\n");
      while(1);
  }
  printf("Paint_NewImage\r\n");
  Paint_NewImage(BlackImage, EPD_1IN54_V2_WIDTH, EPD_1IN54_V2_HEIGHT, WHITE);
 
  Paint_SelectImage(BlackImage);
  Paint_SetRotate(ROTATE_270);

  set_background();
  set_time("12:34:56");
  set_date("1234-56-78");
  set_message("Setting up");
  set_moisture(12);
  set_light(34);
  set_temp(56);  

  Paint_SelectImage(BlackImage);
  Paint_DrawImage(faces[state], 100, 190, 40, 40, COLORED);
  EPD_1IN54_V2_Display(BlackImage);

  Wire.begin();
  Serial.begin(115200);

  if(light.begin())
    Serial.println("Ready to sense some light!"); 
  else
    Serial.println("Could not communicate with the sensor!");

  // Again the gain and integration times determine the resolution of the lux
  // value, and give different ranges of possible light readings. Check out
  // hoookup guide for more info. 
  light.setGain(gain);
  light.setIntegTime(integration_time);

  Serial.println("Reading settings..."); 
  Serial.print("Gain: ");
  float gainVal = light.readGain();
  Serial.print(gainVal, 3); 
  Serial.print(" Integration Time: ");
  int timeVal = light.readIntegTime();
  Serial.println(timeVal);
  
}

void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
  checkSerialCommand(); 
  
  int lightlevel = DetectLight();
  int moisturelevel = 12;
  int temperature = round(DetectTemp());

  if (millis() - lastpoll > pollinginterval) {  // if the time since the last update is bigger than the interval
    lastpoll = millis();
    
    addToDataTable("/light.txt", lightlevel, 100);
    addToDataTable("/moisture.txt", moisturelevel, 100); 
    addToDataTable("/temp.txt", temperature, 100);
    
  }

  if (millis() - lastscreen > screeninterval) {  // if the time since the last update is bigger than the interval
    lastscreen = millis();                    // save the last time you updated the screen


    set_time(getTimeString());
    set_date(getDateString());
    set_message(chooseDateNote(getDateString()));
    set_moisture(moisturelevel);
    set_light(lightlevel);
    set_temp(temperature);  

    draw_bitmap(&paint, faces[state], 100, 190, 40, 40, COLORED);

    epd.DisplayFrame();
  }

  Serial.print("Ambient Light Reading: ");
  Serial.print(DetectLight());
  Serial.println(" Lux");
  Serial.print("Temperature = ");
  Serial.println(DetectTemp());
  if (millis() - lastlight > lightinterval) {  // if the time since the last update is bigger than the interval
    lastlight = millis();                    // save the last time you updated the screen
    int sensor_value = analogRead(SOIL_PIN);
    Serial.println(sensor_value);
  }
}

void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

// Function to update or add an entry to notes.txt
void updateOrAddEntry(const String& date, const String& note) {
  File file = LittleFS.open("/notes.txt", "r");
  int size = note.length();
  String newnote = "";
  
  if (size != 1){
    newnote += note;
  }

  String newContent = "";
  bool found = false;

  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();

      if (line.startsWith(date + ":")) {
        if (size != 1){
        newContent += date + ": " + newnote + "\n"; 
        } else {
         newContent += "";  
        } // Overwrite old entry
        found = true;
      } else if (line.length() > 0) {
        newContent += line + "\n";  // Keep other lines
      }
    }
    file.close();
  }

  // If not found, append new line
  if (!found) {
    newContent += date + ": " + newnote + "\n";
  }

  // Overwrite file
  File writeFile = LittleFS.open("/notes.txt", "w");
  if (!writeFile) {
    Serial.println("Failed to write to file.");
    return;
  }
  writeFile.print(newContent);
  writeFile.close();

  //Serial.println("Entry saved for " + date);
}

String chooseDateNote(const String& date) {
  File file = LittleFS.open("/notes.txt", "r");
  String todaysItem = "";
  bool found = false;

  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();

      if (line.startsWith(date + ":")) {
        int sepIndex = line.indexOf(':');
        String note = line.substring(sepIndex + 2);
        found = true;
        return note;
      }
    }
    file.close();
  }
  if (!found) {
    return "Nothing on today brochacho";
  }
}

void checkSerialCommand() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "notes") {
      printNotesToSerial();
    } else if (input == "today"){
      Serial.println("--- Today's Item ---");
      Serial.println(chooseDateNote(getDateString()));
      Serial.println("--------------------");
    } else {
      Serial.println("Unknown command. Try: notes for full note dump, or today for today's item");
    }
  }
}
void printNotesToSerial() {
  if (!LittleFS.exists("/notes.txt")) {
    Serial.println("No notes found.");
    return;
  }

  File file = LittleFS.open("/notes.txt", "r");
  if (!file) {
    Serial.println("Failed to open notes.txt");
    return;
  }

  Serial.println("------ Notes ------");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }
  Serial.println("-------------------");
  file.close();
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getTimeString() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    return "Time not available";
  }
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getDateString() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    return "Date not available";
  }
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String getContentType(String filename) { // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endswith(".txt")) return "text/plain";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void set_time(String time){ // expects "HH:MM:SS"

  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_DisplayPartBaseImage(BlackImage);
  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  Paint_ClearWindows(16, 72, 16 + Font20.Width * 7, 72 + Font20.Height, WHITE);
  Paint_DrawString_EN(16, 72, time.substring(0,5), &Font20, BLACK, WHITE); // we cut off the seconds for display
  EPD_1IN54_V2_DisplayPart(BlackImage);
}

void set_date(String date){ // expects "YYYY-MM-DD"
  // draw date
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_DisplayPartBaseImage(BlackImage);
  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  Paint_ClearWindows(16, 112, 16 + Font20.Width * 7, 112 + Font20.Height, WHITE);
  Paint_DrawString_EN(16, 112, date.substring(2), &Font20, BLACK, WHITE); // we cut off the century for display
  EPD_1IN54_V2_DisplayPart(BlackImage);
}

void set_message(String buf){
  int num_chars = buf.length();
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_DisplayPartBaseImage(BlackImage);
  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  Paint_ClearWindows(32, 180, 32 + Font20.Width * 7, 180 + 2 * Font20.Height, WHITE);
  
  if(num_chars < 16){
    Paint_DrawString_EN(32, 180, buf, &Font16, BLACK, WHITE);
  } else {
    char line_1[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
    char line_2[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'}; 
    for(int i = 0; i < 15; i++){
      line_1[i] = buf[i];
    }
    
    for(int i = 15; i < num_chars; i++){
      line_2[i - 15] = buf[i];
    }

    Paint_DrawString_EN(32, 180, line_1, &Font16, BLACK, WHITE);
    Paint_DrawString_EN(32, 194, line_2, &Font16, BLACK, WHITE);
  }

  EPD_1IN54_V2_DisplayPart(BlackImage);
}

void set_moisture(int moisture){
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_DisplayPartBaseImage(BlackImage);
  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  
  char buffer[2];
  sprintf(buffer, "%02d", moisture);

  Paint_ClearWindows(16, 20, 16 + Font16.Width * 7, 20 + Font16.Height, WHITE);
  Paint_DrawString_EN(16, 20, buffer, &Font16, BLACK, WHITE); // we cut off the century for display
  EPD_1IN54_V2_DisplayPart(BlackImage);
  
}

void set_light(int light){
  
  
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", light);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 82, paint.GetWidth(), paint.GetHeight());
}

void set_temp(int temp){
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", temp);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 132, paint.GetWidth(), paint.GetHeight());
}

void set_background(){
  paint.SetWidth(64);
  paint.SetHeight(64);

  paint.Clear(UNCOLORED);
  paint.DrawLine(39, 0, 40, 32, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 0, 102, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(200);
  paint.Clear(UNCOLORED);
  paint.DrawHorizontalLine(0, 5, 200, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 16, 200, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(200);
  paint.Clear(UNCOLORED);
  paint.DrawHorizontalLine(0, 5, 200, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 72, 200, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(16);
  paint.SetHeight(150);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "To Do Today:", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 30, 150, paint.GetWidth(), paint.GetHeight());

  // plant health
  paint.SetWidth(16);
  paint.SetHeight(150);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "Plant Health:", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 86, 150, paint.GetWidth(), paint.GetHeight());

    //draw icons
  paint.SetWidth(40);
  paint.SetHeight(40);
  paint.Clear(UNCOLORED);
  paint.DrawRectangle(0, 0, 25, 35, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 155, 200, paint.GetWidth(), paint.GetHeight());

  draw_bitmap(&paint, thermometer_img, 114, 143, 40, 40, COLORED);
  draw_bitmap(&paint, sun_img, 114, 93, 40, 40, COLORED);
  draw_bitmap(&paint, humidity_img, 114, 43, 40, 40, COLORED);
  draw_bitmap(&paint, plant_img, 155, 190, 40, 40, COLORED);

  //plant units
  paint.SetWidth(20);
  paint.SetHeight(35);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "%", &Font20, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 52, paint.GetWidth(), paint.GetHeight());

  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "lux", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 89, paint.GetWidth(), paint.GetHeight());

  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, "C", &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 180, 152, paint.GetWidth(), paint.GetHeight());

  // bullet point
  paint.SetWidth(12);
  paint.SetHeight(12);
  paint.Clear(UNCOLORED);
  paint.DrawFilledCircle(4, 6, 2, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 46, 20, paint.GetWidth(), paint.GetHeight());
}

void draw_bitmap(Paint* paint, const unsigned char* bitmap, int x0, int y0, int width, int height, int colored) {
    paint->SetWidth(width);
    paint->SetHeight(height);
    paint->Clear(UNCOLORED);

    int byteWidth = (width + 7) / 8;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int byteIndex = y * byteWidth + (x / 8);
            uint8_t byte = pgm_read_byte(&bitmap[byteIndex]); // Read from PROGMEM safely

            // image2cpp exports MSB first → test (0x80 >> (x % 8))
            if (!(byte & (0x80 >> (x % 8)))) {
                paint->DrawPixel(x, y, colored);
            }
        }
    }

    epd.SetFrameMemory(paint->GetImage(), x0, y0, paint->GetWidth(), paint->GetHeight());
}


int DetectLight() {
  long luxVal = light.readLight();
  return luxVal;
}

float DetectTemp() {
  int chk = DHT.read11(DHT11_PIN); 

  if (chk == DHTLIB_OK) {
    return DHT.temperature; 
  } else {
    Serial.print("DHT read failed. Error code: ");
    Serial.println(chk);
    return -999;                 
  }
}

void addToDataTable(const String path, const int value, const int num_values) {
  File file = LittleFS.open(path, "r");

  String newContent = String(value) + "\n"; // Header

  if (file) {
    int index = 0;
    while (file.available() && index < num_values) {
      String line = file.readStringUntil('\n');
      line.trim();
      
      newContent += line + "\n";  // Keep other lines
      index++;
    }
    if (index < num_values) {
      newContent += "0\n"; // Append new value if there's space
      index++;
    }
    file.close();
  }

  // Overwrite file
  File writeFile = LittleFS.open(path, "w");
  if (!writeFile) {
    Serial.println("Failed to write to file.");
    return;
  }
  writeFile.print(newContent);
  writeFile.close();

}

void startWiFi(const char *wifissid, const char *wifipassword) { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  wifiMulti.addAP(wifissid, wifipassword);   // add Wi-Fi networks you want to connect to

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED && WiFi.softAPgetStationNum() < 1) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  if(WiFi.softAPgetStationNum() == 0) {      // If the ESP is connected to an AP
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());             // Tell us what network we're connected to
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  } else {                                   // If a station is connected to the ESP SoftAP
    Serial.print("Station connected to ESP8266 AP");
  }
  Serial.println("\r\n");
}

void startFileSystem() { // Start the LittleFS and list all contents
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting file system");
    return;
  }                             // Start the LittleFS
  Serial.println("File System started. Contents:");
  {
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {                      // List the file system contents
      String fileName = file.name();
      size_t fileSize = file.size();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
      file = root.openNextFile();
    }
    Serial.printf("\n");
  }
}

void startWebSocket() { // Start a WebSocket server
  webSocket.begin();                          // start the websocket server
  webSocket.onEvent(webSocketEvent);          // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName);                        // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

void startServer() { // Start a HTTP server with a file read handler

  server.on("/time", handleTime);

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
                                              // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

void handleTime() {
  server.send(200, "text/plain", getTimeString());
}

void handleNotFound(){ // if the requested file or page doesn't exist, return a 404 not found error
  if(!handleFileRead(server.uri())){          // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "calendar.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (LittleFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = LittleFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      String msg = (const char*)payload;
      //Serial.println("Received: " + msg);

      // Parse message: "YYYY-MM-DD: Note text"
      int sepIndex = msg.indexOf(':');
      if (sepIndex == -1) {
        Serial.println("Invalid format. Ignoring.");
        return;
      }

      String date = msg.substring(0, sepIndex);
      String note = msg.substring(sepIndex + 2);

      if (date.length() != 10 || note.length() == 0) {
        Serial.println("Invalid date or empty note.");
        return;
      }

      updateOrAddEntry(date, note);
      break;
  }
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}




