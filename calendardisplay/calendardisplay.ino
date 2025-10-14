#include <LittleFS.h>
#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "FS.h"
#include <WebSocketsServer.h>

#include "time.h"

#include <SPI.h>
#include "epd1in54_V2.h"
#include "epdpaint.h"
#include <stdio.h>
#include "icons.h"

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

int lastscreen = 0;  // will store last time screen was updated
int interval = 5*1000; // interval at which to update display (milliseconds)


WiFiMulti wifiMulti;       // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

WebServer server(80);       // Create a webserver object that listens for HTTP request on port 80 
WebSocketsServer webSocket(81);    // create a websocket server on port 81

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

  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  
  startOTA();                  // Start the OTA service
  
  startFileSystem();               // Start the SPIFFS and list all contents

  startWebSocket();            // Start a WebSocket server
  
  startMDNS();                 // Start the mDNS responder

  startServer();               // Start a HTTP server with a file read handler

    // Init and get time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  int state = 1;

  epd.HDirInit();
  epd.Clear();

  paint.SetRotate(ROTATE_270);

  set_background();
  set_time("12:34:56");
  set_date("1234-56-78");
  set_message("Setting up");
  set_moisture(12);
  set_light(34);
  set_temp(56);  

  draw_bitmap(&paint, faces[state], 100, 190, 40, 40, COLORED);

  epd.DisplayFrame();
  
}

void loop() {
  webSocket.loop();                           // constantly check for websocket events
  server.handleClient();                      // run the server
  ArduinoOTA.handle();                        // listen for OTA events
  checkSerialCommand();  

  if (millis() - lastscreen > interval) {  // if the time since the last update is bigger than the interval
    lastscreen = millis();                    // save the last time you updated the screen

    set_time(getTimeString());
    set_date(getDateString());
    set_message(chooseDateNote(getDateString()));

    draw_bitmap(&paint, faces[state], 100, 190, 40, 40, COLORED);

    epd.DisplayFrame();
  }
}

void startWiFi() { // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.softAP(ssid, password);             // Start the access point
  Serial.print("Access Point \"");
  Serial.print(ssid);
  Serial.println("\" started\r\n");

  wifiMulti.addAP("iBone", "12345678");   // add Wi-Fi networks you want to connect to

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

  server.on("/notes.txt", HTTP_GET, []() {
    if (LittleFS.exists("/notes.txt")) {
      File file = LittleFS.open("/notes.txt", "r");
      String fileContent;
      while(file.available()){
        fileContent += char(file.read());
      }
      file.close();
      server.send(200, "text/plain", fileContent);
    } else {
      server.send(404, "text/plain", "Notes file not found");
    }
  });

  server.on("/", handleRoot);

  server.on("/notes.txt", handleNotes);  // when a client requests the /notes.txt file, go to function 'handleNotes'

  server.on("/time", handleTime);

  server.onNotFound(handleNotFound);          // if someone requests any other file or page, go to function 'handleNotFound'
                                              // and check if the file exists

  server.begin();                             // start the HTTP server
  Serial.println("HTTP server started.");
}

void handleTime() {
  server.send(200, "text/plain", getTimeString());
}

void handleNotes() { // when a client requests the /notes.txt file
  if (LittleFS.exists("/notes.txt")) {       // check if the file exists
    File file = LittleFS.open("/notes.txt", "r"); // open the file
    String fileContent;
    while(file.available()){
      fileContent += char(file.read());
    }
    file.close();
    server.stream(file, "text/plain"); // send the content of the file to the client
  } else {
    server.send(404, "text/plain", "Notes file not found"); // if the file doesn't exist, return a 404 not found error
  }
}

void handleNotFound(){ // if the requested file or page doesn't exist, return a 404 not found error
  if(!handleFileRead(server.uri())){          // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

void handleRoot() { // when a client requests the root page ("/")
  if(!handleFileRead("/calendar.html"))         // send the index file to the client
    server.send(404, "text/plain", "404: File Not Found");
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
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

String chooseDateNote(const String date) {
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
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void set_time(String time){ // expects "HH:MM:SS"

  paint.SetWidth(16);
  paint.SetHeight(72);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, time.substring(0,5), &Font20, COLORED); // we cut off the seconds for display
  epd.SetFrameMemory(paint.GetImage(), 0, 72, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
}

void set_date(String date){ // expects "YYYY-MM-DD"
  // draw date
  paint.SetWidth(16);
  paint.SetHeight(112);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, date.substring(2), &Font20, COLORED); // we cut off the century for display
  epd.SetFrameMemory(paint.GetImage(), 0, 200, paint.GetWidth(), paint.GetHeight());
  //epd.DisplayPartFrame();
}

void set_message(String buf){
  int num_chars = buf.length();
  paint.SetWidth(32); 
  paint.SetHeight(180);
  paint.Clear(UNCOLORED);
  
  if(num_chars < 16){
    paint.DrawStringAt(0, 0, buf, &Font16, COLORED);
  } else {
    char line_1[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
    char line_2[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'}; 
    for(int i = 0; i < 15; i++){
      line_1[i] = buf[i];
    }
    
    for(int i = 15; i < num_chars; i++){
      line_2[i - 15] = buf[i];
    }

    paint.DrawStringAt(0, 0, line_1, &Font16, COLORED);
    paint.DrawStringAt(0, 14, line_2, &Font16, COLORED);
  }

  epd.SetFrameMemory(paint.GetImage(), 46, 200, paint.GetWidth(), paint.GetHeight());
}

void set_moisture(int moisture){
  paint.SetWidth(16);
  paint.SetHeight(20);
  paint.Clear(UNCOLORED);

  char buffer[2];
  sprintf(buffer, "%02d", moisture);
  paint.DrawStringAt(0, 0, buffer, &Font16, COLORED);
  epd.SetFrameMemory(paint.GetImage(), 165, 32, paint.GetWidth(), paint.GetHeight());
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






