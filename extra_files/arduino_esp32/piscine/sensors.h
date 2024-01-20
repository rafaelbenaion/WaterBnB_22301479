#include "OneWire.h"
#include "DallasTemperature.h"

void setup_led(int LEDpin, int mode, int status);

String get_temperature(DallasTemperature tempSensor);
String get_light(int LightPin);
String getUptimeSeconds();
String get_ip_address();
String get_mac_address();
String get_wifi_ssid();
