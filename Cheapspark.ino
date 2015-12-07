// WIFI (ESSENTIAL)
#include <espduino.h>
#define MYSSID "tim"
#define MYPASS "password"
ESP esp(&Serial, 4);                          //Uses digital Pin 4 for sleep mode
boolean wifiConnected = false;


// MQTT (ESSENTIAL)
#include <mqtt.h>
#define BROKERIP "192.168.0.213"
#define MQTTCLIENT "cheapspark0"
#define MQTT_TOPIC_DEBUG "/cheapspark0/debug"        //Debug info
MQTT mqtt(&esp);


// DHT
#include <dht.h>
#define DHT_PIN 5
#define MQTT_TOPIC_HUMI "/cheapspark0/dht22/humi"   //output
#define MQTT_TOPIC_TEMP "/cheapspark0/dht22/temp"   //output
#define DHT_SEND_INTERVAL 15000      // Send data each 15 seconds
unsigned long nextDHTPub = DHT_SEND_INTERVAL;
dht DHT;


// Relay
#define REL1_PIN 9
#define REL2_PIN 10
#define REL3_PIN 11
#define REL4_PIN 12
#define MQTT_TOPIC_RELAYS "/cheapspark0/relays"       //Input send r1, r2, r3 or r4


// OneWireTemp
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 6      // Data wire is plugged into pin 6 on the Arduino
#define MQTT_TOPIC_TEMPONEWIRE "/cheapspark0/onewire/temp"   //output
#define ONEWIRETEMP_SEND_INTERVAL 20000      // Send data each 15 seconds
unsigned long nextOneWirePub = ONEWIRETEMP_SEND_INTERVAL;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Switch
#define SWITCH_PIN 0              //Uses analog pin 0
#define SWITCH_PIN_D A0           //Needed because pin 0 is also accessed digital
#define MQTT_TOPIC_SWITCH "/cheapspark0/switch"       //Input send r1, r2, r3 or r4
boolean switchstate = false;


// Nano Led for debugging
#define LED_PIN 13                //Digital pin 13 on Nano is connected to the onboard led
boolean ledstate = false;


// Common Loop Timer
unsigned long now = 0;


///////////////////////////////////
///////////////////////////////////
// Main Setup                    //
///////////////////////////////////
///////////////////////////////////
void setup() 
{
  // Switch Setup
  switchstate = false;
  pinMode(SWITCH_PIN_D,INPUT);
  digitalWrite(SWITCH_PIN_D,HIGH);
  
  // Relay Setup
  pinMode(REL1_PIN,OUTPUT);
  pinMode(REL2_PIN,OUTPUT);
  pinMode(REL3_PIN,OUTPUT);
  pinMode(REL4_PIN,OUTPUT);
  digitalWrite(REL1_PIN,HIGH);
  digitalWrite(REL2_PIN,HIGH);
  digitalWrite(REL3_PIN,HIGH);
  digitalWrite(REL4_PIN,HIGH);
  
  //OneWireTemp Setup
  sensors.begin();

  // Wifi Setup
  delay(5000);      //LAGER PROBEREN
  Serial.begin(19200);
  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while(!esp.ready());

  //MQTT Setup
  if(!mqtt.begin(MQTTCLIENT, "", "", 30, 1)) while(1);      //Wait till MQTT begins
  mqtt.lwt(MQTT_TOPIC_DEBUG, MQTTCLIENT " offline (lwt)", 0, 0);  //setup mqtt lwt
  mqtt.connectedCb.attach(&mqttConnected);                  //setup mqtt events
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);

  // Wifi Finish Setup
  esp.wifiCb.attach(&wifiCb);
  esp.wifiConnect(MYSSID,MYPASS);
}


