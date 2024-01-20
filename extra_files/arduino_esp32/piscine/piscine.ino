/* -------------------------------------------------------------------------------------------------------- */
/* IOT, WaterBnB Project - Flask server                                                                     */
/* -------------------------------------------------------------------------------------------------------- */
/* 16 Jan 2024, Université Côte d'Azur,                                                                     */
/* Rafael Baptista.                                                                                         */
/* -------------------------------------------------------------------------------------------------------- */
/* Dashboard :                                                                                              */
/* https://charts.mongodb.com/charts-project-0-uqrbc/public/dashboards/65a9d5c3-f981-4dcc-8564-91b2db846213 */
/* -------------------------------------------------------------------------------------------------------- */
/* Web Portal :                                                                                             */
/* https://waterbnb-22301479.onrender.com                                                                   */
/* -------------------------------------------------------------------------------------------------------- */
/* Github :                                                                                                 */
/* https://github.com/rafaelbenaion/WaterBnB_22301479                                                       */
/* -------------------------------------------------------------------------------------------------------- */

/* 
 * Auteur : G.Menez
 * Fichier : http_as_serverasync/http_as_serverasync.ino 
*/


/* -------------------------------------------------------------------------------------------------------- */
/* Gloabal variables                                                                                        */
/* -------------------------------------------------------------------------------------------------------- */

const int    cooler         = 19;                                                // GREEN LED D19 sur la carte
const int    heater         = 21;                                                  // RED LED D21 sur la carte
      int    high_temp_s    = 28;
      int    low_temp_s     = 25;
const int    seuil[5]       = {25, 26, 27, 28, 29};             // 4 temp seuils, seuil[0] : SB, seuil[3] : SH
const int    rcv[5]         = {0, 64, 127, 191, 255};                                      // Fan speed states
const int    fireTempSeuil  = 25;                                           // Fire Temperature Seuil Constant
const int    fireLightSeuil = 2000;                                               // Fire Light Seuil Constant
      int    fanSpeed       = 0;                                                          // Fan current speed
      int    light          = 0;                                                        // Light sensor result
      bool   fire           = false;                                                          // Fire detected
      String coolerState    = "OFF";                                                          // AC json state
      String heaterState    = "OFF";                                                      // Heater json state
      String fanState       = "HALT";                                                        // Fan json state                                
      float  temp           = 0;                                                  // Temperature sensor result  
      bool   repporting     = false;                              
      String target_ip      = "";
      String target_port    = "";
      String target_sp      = "";
      float  temperatureMQ  = 0;
      float  lightMQ        = 0;
      bool   hotspot        = false;                                            // Variable for hotspot signal
      float  highiest_temp  = 0;                             // Variable for storing the highiest temp in 10km
      float  my_latitude    = 43.57622;                                           // Variable for ESP latitude
      float  my_longitude   = 7.11303;                                           // Variable for ESP longitude
      int    counterFire    = 0;                                        // Delay for RED light refused request
      bool   occupied       = false;                                           // Variabme for the pool status

/* -------------------------------------------------------------------------------------------------------- */
/* Import required libraries                                                                                */
/* -------------------------------------------------------------------------------------------------------- */

#include <ArduinoOTA.h>
#include "ArduinoJson.h"
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "wifi_utils.h"
#include "sensors.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "SPIFFS.h"
#include "HTTPClient.h"
#include <PubSubClient.h>
#include <Wire.h>
#include "routes.h"

#define USE_SERIAL Serial

void setup_OTA(); // from ota.ino

/*===== ESP GPIO configuration ==============*/
/* ---- LED         ----*/
const int LEDpin = 19; // LED will use GPIO pin 19
/* ---- Light       ----*/
const int LightPin = A5; // Read analog input on ADC1_CHANNEL_5 (GPIO 33)
/* ---- Temperature ----*/
OneWire oneWire(23); // Pour utiliser une entite oneWire sur le port 23
DallasTemperature TempSensor(&oneWire) ; // Cette entite est utilisee par le capteur de temperature

