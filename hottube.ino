// #define DEBUG // if DEBUG is defined, ethernet is disabled and serial communications happen
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>

#include "DS18S20.h" // reads temperature from the one digital temp sensor
#include "button.h" // what does this do?

IPAddress ip(192,168,1,75);
static byte mac[] = { 0xDE,0xAD,0x69,0x2D,0x30,0x32 };
#define SERVER_PORT 80 // what port is our web server on

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(SERVER_PORT);

#define METER_PIN 9 // analog meter connected to this pin
#define METER_TIME 1000 // how long to wait before updating meter in loop()

char buffer[1024];
int bidx;

float set_celsius = 40.5; // 40.5C = 105F
float beerctl_temp = 0; // what temp to set the heater to
unsigned long updateMeter = 0;
unsigned long time = 0;
#include "beerctl.h" // controls the heater, must come after time

void setMeter(float celsius) { // set analog temperature meter
  // PWM of 24 = 0 celsius
  //  114 = 20 C
  //  210 = 40 C
  //  from 20 to 40 degrees = 96 PWM so 4.8 PWM per degree
  //  PWM = (celsius * 4.8) + 18 works great above 2 celsius
  int pwm = constrain((int)(celsius * 4.8) + 18, 0, 255);
  analogWrite(METER_PIN,pwm);
}

void setup() {
  pinMode(METER_PIN, OUTPUT); // enable the analog temperature meter
  analogWrite(METER_PIN, 20);  // move the needle to about -1 degree C
  Serial.begin(57600);
  Serial.println("\n[backSoon]");
#ifndef DEBUG
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  bidx = 0;
#endif
  if (initTemp()) {
    Serial.print("DS18B20 temp sensor found, degrees C = ");
    Serial.println(getTemp());
    setMeter(getTemp());
  }
  else Serial.println("ERROR: DS18B20 temp sensor NOT found!!!");
  beerctl_init(); // init the heater and thermistors
}

void sendResponse(EthernetClient* client) {
  if (strncmp("GET /sc/", (char*)buffer, 8) == 0) {
    set_celsius = (unsigned long)atoi(buffer+8);
  }
  else if (strncmp("GET /j/off", (char*)buffer, 10) == 0) {
    // deactivate jets
  }
  else if (strncmp("GET /j/on", (char*)buffer, 9) == 0) {
    // activate jets
  }

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html");
  client->println("Pragma: no-cache");
  client->println();
  // print the current readings, in HTML format:
  client->print("Temperature: ");
  float celsius = getTemp(); // query the DS18B20 temp sensor
  client->print(celsius);
  client->print(" degrees C or ");
  client->print(celsiusToFarenheit(celsius));
  client->println(" degrees F<br />");
  client->print("Set point: ");
  client->print(set_celsius);
  client->print(" degrees C or ");
  client->print(celsiusToFarenheit(set_celsius));
  client->println(" degrees F<br />");
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
  if (time - updateMeter >= METER_TIME ) {
    float celsius = getTemp();
    setMeter(celsius); // set the temperature meter
#ifdef DEBUG
    Serial.println(celsiusToFarenheit(celsius));
#endif
    updateMeter = time;
    if (celsius < set_celsius) {
      beerctl_temp = celsiusToFarenheit(set_celsius);
      // turn on pump 
    } else {
      beerctl_temp = 0;
      // turn off pump if a minute has elapsed since beerctl_temp changed to 0
    }
  }
  beerctl_loop(beerctl_temp); // whatever that temp may be
#ifndef DEBUG
  listenForEthernetClients();
#endif
}

