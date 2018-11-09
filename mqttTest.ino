#include <ESP8266WiFi.h>
#include <MQTTClient.h>
#include <Wire.h>
#include <BH1750.h>
#include "DHT.h"
#include <RCSwitch.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


#define DHTPIN D7
#define DHTPIN2 D9
#define DHTTYPE DHT22
#define SENSOR_PIN A0

 
const char* ssid     = "conni1";
const char* password = "project";

 
WiFiClient WiFiclient;
MQTTClient client;
DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
BH1750 lightMeter;
RCSwitch mySwitch = RCSwitch();
 
unsigned long lastMillis = 0;

//ultraschall sensor HC-SR04
const int trigPin = 14;  //D5
const int echoPin = 12;  //D6

//distance controll
long duration;
int distance;


//soil moisture content
int sensorValue = 0; 
int wet = 500;
int dry = 700;
String stateSoilMoistureContent;

//433MHz
char lightState;

//pump
int pumpPin = 5;
int pumpDelay = 2000;
int pumpControll = 0;

//fan controll
int fanPin = 4;

//NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "de.pool.ntp.org", 7200, 60000); //Default 3600 -> 7200 für +01:00:00
String lightStart = "08:00:00";
String lightStop = "20:00:00";


//----------------------------------------------------------------Setup---------------------

 
void setup() {
 Serial.begin(9600);
 delay(10);
 Serial.println();
 Serial.println();
 Serial.print("Connecting to ");
 Serial.println(ssid);
 
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
 }
 
 Serial.println("");
 Serial.println("WiFi connected");  
 Serial.println("IP address: ");
 Serial.println(WiFi.localIP());
 delay(2000);
 
 Serial.print("connecting to MQTT broker...");
 client.begin("broker.shiftr.io", WiFiclient);
 client.onMessage(messageReceived);
 connect();

   //DHT22
  Serial.println("DHT22_test");
  dht.begin();
  dht2.begin();

  //BH1750_GY-30
  Wire.begin(D4, D3);
  Serial.println(F("BH1750_test"));
  lightMeter.begin();

  //HC-SR04
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //433MHz
  mySwitch.enableTransmit(15);

  //Fan Controll
  pinMode(fanPin, OUTPUT);

  //Pump ControlL
  pinMode(pumpPin, OUTPUT);

  //NTP Client
  timeClient.begin();
}


//------------------------------------------------------------------------------------------

 
void connect() {
 while (!client.connect("nodeMCU", "7ab8d043", "4acf899e28d7740c")) {
   Serial.print(".");
 }
 
 Serial.println("\nconnected!");
 client.subscribe("/smartGreenhouse/Sollwerte/LightStartTime");
 client.subscribe("/smartGreenhouse/Sollwerte/LightStopTime");
 client.subscribe("/smartGreenhouse/Sollwerte/LightSwitch");
 client.subscribe("/smartGreenhouse/Sollwerte/PumpSwitch");
 client.subscribe("/smartGreenhouse/Sollwerte/DryTargetValue");
 client.subscribe("/smartGreenhouse/Sollwerte/HumidTargetValue");
 client.subscribe("/smartGreenhouse/Sollwerte/FanSwitch");
}


//----------------------------------------------------------------loop----------------------


void loop() {
  mqttClient();
  delay(20);
  timeClient.update();
  delay(20);
  tempHumiditySensor();
  delay(20);
  lightSensor();
  delay(20);
  ultraschallSensor();
  delay(20);
  soilmoisturecontentSensor();
  delay(20);
  fanControll();
  delay(20);
  Serial.print("lightStart: ");
  Serial.println(lightStart);
  Serial.print("wet");
  Serial.println(wet);
}


//------------------------------------------------------------------------------------------


void mqttClient(){
  float h = dht.readHumidity();
  float h2 = dht2.readHumidity();
  float t = dht.readTemperature();
  float t2 = dht2.readTemperature();
  uint16_t lux = lightMeter.readLightLevel();
  
   client.loop();
 if(!client.connected()) {
   connect();
 }
 
 if(millis() - lastMillis > 1000) {
   lastMillis = millis();
   client.publish("/smartGreenhouse/AirHumidityOut", String((String)h+"%"));
   client.publish("/smartGreenhouse/AirHumidityIn", String((String)h2+"%"));
   
   client.publish("/smartGreenhouse/TemperatureOut", String((String)t+"°C"));
   client.publish("/smartGreenhouse/TemperatureIn", String((String)t2+"°C"));
   
   client.publish("/smartGreenhouse/LightLevel", String((String)lux+" Lumen"));
   client.publish("/smartGreenhouse/pumpDelay", (String)pumpDelay);
   client.publish("/smartGreenhouse/SoilHumidity", String((String)(100-sensorValue*100/1024)+"%"));
 }
}


