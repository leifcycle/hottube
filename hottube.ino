#include <SPI.h>
#include <Ethernet.h>

#include "button.h"
IPAddress ip(192,168,1,75);
static byte mac[] = { 0xDE,0xAD,0x69,0x2D,0x30,0x32 };

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

char buffer[1024];
int bidx;

#define PIN_SF A0 // sparkfun valve. set PIN_SF high for on, low for off

#define PIN_ON A2  // orbit valve. pulse PIN_ON for on, pulse PIN_OFF for off
#define PIN_OFF A1

#define BUTTON_ORBIT 3
#define BUTTON_SF 2

Button orbit_button = Button(BUTTON_ORBIT);
Button sf_button = Button(BUTTON_SF);

int orbit_status = -1;
int sf_status = -1;
unsigned long orbit_timeout = 0;
unsigned long sf_timeout = 0;
unsigned long time = 0;

void setup() {
  pinMode(PIN_SF, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  pinMode(PIN_OFF, OUTPUT);
  digitalWrite(PIN_SF, LOW);
  digitalWrite(PIN_ON, LOW);
  digitalWrite(PIN_OFF, LOW);

  Serial.begin(57600);
  Serial.println("\n[backSoon]");

  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  bidx = 0;
}

void pulse(int pin) {
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
}

void orbit_on() {
  Serial.println("Orbit on");
  pulse(PIN_ON);
  orbit_status = 1;
}

void orbit_off() {
  Serial.println("Orbit off");
  pulse(PIN_OFF);
  orbit_status = 0;
}


void sf_on() {
  Serial.println("SF on");
  digitalWrite(PIN_SF, HIGH);
  sf_status = 1;
}

void sf_off() {
  Serial.println("SF off");
  digitalWrite(PIN_SF, LOW);
  sf_status = 0;
}

void sendResponse(EthernetClient* client) {
  if (strncmp("GET /orbit/on", (char*)buffer, 13) == 0) {
    orbit_on();
    orbit_timeout = time + (unsigned long)atoi(buffer+14) * 60000;
  }
  else if (strncmp("GET /orbit/off", (char*)buffer, 14) == 0) {
    orbit_off();
  }
  else if (strncmp("GET /sf/on", (char*)buffer, 10) == 0) {
    sf_on();
    sf_timeout = time + (unsigned long)atoi(buffer+11) * 60000;
  }
  else if (strncmp("GET /sf/off", (char*)buffer, 11) == 0) {
    sf_off();
  }
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Pragma: no-cache");
  client->println();
  // print the current readings, in HTML format:
  client->print("Temperature: ");
  //client->print(temperature);
  client->print(" degrees C");
  client->println("<br />");
}

void listenForEthernetClients() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("Got a client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        buffer[bidx++] = c;

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          sendResponse(&client);
          bidx = 0; // reset the buffer
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}

void loop() {
  time = millis();
  if (sf_timeout && time >= sf_timeout) {
    sf_off();
    sf_timeout = 0;
  }
  if (orbit_timeout && time >= orbit_timeout) {
    orbit_off();
    orbit_timeout = 0;
  }
  listenForEthernetClients();
}
