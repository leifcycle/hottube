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

unsigned long orbit_timeout = 0;
unsigned long sf_timeout = 0;
unsigned long time = 0;

void setup() {
  Serial.begin(57600);
  Serial.println("\n[backSoon]");

  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  bidx = 0;

  if (initTemp()) {
    Serial.print("DS18B20 temp sensor found, degrees C = ");
    Serial.println(getTemp());
    Serial.println(getTemp()); // have to do this twice here or it won't work later
  }
  else Serial.println("ERROR: DS18B20 temp sensor NOT found!!!");
}

}

void sendResponse(EthernetClient* client) {
  if (strncmp("GET /orbit/on", (char*)buffer, 13) == 0) {
    orbit_timeout = time + (unsigned long)atoi(buffer+14) * 60000;
  }
  else if (strncmp("GET /orbit/off", (char*)buffer, 14) == 0) {
  }
  else if (strncmp("GET /sf/on", (char*)buffer, 10) == 0) {
    sf_timeout = time + (unsigned long)atoi(buffer+11) * 60000;
  }
  else if (strncmp("GET /sf/off", (char*)buffer, 11) == 0) {
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
  }
  listenForEthernetClients();
}