/* -------------------------------------------------------------------------------------------------------- */
/* ESP Status                                                                                               */
/* -------------------------------------------------------------------------------------------------------- */

// Ces variables permettent d'avoir une representation
// interne au programme du statut "electrique" de l'objet.
// Car on ne peut pas "interroger" une GPIO pour lui demander !

String LEDState = "off";
String last_temp;
String last_light;
String high_temp;
String low_temp;
String where;
String uptime;
String mac_address;
String ip_address;
String wifi_ssid;
String heater_stat;
String cooler_stat;

/* -------------------------------------------------------------------------------------------------------- */
/* Time period delay                                                                                        */
/* -------------------------------------------------------------------------------------------------------- */

// Set timer 
unsigned long loop_period = 10L * 10000;                                              /* =>  10000ms : 10 s */

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Thresholds
short int Light_threshold = 250; // Less => night, more => day

/*=======================================================*/
/* Delay can be non compatible with OTA handle or with async !!!! 
 *  because ESP32 pauses your program during the delay(). 
 *  If next OTA request is generated while Arduino is paused waiting for 
 *  the delay() to pass, your program will miss that request.
 *   => Use a non-blocking method, using mills() and a delta to decide to read or not. eg */

void DoSmtg(int delai){
  static uint32_t tick = 0;
  if ( millis() - tick < delai) { 
    return; 
  }
  else {                                                                   /* Do stuff here every 10 second */ 
    USE_SERIAL.print("deja 10 secondes écoulées ! \n");
    tick = millis();
  
    last_temp   = get_temperature(TempSensor);
    last_light  = get_light(LightPin);
    high_temp   = String(high_temp_s);
    low_temp    = String(low_temp_s);
    where       = "Sophia Antipolis, France.";
    uptime      = getUptimeSeconds();
    mac_address = get_mac_address();
    ip_address  = get_ip_address();
    wifi_ssid   = get_wifi_ssid();
    heater_stat = heaterState;
    cooler_stat = coolerState;
  } 
}

/* -------------------------------------------------------------------------------------------------------- */
/* MQTT                                                                                                     */
/* -------------------------------------------------------------------------------------------------------- */

#define MQTT_HOST IPAddress(192, 168, 1, XXX)

/*===== MQTT broker/server ========*/
//const char* mqtt_server = "192.168.1.101"; 
//const char* mqtt_server = "public.cloud.shiftr.io"; // Failed in 2021
// need login and passwd (public,public) mqtt://public:public@public.cloud.shiftr.io
//const char* mqtt_server = "broker.hivemq.com"; // anynomous Ok in 2021 
const char* mqtt_server = "test.mosquitto.org"; // anynomous Ok in 2021
//const char* mqtt_server = "mqtt.eclipseprojects.io"; // anynomous Ok in 2021
/*===== MQTT TOPICS ===============*/
#define TOPIC_TEMP "uca/iot/piscine"
#define TOPIC_LED  "uca/iot/piscine/22301479"



/*===== ESP is a MQTT Client =======*/
WiFiClient espClient;           // Wifi 
PubSubClient mqttclient(espClient); // MQTT client
String hostname = "Mon petit objet ESP32";

#define USE_SERIAL Serial

/* -------------------------------------------------------------------------------------------------------- */
/* Temp Sensor                                                                                              */
/* -------------------------------------------------------------------------------------------------------- */

//#include "OneWire.h"                                                                   // Temperature Sensor
//#include "DallasTemperature.h"

//OneWire           oneWire(23);                            // Pour utiliser une entite oneWire sur le port 23
DallasTemperature tempSensor(&oneWire);                            

/* -------------------------------------------------------------------------------------------------------- */
/* JSON                                                                                                     */
/* -------------------------------------------------------------------------------------------------------- */

//#include <ArduinoJson.h>

/* -------------------------------------------------------------------------------------------------------- */
/* Fan test                                                                                                 */
/* -------------------------------------------------------------------------------------------------------- */

int  numberKeyPresses = 0;
void IRAM_ATTR isr() {                                                                    // Interrupt Handler
  numberKeyPresses++;
}
                                                                                                            
