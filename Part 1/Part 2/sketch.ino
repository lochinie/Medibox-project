#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define DHT_PIN 14
#define BUZZER 4
#define LDR_R_PIN 33
#define LDR_L_PIN 32

const int servoPin = 18;
double gammavalve = 0.75;  
Servo myservo;
int current_pos =0;
int pos = 0;
double T_offset = 30;
float min_angle = 0;
int D = 0;
int I = 0;

char tempAr[6];
char sensorLarr[6];
char sensorRarr[6];

WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

DHTesp dhtSensor;

bool isScheduledON = false;
unsigned long scheduledOnTime;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupWifi();

  setupMqtt();

  dhtSensor.setup(DHT_PIN,DHTesp::DHT22);

  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);
  
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  pinMode(LDR_R_PIN, INPUT);
  pinMode(LDR_L_PIN, INPUT);
  myservo.attach(servoPin, 500, 2400);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();

  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("ENTC-ADMIN-TEMP",tempAr);
  mqttClient.publish("ENTC-ADMIN-LDR-L", sensorLarr);
  mqttClient.publish("ENTC-ADMIN-LDR-R", sensorRarr);
  checkSchedule();

  if(min_angle>current_pos){
    for (pos = current_pos; pos <= min_angle; pos += 1) {
    myservo.write(pos);
    delay(10);}
    current_pos= min_angle;
  }
  else if(min_angle<current_pos){
  for (pos = current_pos; pos >= min_angle; pos -= 1) {
    myservo.write(pos);
    delay(10);}
    current_pos= min_angle;
  }  
}


void setupWifi(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST","");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("wiFi Connected");
  Serial.println("IP address :");
  Serial.println(WiFi.localIP());
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(recieveCallback);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.print("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-12345645454")){
      Serial.println("Connected");
      mqttClient.subscribe("ENTC-ADMIN-MAIN-ON-OFF");
      mqttClient.subscribe("ENTC-ADMIN-SCH-ON");
      mqttClient.subscribe("ENTC-ADMIN-ANGLE");
      mqttClient.subscribe("ENTC-ADMIN-CONTROL");
    }
    else{
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr,6);
}

void buzzerOn(bool on){
  if(on){
    tone(BUZZER,256);
  }
  else{
    noTone(BUZZER);
  }
}


void recieveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for(int i=0;i < length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println();

  if(strcmp(topic,"ENTC-ADMIN-MAIN-ON-OFF") == 0){
    buzzerOn(payloadCharAr[0]=='1');
  }
  else if(strcmp(topic,"ENTC-ADMIN-SCH-ON") == 0){
    if(payloadCharAr[0] =='N'){
      isScheduledON = false;
    }
    else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }
  else if(strcmp(topic,"ENTC-ADMIN-Angle")==0){
    T_offset = atof(payloadCharAr);
  }
  else if(strcmp(topic,"ENTC-ADMIN-Control") == 0){
    gammavalve = atof(payloadCharAr);
  }
}

unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();
}

void checkSchedule(){
  if(isScheduledON){
    unsigned long currentTime = getTime();
    if(currentTime > scheduledOnTime){
      tone(BUZZER,256);
      isScheduledON = false;
      mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP","1");
      mqttClient.publish("ENTC-ADMIN-SCH-ESP-ON","0");
      Serial.println("Schedule ON");
    }
  }
}

void motorAngle(){
  int sensorL =analogRead(LDR_L_PIN);
  int sensorR =analogRead(LDR_R_PIN);
  String(sensorL).toCharArray(sensorLarr,6);
  String(sensorR).toCharArray(sensorRarr,6);

  double D = (sensorR>sensorL) ? 0.5 : 1.5;
  double min_VAL = min(sensorR,sensorL);
  double I = 1-(min_VAL-32)/4031;
  min_angle = min(180.0,((T_offset*D)+(180-T_offset)*I*gammavalve));
}