//void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
void messageReceived(String &topic, String &payload) {
  Serial.print("incoming: ");
  Serial.print(topic);
  Serial.print(" - ");
  Serial.print(payload);
  Serial.println();
  if(topic=="/smartGreenhouse/Sollwerte/LightStartTime"){
      String temp = payload;
      temp.replace(" Uhr","");
      lightStart = temp;//.replace("Uhr","");
  }else if(topic=="/smartGreenhouse/Sollwerte/LightStopTime"){
      String temp = payload;
      temp.replace(" Uhr","");
      lightStop = temp;//.replace("Uhr","");
  }else if(topic=="/smartGreenhouse/Sollwerte/LightSwitch"){
      if(payload=="HIGH"){
         mySwitch.sendTriState("FFF0FF0F0FFF");
      }else if(payload=="LOW"){
         mySwitch.sendTriState("FFF0FF0F0FF0");
      }
      
  }else if(topic=="/smartGreenhouse/Sollwerte/PumpSwitch"){
      if(payload=="HIGH"){
        digitalWrite(pumpPin, HIGH);
      }else if(payload=="LOW"){
        digitalWrite(pumpPin, LOW);
      } 
  }else if(topic=="/smartGreenhouse/Sollwerte/FanSwitch"){
      if(payload=="HIGH"){
        digitalWrite(fanPin, HIGH);
      }else if(payload=="LOW"){
        digitalWrite(fanPin, LOW);
      }       
      
  }else if(topic=="/smartGreenhouse/Sollwerte/DryTargetValue"){
      String temp = payload;
      temp.replace("%","");
      dry=temp.toInt()*1024/100; // % abschneiden und zurück in Digitalwert umrechnen
  }else if(topic=="/smartGreenhouse/Sollwerte/HumidTargetValue"){
      String temp = payload;
      temp.replace("%","");
      wet=temp.toInt()*1024/100; // % abschneiden und zurück in Digitalwert umrechnen
  }
}



void tempHumiditySensor() {
  delay(2000);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();
  
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");
  Serial.print("Humidity2: ");
  Serial.print(h2);
  Serial.print(" %\t");
  Serial.print("Temperature2: ");
  Serial.print(t2);
  Serial.println(" *C ");
}



void lightSensor() {
  delay(1000);
  //light sensivity
  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  //time via NTP client
  String timeStamp = timeClient.getFormattedTime();
  Serial.print("Time: ");
  Serial.println(timeStamp);
  //light controll
  if(lightStart < lightStop){
     if(timeStamp >= lightStart && timeStamp < lightStop){
        mySwitch.sendTriState("FFF0FF0F0FFF");
     }else if((timeStamp < lightStart && timeStamp >= "00:00:00") || (timeStamp <= "23:59:59" && timeStamp >= lightStop)){
        mySwitch.sendTriState("FFF0FF0F0FF0");
     }else{
        mySwitch.sendTriState("FFF0FF0F0FF0");
        Serial.println("Light Controll fail 1");
     } 
  }else if(lightStart > lightStop){
     if((timeStamp >= lightStart && timeStamp <= "23:59:59") || (timeStamp >= "00:00:00" && timeStamp < lightStop)){
       mySwitch.sendTriState("FFF0FF0F0FFF");
     }else if(timeStamp < lightStart && timeStamp >= lightStop){
       mySwitch.sendTriState("FFF0FF0F0FF0");
     }else{
       mySwitch.sendTriState("FFF0FF0F0FF0");
       Serial.println("Light Controll fail 2");
     }
  }else{
     mySwitch.sendTriState("FFF0FF0F0FF0");
     Serial.println("Light Controll fail 3");
  }
}



void ultraschallSensor() {

  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculating the distance
  distance = duration * 0.034 / 2;
  // Prints the distance on the Serial Monitor
  Serial.print("Distance: ");
  Serial.println(distance);
  delay(2000);
}



String soilmoisturecontentSensor(){
  sensorValue = getSensorValue();
  Serial.print("Bodenfeuchtigkeit: ");
  Serial.println(sensorValue);
  delay(20);
    if(sensorValue >= 0 && sensorValue <wet){
       Serial.println("zu nass");
       stateSoilMoistureContent = "zu Nass";
       digitalWrite(pumpPin, LOW);
       pumpControll = pumpControll - 1;
       if(pumpControll <= -10){
         pumpDelay -= 50;
         pumpControll = 0;
       }
    } else if(sensorValue >= wet && sensorValue <=dry){
        Serial.println("nicht zu nass und auch nicht zu trocken");
        stateSoilMoistureContent = "optimal";
        digitalWrite(pumpPin, LOW);
        pumpControll--;
    } else if(sensorValue > dry){ 
        Serial.println("zu trocken");  
        stateSoilMoistureContent = "zu trocken";      
        digitalWrite(pumpPin, HIGH);
        delay(pumpDelay);
        digitalWrite(pumpPin, LOW);
        pumpControll++;
        if(pumpControll >= 10){
          pumpDelay += 50;
          pumpControll = 0;
          }
    }
    if(pumpDelay <=50){
      pumpDelay +=50;
    }
    Serial.print("pumpDelay: ");
    Serial.print(pumpDelay);
    Serial.print("  pumpControll: ");
    Serial.print(pumpControll); 
    Serial.println(" ");
  return stateSoilMoistureContent;
}

//value soil moisture content
int getSensorValue(){
  return analogRead(SENSOR_PIN);
} 



//fan controll
void fanControll(){
  
  digitalWrite(fanPin, HIGH);
  delay(1000);
  digitalWrite(fanPin, LOW);
}