/* -------------------------------------------------------------------------------------------------------- */
/* Led Strip                                                                                                */
/* -------------------------------------------------------------------------------------------------------- */

#include <Adafruit_NeoPixel.h>                                                                    // LED Strip
#define  PIN 13                                                                                   // LED Strip
#define  NUMLEDS 5

Adafruit_NeoPixel strip(NUMLEDS, PIN, NEO_GRB + NEO_KHZ800);

/* -------------------------------------------------------------------------------------------------------- */
/* Setup                                                                                                    */
/* -------------------------------------------------------------------------------------------------------- */

void setup(){ 
  
  /* Serial connection -----------------------*/
  USE_SERIAL.begin(9600);
  while(!USE_SERIAL); //wait for a serial connection  

  /* WiFi connection  -----------------------*/
  String hostname = "Mon petit objet ESP32";
  wifi_connect_multi(hostname);               
  
  /* WiFi status     --------------------------*/
  if (WiFi.status() == WL_CONNECTED){
    USE_SERIAL.print("\nWiFi connected : yes ! \n"); 
    wifi_printstatus(0);  
  } 
  else {
    USE_SERIAL.print("\nWiFi connected : no ! \n"); 
    //  ESP.restart();
  }

  // Init OTA
  //setup_OTA();
    
  // Initialize the LED 
  setup_led(LEDpin, OUTPUT, LOW);
  
  // Init temperature sensor 
  //TempSensor.begin();

  // Initialize SPIFFS
  SPIFFS.begin(true);

  // Setup routes of the ESP Web server
  setup_http_routes(&server);
  
  // Start ESP Web server
  server.begin();


    /* ---------------------------------------------------------------------------------------------------- */
    /* Led Setup                                                                                            */
    /* ---------------------------------------------------------------------------------------------------- */

    pinMode(cooler, OUTPUT);                                            // Initialize digital pin as an OUTPUT
    pinMode(heater, OUTPUT);                          
    pinMode(2, OUTPUT);                                           // Initialize GPIO 2 as an output Fire alert

    /* ---------------------------------------------------------------------------------------------------- */
    /* TempSensor Setup                                                                                     */
    /* ---------------------------------------------------------------------------------------------------- */
  
    tempSensor.begin();                                                         // Init capteur de temperature

    /* ---------------------------------------------------------------------------------------------------- */
    /* Fan Setup                                                                                            */
    /* ---------------------------------------------------------------------------------------------------- */
  
    Serial.println  ("\nFan speed test using PWM !");                               // Interrupt configuration
    pinMode         (26, INPUT_PULLUP);                                                           // broche 26
    attachInterrupt (26, isr, FALLING);                                                             // handler
    
    ledcAttachPin (27, 0);                                                     // Pin 27 FAN PWM configuration
    ledcSetup     (0, 25000, 8);
    ledcWrite     (0,255);

    /* ---------------------------------------------------------------------------------------------------- */
    /* MQTT                                                                                                 */
    /* ---------------------------------------------------------------------------------------------------- */
  

    // set server of our MQTT client
    mqttclient.setServer(mqtt_server, 1883);
    // set callback when publishes arrive for the subscribed topic
    mqttclient.setCallback(mqtt_pubcallback); 
    mqttclient.setBufferSize(1024);
}

/* -------------------------------------------------------------------------------------------------------- */
/* Callback - Listening to the subscribed topic                                                             */
/* -------------------------------------------------------------------------------------------------------- */