///////////////////////////////////
///////////////////////////////////
// Main Loop                     //
///////////////////////////////////
///////////////////////////////////
void loop()
{
  esp.process();  
  if(wifiConnected)
  {
    now = millis();
    
    // Switch
    int switchval = analogRead(SWITCH_PIN);
    if ((switchval>500) && (switchstate == false))
    {
      mqtt.publish(MQTT_TOPIC_SWITCH,"Switch1On");
      switchstate = !switchstate;
    }
    if ((switchval<500) && (switchstate == true))
    {
      mqtt.publish(MQTT_TOPIC_SWITCH,"Switch1Off");
      switchstate = !switchstate;
    }
    
    // DHT publishing loop
    if (now >= nextDHTPub)                  //COULD GO WRONG AFTER 49 DAYS
    { 
      nextDHTPub = now + DHT_SEND_INTERVAL;
      
      int chk = DHT.read22(DHT_PIN);
      float humid = DHT.humidity;
      float tempe = DHT.temperature;
      
      char chHumid[10];
      char chTempe[10];
      dtostrf(humid,1,2,chHumid);
      dtostrf(tempe,1,2,chTempe);
      
      mqtt.publish(MQTT_TOPIC_HUMI,chHumid);
      mqtt.publish(MQTT_TOPIC_TEMP,chTempe);
    }
    
    // OneWireTemp publishing loop
    if (now >= nextOneWirePub)                  //COULD GO WRONG AFTER 49 DAYS
    { 
      nextOneWirePub = now + ONEWIRETEMP_SEND_INTERVAL;
      
      sensors.requestTemperatures(); // Send the command to get temperatures
      
      float tempOW = sensors.getTempCByIndex(0); // returns -127 when nothing is connected!!!!
      
      char chTempOW[5];      
      dtostrf(tempOW,1,2,chTempOW);

      mqtt.publish(MQTT_TOPIC_TEMPONEWIRE,chTempOW);
    }
  }
}


///////////////////////////////////
// WIFI FUNCTIONS                //
///////////////////////////////////
void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {        //WIFI CONNECTED
      mqtt.connect(BROKERIP, 1883, false);
      wifiConnected = true;
    } else {
      wifiConnected = false;
      mqtt.disconnect();
    }
  }
}


///////////////////////////////////
// MQTT FUNCTIONS                //
///////////////////////////////////
void mqttConnected(void* response)
{
  delay(500);  // Weg doen?
  mqtt.publish(MQTT_TOPIC_DEBUG, MQTTCLIENT " online");
  
  // Relay - Subscribe to relay topic
  mqtt.subscribe(MQTT_TOPIC_RELAYS);
}

void mqttDisconnected(void* response)
{

}

void mqttData(void* response)          // New MQTT msgs recieved
{
  RESPONSE res(response);
  char buffer[4];
  String topic = res.popString();
  String data = res.popString();
  data.toCharArray(buffer,4);
  
  // Nano Led for debugging
  ledstate = !ledstate;
  digitalWrite(LED_PIN, ledstate);
  
  // Debug info
  //mqtt.publish(MQTT_TOPIC_DEBUG, MQTTCLIENT " recieved a msg:" );
  
  // Relay
  if (topic.compareTo(MQTT_TOPIC_RELAYS) == 0)        //Relay Command received?   (topic == MQTT_TOPIC_RELAYS) => Should work fast
  {    
    if (strcmp(buffer,"r1") == 0) {
      digitalWrite(REL1_PIN,LOW);
      delay(50);
      digitalWrite(REL1_PIN,HIGH);
    } else if (strcmp(buffer,"r2") == 0) {
      digitalWrite(REL2_PIN,LOW);
      delay(50);
      digitalWrite(REL2_PIN,HIGH);
    } else if (strcmp(buffer,"r3") == 0) {
      digitalWrite(REL3_PIN,LOW);
      delay(50);
      digitalWrite(REL3_PIN,HIGH);
    } else if (strcmp(buffer,"r4") == 0) {
      digitalWrite(REL4_PIN,LOW);
      delay(50);
      digitalWrite(REL4_PIN,HIGH);
    }
  }
}

void mqttPublished(void* response)        //runs when publish is a success
{

}
