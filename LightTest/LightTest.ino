#include <Wire.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"
#include <dht.h>

#define DHT11_PIN 3
#define SOIL_PIN 2

#define AL_ADDR 0x10 // I2C Address

SparkFun_Ambient_Light light(AL_ADDR);

dht DHT;

// Possible values: .125(1/8), .25(1/4), 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark
// glass.
float gain = .125;

// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light. 
int time = 100;

int button_state;

void setup(){

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
  light.setIntegTime(time);

  Serial.println("Reading settings..."); 
  Serial.print("Gain: ");
  float gainVal = light.readGain();
  Serial.print(gainVal, 3); 
  Serial.print(" Integration Time: ");
  int timeVal = light.readIntegTime();
  Serial.println(timeVal);

}

void loop(){
  Serial.print("Ambient Light Reading: ");
  Serial.print(DetectLight());
  Serial.println(" Lux");
  Serial.print("Temperature = ");
  Serial.println(DetectTemp());
  delay(1000);
  int sensor_value = analogRead(SOIL_PIN);
  Serial.println(sensor_value);
  
}

long DetectLight() {
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