void mqtt_pubcallback(char* topic, 
                      byte* payloadMQ, 
                      unsigned int length) {
  /* 
   * Callback when a message is published on a subscribed topic.
   */
  USE_SERIAL.print("Message arrived on topic : ");
  USE_SERIAL.println(topic);
  USE_SERIAL.print("=> ");

  // Byte list (of the payloadMQ) to String and print to Serial
  String message;
  for (int i = 0; i < length; i++) {
    //USE_SERIAL.print((char)payloadMQ[i]);
    message += (char)payloadMQ[i];
  }
  
  USE_SERIAL.println(message);

/* -------------------------------------------------------------------------------------------------------- */
/* Refused request MQTT                                                                                     */
/* -------------------------------------------------------------------------------------------------------- */

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (String(topic) == TOPIC_LED) {
    USE_SERIAL.print("-------------------- request to unlock --------------------- ");
    if (message == "ERROR404") {
      USE_SERIAL.println("Failed !");
      fire = true;
    }
  }

  if (error) {
    USE_SERIAL.print(F("deserializeJson() failed: "));
    USE_SERIAL.println(error.f_str());
    return;
  }

  /* ------------------------------------------------------------------------------------------------------ */
  /* HOTSPOT check                                                                                          */
  /* ------------------------------------------------------------------------------------------------------ */

  float temperature_recieved = doc["status"]["temperature"];
  USE_SERIAL.print("Temperature revieved: ");
  USE_SERIAL.println(temperature_recieved);

  String id_recieved         = doc["info"]["ident"];
  float  latitude_recieved   = doc["location"]["gps"]["lat"];
  float  longitude_recieved  = doc["location"]["gps"]["lon"];
  float  distance_from_point = distance(my_latitude, my_longitude,latitude_recieved,longitude_recieved);

  USE_SERIAL.print("Ident: ");
  USE_SERIAL.println(id_recieved);

  USE_SERIAL.print("Distance: ");
  USE_SERIAL.println(distance_from_point);

  if(distance_from_point < 10 && id_recieved != "P_22301479"){
    USE_SERIAL.print("Message should be taken into account.");
    
    if(temperature_recieved > highiest_temp){
      highiest_temp = temperature_recieved;
    }
  }

  if(temp > highiest_temp){
      hotspot = true;
  }else{
      hotspot = false;
  }
  
  if (String(topic) == TOPIC_LED) {
    USE_SERIAL.print("so ... changing output to ");
    if (message == "on") {
      USE_SERIAL.println("on");
      set_LED(HIGH);
    }
    else if (message == "off") {
      USE_SERIAL.println("off");
      set_LED(LOW);
    }
  }
}

/* -------------------------------------------------------------------------------------------------------- */
/* MQTT subscribe                                                                                           */
/* -------------------------------------------------------------------------------------------------------- */

void mqtt_subscribe_mytopics() {
  /*
   * Subscribe to MQTT topics 
   * There is no way on checking the subscriptions from a client. 
   * But you can also subscribe WHENEVER you connect. 
   * Then it is guaranteed that all subscriptions are existing.
   * => If the client is already connected then we have already subscribe
   * since connection and subscriptions go together 
   */
  // Checks whether the client is connected to the MQTT server
  while (!mqttclient.connected()) { // Loop until we're reconnected
    USE_SERIAL.print("Attempting MQTT connection...");
    
    // Attempt to connect => https://pubsubclient.knolleary.net/api
    
    // Create a client ID from MAC address ..
    String mqttclientId = "ESP32-Rafael";
    mqttclientId += WiFi.macAddress(); // if we need random : String(random(0xffff), HEX);
    if (mqttclient.connect(mqttclientId.c_str(), 
         NULL,   /* No credential */ 
         NULL))
      {
      USE_SERIAL.println("connected");
          
      // then Subscribe topics
      mqttclient.subscribe(TOPIC_LED,1);
      mqttclient.subscribe(TOPIC_TEMP,1);

    } 
    else { // Connection to broker failed : retry !
      USE_SERIAL.print("failed, rc=");
      USE_SERIAL.print(mqttclient.state());
      USE_SERIAL.println(" try again in 5 seconds");
      delay(5000); // Wait 5 seconds before retrying
    }
  } // end while
}


/* -------------------------------------------------------------------------------------------------------- */
/* JSON Object Update Function                                                                              */
/* -------------------------------------------------------------------------------------------------------- */

