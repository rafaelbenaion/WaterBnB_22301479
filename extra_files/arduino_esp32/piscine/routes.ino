/* 
 * Auteur : G.Menez
 * Fichier : http_as_serverasync/routes.ino 
 */

#include "ESPAsyncWebServer.h"
#include "routes.h"
#include "SPIFFS.h"
#include "HTTPClient.h"

#define USE_SERIAL Serial
extern String last_temp, last_light, low_temp, high_temp;

/*===================================================*/
String processor(const String& var){
  /* Replaces "placeholder" in  html file with sensors values */
  /* accessors functions get_... are in sensors.ino file   */

  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return last_temp;
    /* On aimerait écrire : return get_temperature(TempSensor);
     * mais c'est un exemple de ce qu'il ne faut surtout pas écrire ! 
     * yield + async => core dump ! 
     */
    //return get_temperature(TempSensor);
  }
  else if(var == "LIGHT"){
    return last_light;
  }
  else if(var == "LT"){
    return low_temp;
  }
  else if(var == "HT"){
    return high_temp;
  }
   else if(var == "UPTIME"){
    return uptime;
  }
  else if(var == "WHERE"){
    return where;
  }
   else if(var == "SSID"){
    return wifi_ssid;
  }
   else if(var == "MAC"){
    return mac_address;
  }
  else if(var == "IP"){
    return ip_address;
  }
  else if(var == "HEATER"){
    return heater_stat;
  }
  else if(var == "COOLER"){
    return cooler_stat;
  }
  return String(); // parce que => cf doc de asyncwebserver
}

/*===================================================*/
void setup_http_routes(AsyncWebServer* server) {
  /* 
   * Sets up AsyncWebServer and routes 
   */
  
  // doc => Serve files in directory "/" when request url starts with "/"
  // Request to the root or none existing files will try to server the default
  // file name "index.htm" if exists 
  // => premier param la route et second param le repertoire servi (dans le SPIFFS) 
  // Sert donc les fichiers css !  
  server->serveStatic("/", SPIFFS, "/").setTemplateProcessor(processor);  
  
  // Declaring root handler, and action to be taken when root is requested
  auto root_handler = server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      /* This handler will download index.html (stored as SPIFFS file) and will send it back */
      request->send(SPIFFS, "/index.html", String(), false, processor); 
      // cf "Respond with content coming from a File containing templates" section in manual !
      // https://github.com/me-no-dev/ESPAsyncWebServer
      // request->send_P(200, "text/html", page_html, processor); // if page_html was a string .   
    });
  
  server->on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
      /* The most simple route => hope a response with temperature value */ 
      //USE_SERIAL.printf("GET /temperature request \n"); 
      /* Exemple de ce qu'il ne faut surtout pas écrire car yield + async => core dump !*/
      //request->send_P(200, "text/plain", get_temperature(TempSensor).c_str());
      request->send_P(200, "text/plain", last_temp.c_str());
    });

  server->on("/light", HTTP_GET, [](AsyncWebServerRequest *request){
      /* The most simple route => hope a response with light value */ 
      request->send_P(200, "text/plain", last_light.c_str());
    });

  // This route allows users to change thresholds values through GET params
  server->on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
      /* A route with a side effect : this get request has a param and should     
       *  set a new light_threshold ... used for regulation !
       */
      if (request->hasArg("light_threshold")) { // request may have arguments
      	Light_threshold = atoi(request->arg("light_threshold").c_str());
      	request->send_P(200, "text/plain", "Threshold Set !");
      }
    });
  
  server->on("/target", HTTP_POST, [](AsyncWebServerRequest *request){
      /* A route receiving a POST request with Internet coordinates 
       * of the reporting target host.
       */
      Serial.println("Receive Request for a periodic report !"); 
      if (request->hasArg("ip") && request->hasArg("port") && request->hasArg("sp")) {
      	
      	target_ip   = request->arg("ip");
      	target_port = atoi(request->arg("port").c_str());
      	target_sp   = atoi(request->arg("sp").c_str());

        startRepporting(""+target_ip,""+target_port,""+target_sp);
       
      }
      request->send(SPIFFS, "/index.html", String(), false, processor);
    });
  
  // If request doesn't match any route, returns 404.
  server->onNotFound([](AsyncWebServerRequest *request){
      request->send(404);
    });
}
/*===================================================*/
void startRepporting(String ip, String port, String sp){
  target_ip   = ip; 
  target_port = port; 
  target_sp   = sp;      
  repporting  = true; 
  
}
