#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "ssid.h"
 
IPAddress localIP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
 
#define BUTTON_PIN 0            // LOW when pressed
#define GREEN_LED_PIN 13        // LOW ON
#define RELAY_BLUE_LED_PIN 12   // HIGH ON
#define DEBOUNCE_DELAY 200      // in millisecs
 
ESP8266WebServer server(80);
volatile bool isOn = false;
 
void handleRoot() {
  server.send(200, "text/html", "<h1>Use either /on or / off</h1>");
}
 
void handleOn() {
  if(isOn) {
    server.send(200, "text/html", "<h1>Already ON, nothing to do.</h1>");
  } else {
    isOn = true;
    digitalWrite(RELAY_BLUE_LED_PIN, isOn);
    server.send(200, "text/html", "<h1>Turned ON.</h1>");
  }
}
 
void handleOff() {
  if(!isOn) {
    server.send(200, "text/html", "<h1>Already OFF, nothing to do.</h1>");
  } else {
    isOn = false;
    digitalWrite(RELAY_BLUE_LED_PIN, isOn);
    server.send(200, "text/html", "<h1>Turned OFF.</h1>");
  }
}
 
void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RELAY_BLUE_LED_PIN, OUTPUT);
 
  digitalWrite(GREEN_LED_PIN, HIGH);   // Turn Green LED OFF
  digitalWrite(RELAY_BLUE_LED_PIN, LOW);  // Turn relay OFF
 
  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  attachInterrupt(BUTTON_PIN, onSwitchChange, CHANGE);

  Serial.begin(115200);
  Serial.println("Starting Sonoff");
  connectToWifi();
 
//  // create access point
//  // void softAP(const char* ssid, const char* passphrase, int channel = 1, int ssid_hidden = 0);
//  WiFi.softAPConfig(localIP, gateway, subnet);
//  WiFi.softAP("sonoff", "whatsonoff", 1, 0); // hide the SSID
// 
//  // start web server
//  server.on("/", handleRoot);
//  server.on("/on", handleOn);
//  server.on("/off", handleOff);
//  server.begin();
}
 
void loop() {
  getNTPTime();
//  server.handleClient();
}
 
volatile long lastEventTime = 0;
volatile bool lastState = HIGH;
 
void onSwitchChange() {
  long currentTime = millis();
  bool currentState = digitalRead(BUTTON_PIN);
 
  // debouce by making sure this event doesn't happen too often
  // debouncing is done on ALL changes
  if(currentTime - lastEventTime > DEBOUNCE_DELAY) {
    // toggle the relay and store its current state
    // toggling is done only on FALLING events (when switch is pressed -> LOW)
    // we need to check that it's actually Falling as opposed to just LOW to avoid dupe events
    if(currentState == LOW && lastState == HIGH) {
      isOn = !isOn;
      digitalWrite(RELAY_BLUE_LED_PIN, isOn);
    }
  }
 
  lastEventTime = currentTime;
  lastState = currentState;
}