void updateJdoc(StaticJsonDocument<1500> & jdoc) {
  
  JsonObject status         = jdoc.createNestedObject("status");
  status["temperature"]     = temp;
  status["light"]           = light; 
  status["regul"]           = fanState;
  status["heat"]            = heaterState;
  status["cold"]            = coolerState;
  status["fire"]            = fire;

  JsonObject location       = jdoc.createNestedObject("location");
  location["room"]          = "NOP";
  
  JsonObject gps            = location.createNestedObject("gps");
  gps["lat"]                = my_latitude;
  gps["lon"]                = my_longitude;
  location["address"]       = "Juan-les-Pins";

  JsonObject regulation     = jdoc.createNestedObject("regul");
  regulation["ht"]          = seuil[0];
  regulation["lt"]          = seuil[3];

  JsonObject info           = jdoc.createNestedObject("info");
  info["ident"]             = "P_22301479";
  info["loc"]               = "Juan-les-Pins";
  info["user"]              = "Rafael";

  JsonObject network        = jdoc.createNestedObject("net");
  network["uptime"]         = 0;
  network["ssid"]           = WiFi.BSSIDstr().c_str();
  network["mac"]            = WiFi.macAddress().c_str();
  network["ip"]             = "NOP";
  
  JsonObject reportHost     = jdoc.createNestedObject("reporthost"); 
  reportHost["target_ip"]   = "NOP";
  reportHost["target_port"] = "NOP";
  reportHost["sp"]          = "NOP";

  JsonObject piscine        = jdoc.createNestedObject("piscine"); 
  piscine["hotspot"]        = hotspot;
  
  if(light<50)
  {
    piscine["occuped"]     = true;
    occupied               = true;
  }else{
    piscine["occuped"]     = false;
    occupied               = false;
  }

}

/* -------------------------------------------------------------------------------------------------------- */
/* LOOP                                                                                                     */
/* -------------------------------------------------------------------------------------------------------- */

void loop(){  
  
    int delai = 1000;
 
    /* Update sensors */
    DoSmtg(delai);
    
    int32_t period = 5000; // 5 sec

    /* ---------------------------------------------------------------------------------------------------- */
    /* Light sensor                                                                                         */
    /* ---------------------------------------------------------------------------------------------------- */
    
    light = analogRead(A5);                                   // Read analog input on ADC1_CHANNEL_5 (GPIO 33)

    //Serial.println(light, DEC);                                       // Prints the value to the serial port
    
    /* ---------------------------------------------------------------------------------------------------- */
    /* Temperature sensor                                                                                   */
    /* ---------------------------------------------------------------------------------------------------- */

    tempSensor.requestTemperaturesByIndex(0);
    temp = tempSensor.getTempCByIndex(0); 
    
    //Serial.printf("Temperature of the room: %fC \n",temp);
    
    /* ---------------------------------------------------------------------------------------------------- */
    /* LED light Control refused / occupied / available                                                     */
    /* ---------------------------------------------------------------------------------------------------- */

    strip.begin();   
   
    if(fire)
    {                                                                                                                        
      for(int i=0; i<NUMLEDS; i++) {                                                       // Request refused
          strip.setPixelColor(i, strip.Color(255, 0, 0));
      }
      strip.show();
      
      counterFire = counterFire + 1;
   
      if(counterFire>3){
        fire = false;
        counterFire = 0;
      }
        
    }
    else{
      if(occupied){
          for(int i=0; i<NUMLEDS; i++) {                                                     // Pool occupied
            strip.setPixelColor(i, strip.Color(255, 255, 0));
          }
          strip.show();
      }else{
        for(int i=0; i<NUMLEDS; i++) {                                                       // Poo available
            strip.setPixelColor(i, strip.Color(0, 255, 0));
        }
        strip.show();
      }
    }

    ledcWrite(0, rcv[fanSpeed]);

    /* ---------------------------------------------------------------------------------------------------- */
    /* JSON                                                                                                 */
    /* ---------------------------------------------------------------------------------------------------- */

    char payload[1024];                                                                      // Create payload
  
    StaticJsonDocument<1500> jdoc;                                                       // Create JSON object 
    updateJdoc(jdoc);                                                             // Call JSON update function
    serializeJson(jdoc, payload);
    Serial.println(payload);                                                                  // Print payload

    /* ---------------------------------------------------------------------------------------------------- */
    /* HTTP POST                                                                                            */
    /* ---------------------------------------------------------------------------------------------------- */

    if(repporting){
      
      HTTPClient http;
  
      // http://172.20.10.2:1880/esp
      
      http.begin("http://" + target_ip + ":" + target_port + "/esp");  // Specify destination
      
      //http.begin("http://172.20.10.2:1880/esp");  // Specify destination
      http.addHeader("Content-Type", "application/json");
        
      int httpResponseCode = http.POST(payload); // Your POST data
    
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        Serial.println("http://" + target_ip + ":" + target_port + "/esp");
      }
      http.end();
      
      delay(500 * (target_sp.toInt())); // Post every 10 seconds

    }

    /* ---------------------------------------------------------------------------------------------------- */
    /* MQTT                                                                                                 */
    /* ---------------------------------------------------------------------------------------------------- */
    
    /*--- subscribe to TOPIC_LED if not yet ! */
    mqtt_subscribe_mytopics();
  
    /*--- make payload ... too badly ! */
    delay(period);
    // Convert the value to a char array
    //char payloadMQ[100];
    //sprintf(payloadMQ,"{\"temperature\" : \"%.2f\"}",temperatureMQ);
    // Serial info
    USE_SERIAL.print("Publish payloadMQ : "); USE_SERIAL.print(payload); 
    USE_SERIAL.print(" on topic : "); USE_SERIAL.println(TOPIC_TEMP);
    
    
    /*--- Publish payloadMQ on TOPIC_TEMP  */
    mqttclient.publish(TOPIC_TEMP, payload);
  
    /* Process MQTT ... une fois par loop() ! */
    mqttclient.loop(); // Process MQTT event/action

    /* ---------------------------------------------------------------------------------------------------- */
    /* Check HOTSPOT                                                                                        */
    /* ---------------------------------------------------------------------------------------------------- */

    if(temp > highiest_temp){
      hotspot = true;
    }else{
      hotspot = false;
    }
}

