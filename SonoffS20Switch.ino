#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include "ssid.h"

/*
 * Arduino IDE settings:
 * Board: "Generic ESP8266 Module"
 * Flash Mode: "DOUT"
 * Flash Size: "1M (no SPIFFS)"
 * Upload Speed: "115200"
 * Sonof PINOUT:
 * GND <- LED
 * RXD (grey)
 * TXD (brown)
 * 3V3 <- Triangle
 * 
 * Hold button when connecting USB to enable boot mode
 */

IPAddress localIP(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
 
#define BUTTON_PIN 0            // LOW when pressed
#define GREEN_LED_PIN 13        // LOW ON
#define RELAY_BLUE_LED_PIN 12   // HIGH ON
#define DEBOUNCE_DELAY 200      // in millisecs

ESP8266WebServer server(80);
volatile bool isOn = false;
volatile time_t extraTimeUntil = 0;

unsigned long epoch = 0;

void turnOff() {
  if (isOn) {
    isOn = false;
    digitalWrite(RELAY_BLUE_LED_PIN, isOn);
  }
}

void turnOn() {
  if (!isOn) {
    isOn = true;
    digitalWrite(RELAY_BLUE_LED_PIN, isOn);
  }
}

String getHeader() {
  return
  "<head>\n"
  "    <meta name=viewport content='width=250'>\n"
  "    <style>\n"
  "      body {\n"
  "        background-color: whitesmoke;\n"
  "        font-family: Helvetica;\n"
  "      }\n"
  "    </style>\n"
  "</head>\n";
}
 
void handleRoot() {
  time_t now = time(nullptr);
  String currentTime = ctime(&now);
  if (isOn) {
    server.send(200, "text/html", getHeader() +
      "<body>\n"
      "    <h1>Internet is ON</h1>\n"
      "    <p>Current time is: " + currentTime + "</p>\n"
      "</body>");
  } else {
    String onfor = server.arg("onfor");
    if (onfor == "15") {
      extraTimeUntil = time(nullptr) + 900;
      server.send(200, "text/html", getHeader() +
        "<body>\n"
        "    <h3>Switching on for another 15 minutes</h3>\n"
        "</body>");
    } else if (onfor == "30") {
      extraTimeUntil = time(nullptr) + 1800;
      server.send(200, "text/html", getHeader() +
        "<body>\n"
        "    <h3>Switching on for another 30 minutes</h3>\n"
        "</body>");
    } else {
      server.send(200, "text/html", getHeader() +
        "<body>\n"
        "    <h1>Internet is OFF</h1>\n"
        "    <p>Current time is: " + currentTime + "</p>\n"
        "    <div>\n"
        "        <form method='get' action='/'>\n"
        "            <p><button type='submit' name='onfor' value='15'>Switch on for 15 minutes</button></p>\n"
        "            <p><button type='submit' name='onfor' value='30'>Switch on for 30 minutes</button></p>\n"
        "        </form>\n"
        "    </div>\n"
        "</body>\n");
    }
  }
}
 
void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RELAY_BLUE_LED_PIN, OUTPUT);
 
  digitalWrite(GREEN_LED_PIN, HIGH);   // Turn Green LED OFF
  digitalWrite(RELAY_BLUE_LED_PIN, HIGH);  // Turn relay ON
 
  // Attach an interrupt to the pin, assign the onChange function as a handler and trigger on changes (LOW or HIGH).
  attachInterrupt(BUTTON_PIN, onSwitchChange, CHANGE);

  Serial.begin(115200);
  Serial.println("Starting Sonoff");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  // Specifies that the clocks go forward on the last sunday of march at 1am
  // and backwards on the last sunday of October at 2am
  // https://remotemonitoringsystems.ca/time-zone-abbreviations.php
  setenv("TZ", "GMT+0BST-1,M3.5.0/01:00:00,M10.5.0/02:00:00", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time");
  // By default time returns a time of around epoch + 8 hours for some reason before it has synched
  while (time(nullptr) < 50000) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Got time from NTP: ");
  time_t now = time(nullptr);
  Serial.print(ctime(&now));
  struct tm * timeinfo = localtime(&now);
  Serial.print("DST Enabled: ");
  Serial.println(timeinfo->tm_isdst);

  Serial.println("Switching to Access Point mode");
  
  WiFi.mode(WIFI_AP);
  // create access point
  // void softAP(const char* ssid, const char* passphrase, int channel = 1, int ssid_hidden = 0);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP("sonoff", "whatsonoff", 1, 0); // hide the SSID
 
  // start web server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Webserver started");
}
 
void loop() {
  time_t now = time(nullptr);
  struct tm * timeinfo = localtime(&now);
  
  boolean weekday = timeinfo->tm_wday >= 1 && timeinfo->tm_wday <= 5;
  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;
  // Weekday is number between 1 and 7. Monday is 1. Saturday 6, Sunday 7
  if (now < extraTimeUntil) {
    turnOn();
  } else if (weekday && (hour >= 11 && hour < 16)) {
    // Work time!
    turnOff();
  } else if (hour == 0 && minute >= 15) {
    // Bed time!
    turnOff();
  } else if (hour >= 1 && hour < 6) {
    // Sleep time!
    turnOff();
  } else {
    turnOn();
  }
  server.handleClient();
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