/* -------------------------------------------------------------------------------------------------------- */
/* Fonction for calculating the distance between 2 gps points                                               */
/* -------------------------------------------------------------------------------------------------------- */

double distance(float latitudeA, float longitudeA, float latitudeB, float longitudeB) {
    
    double dif_latitude  = radians(latitudeB - latitudeA);
    double dif_longitude = radians(longitudeB - longitudeA);
    
    double a = sin(dif_latitude / 2) * 
               sin(dif_latitude / 2) + 
               cos(radians(latitudeA)) * 
               cos(radians(latitudeB)) * 
               sin(dif_longitude / 2) * 
               sin(dif_longitude / 2);
               
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    double distance = 6371 * c; 

    return distance;
}

/* -------------------------------------------------------------------------------------------------------- */
/* END                                                                                                      */
/* -------------------------------------------------------------------------------------------------------- */

#if 0
/*
 *  Old fashion payloadMQ and publishing
 *
 */
  char payloadMQ[100];
  char topic[150];
  // Space to store values to send
  char str_sensor[10];
  char str_lat[6];
  char str_lng[6];
  
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);
  sprintf(payloadMQ, "%s", ""); // Cleans the payloadMQ
  sprintf(payloadMQ, "{\"%s\":", VARIABLE_LABEL); // Adds the variable label
  
  float sensor = analogRead(SENSOR); 
  float lat = 6.101;
  float lng= -1.293;

  /* 4 is mininum width, 2 is precision; float value is copied onto str_sensor*/
  dtostrf(sensor, 4, 2, str_sensor);
  dtostrf(lat, 4, 2, str_lat);
  dtostrf(lng, 4, 2, str_lng);  
  
  sprintf(payloadMQ, "%s {\"value\": %s", payloadMQ, str_sensor); // Adds the value
  sprintf(payloadMQ, "%s, \"context\":{\"lat\": %s, \"lng\": %s}", payloadMQ, str_lat, str_lng); 
  sprintf(payloadMQ, "%s } }", payloadMQ); // Closes the dictionary brackets
  
  client.publish(topic, payloadMQ);
#endif
