// #define DEBUG // if DEBUG is defined, ethernet is disabled and serial communications happen
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>

#include "DS18S20.h" // reads temperature from the one digital temp sensor
#include "button.h" // what does this do?

#define DEBUG

IPAddress ip(192,168,1,75);
static byte mac[] = { 0xDE,0xAD,0x69,0x2D,0x30,0x32 };
#define SERVER_PORT 80 // what port is our web server on

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(SERVER_PORT);

#define PUMPMINTIME 60000 // minimum time to run heater pump
#define HI_FLOW_PUMP_PIN 3 // to turn pump on high AKA jets
#define LO_FLOW_PUMP_PIN 5 // to turn on heater circulator pump
#define HYSTERESIS 0.5 // how many degrees lower then set_celsius before turning heater on
#define METER_PIN 9 // analog meter connected to this pin
#define METER_TIME 1000 // how long to wait before updating meter in loop()
#define JETS_TIME_MAX 240 // maximum jets time in minutes
#define JETS_REQUEST_PIN A5 // short this pin to ground to turn jets on or off
#define JETS_REQUEST_TIME 5 // minutes of jets requested
#define TEMP_VALID_MIN 10 // minimum celsius reading from temp sensor considered valid
#define TEMP_VALID_MAX 120 // maximum celsius reading from temp sensor considered valid
#define MAXREADINGAGE 60000 // maximum time since last valid reading to continue to use it
#define BUFFER_SIZE 256 // 1024 was too big, it turns out, as was 512 after CORS
char buffer[BUFFER_SIZE];
int bidx = 0;

float set_celsius = 5; // 40.5555555C = 105F
float celsiusReading = 0; // stores valid value read from temp sensor
unsigned long updateMeter, pumpTime, jetsOffTime, lastTempReading = 0;
unsigned long time = 0;

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
  pinMode(HI_FLOW_PUMP_PIN, OUTPUT);
  pinMode(HEATER_PUMP_PIN, OUTPUT);
  analogWrite(METER_PIN, 20);  // move the needle to about -1 degree C
  Serial.begin(57600);
  Serial.println("\n[backSoon]");
#ifndef DEBUG
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
#endif
  if (initTemp()) {
    Serial.print("DS18B20 temp sensor found, degrees C = ");
    Serial.println(getTemp());
    setMeter(getTemp());
  }
  else Serial.println("ERROR: DS18B20 temp sensor NOT found!!!");
}

void redirectClient(EthernetClient* client) {
    client->println("HTTP/1.1 302");
    client->println("Pragma: no-cache");
    client->println("Access-Control-Allow-Origin: *");
    client->println("Location: /");
}

void sendResponse(EthernetClient* client) {
  if (strncmp("GET /sc/", (char*)buffer, 8) == 0) {
    set_celsius = atof(buffer+8);
    redirectClient(client);
    return;
  }
  else if (strncmp("GET /sf/", (char*)buffer, 8) == 0) {
    set_celsius = farenheitToCelsius(atof(buffer+8));
    redirectClient(client);
    return;
  }
  else if (strncmp("GET /j/off", (char*)buffer, 10) == 0) {
    digitalWrite(HI_FLOW_PUMP_PIN,LOW); // deactivate jets (even though jetsOffTime will cause that)
    jetsOffTime = time; // it's turnoff time
    redirectClient(client);
    return;
  }
  else if (strncmp("GET /j/on/", (char*)buffer, 10) == 0) {
    int jetMinutes = atoi(buffer+10); // activate jets for x minutes
    if ((jetMinutes > 0) && (jetMinutes <= JETS_TIME_MAX)) {
      digitalWrite(LO_FLOW_PUMP_PIN,LOW); // turn off low pump
      digitalWrite(HI_FLOW_PUMP_PIN,HIGH); // turn on high pump
      jetsOffTime = time + (jetMinutes * 60000); // set turn-off time in minutes from now
    }
    redirectClient(client);
    return;
  }

  client->println("HTTP/1.1 200 OK");
  client->println("Pragma: no-cache");
  client->println("Access-Control-Allow-Origin: *");
  float celsius = getTemp(); // query the DS18B20 temp sensor

  if (strncmp("GET /help", (char*)buffer, 9) == 0) {
    client->println("Content-Type: text/plain\n");
    client->println("GET /sc/{DEGREES}");
    client->println("  Set the temperature in degrees celsius.\n");
 
    client->println("GET /sf/{DEGREES}");
    client->println("  Set the temperature in degrees fahrenheit.\n");
 
    client->println("GET /j/on/{MINUTES}");
    client->println("GET /j/off");
    client->println("  Turn the jets on for MINUTES or off.\n");
 
    client->println("GET /sensors[.json]");
    client->println("  All the sensor data [as json]\n");
  }
  else if (strncmp("GET /sensors", (char*)buffer, 12) == 0) {
    if (strncmp("GET /sensors.json", (char*)buffer, 17) == 0) {
      client->println("Content-Type: application/json\n");
    } else {
      client->println("Content-Type: text/plain\n");
    }
    client->println("{");

    client->print("  \"heater_pump\": ");
    client->println(digitalRead(HEATER_PUMP_PIN) ? "true," : "false,");

    client->println("  \"temperature\": {");
    client->print("    \"celsius\": ");
    client->print(celsius);
    client->println(",");
    client->print("    \"fahrenheit\": ");
    client->print(celsiusToFarenheit(celsius));
    client->println("\n  },");
    
    client->println("  \"set_temp\": {");
    client->print("    \"celsius\": ");
    client->print(set_celsius);
    client->println(",");
    client->print("    \"fahrenheit\": ");
    client->print(celsiusToFarenheit(set_celsius));
    client->println("\n  },");

    client->print("    \"jets\": ");
    client->print(jetsOffTime > time ? jetsOffTime - time : 0);
    client->println("\n}");
  }
  else {
    client->println("Content-Type: text/html\n");
    // print the current readings, in HTML format:
    if (digitalRead(HEATER_PUMP_PIN)) {
      client->println("Heater pump is on!");
      client->println();
    }
    client->print("Temperature: ");
    client->print(celsius);
    client->print(" degrees C or ");
    client->print(celsiusToFarenheit(celsius));
    client->println(" degrees F<br />");
    client->print("Set point: ");
    client->print(set_celsius);
    client->print(" degrees C or ");
    client->print(celsiusToFarenheit(set_celsius));
    client->println(" degrees F<br />");
 
    client->println("<br />See <a href=\"/help.txt\">help.txt</a> for API information<br />");
  }
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
        if (bidx == BUFFER_SIZE) bidx = BUFFER_SIZE - 1; // don't overflow the buffer!!!

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

unsigned long jetRequestDebounce = 0; // last time jets request pin was high
#define JETS_REQUEST_DEBOUNCE_TIME 900 // time in milliseconds to debounce pin
#define JETS_REQUEST_CANCEL_TIME 1500 // time in milliseconds to cancel jets altogether

// this routine is seriously influenced by the main loop waiting 850ms for each
// call to read the DS18S20 temperature sensor.  This will change when DS18S20.h
// is modified to non-blocking behavior.

void updateJets() {
  if (time > jetsOffTime) digitalWrite(HI_FLOW_PUMP_PIN,LOW); // it's turn-off time!
  if (digitalRead(JETS_REQUEST_PIN)) { // jets request button is not activated
    jetRequestDebounce = time; // record last time we were high (unactivated)
  } else if (time - jetRequestDebounce > JETS_REQUEST_DEBOUNCE_TIME) { // activated and it's not a bounce
    if (time - jetRequestDebounce > JETS_REQUEST_CANCEL_TIME) { // holding button down cancels jets
      jetsOffTime = time; // cancel jets
      digitalWrite(HI_FLOW_PUMP_PIN,LOW); // turn OFF the jets
    } else { // just trying to increment jet time
      if (digitalRead(HI_FLOW_PUMP_PIN)) { // if pump is already on
        jetsOffTime += JETS_REQUEST_TIME * 60000; // add the time increment
        if (jetsOffTime - time > (JETS_TIME_MAX * 60000)) jetsOffTime = time + (JETS_TIME_MAX * 60000); // constrain
      } else {
        jetsOffTime = time + JETS_REQUEST_TIME * 60000; // set the time increment starting now
        digitalWrite(LO_FLOW_PUMP_PIN,LOW); // turn off the low pump
        digitalWrite(HI_FLOW_PUMP_PIN,HIGH); // turn on the hi pump
      }
    }
  }
}

void loop() {
  time = millis();
  if (time - updateMeter >= METER_TIME ) {
    float lastCelsiusReading = celsiusReading;
    celsiusReading = getTemp();
    setMeter(celsiusReading); // set the temperature meter
    if ((celsiusReading > TEMP_VALID_MIN) && (celsiusReading < TEMP_VALID_MAX)) {
      lastTempReading = time; // temperature sensor reported a sane value
    } else if (time - lastTempReading < MAXREADINGAGE) { // if the last reading isn't too old
      celsiusReading = lastCelsiusReading; // just use the last valid value
    }
#ifdef DEBUG
    Serial.println(celsiusToFarenheit(celsiusReading));
#endif
    updateMeter = time;
    if (celsiusReading + HYSTERESIS < set_celsius) {  // only turn on heat if HYSTERESIS deg. C colder than target
      if (!digitalRead(HEATER_PUMP_PIN)) pumpTime = time; // remember when we last turned on
      digitalWrite(HEATER_PUMP_PIN,HIGH); // turn on pump
    }
    if ((celsiusReading > set_celsius) && (time - pumpTime > PUMPMINTIME)) { // if we reach our goal, turn off heater
      digitalWrite(HEATER_PUMP_PIN,LOW); // turn off pump
    }
  }
#ifndef DEBUG
  listenForEthernetClients();
  updateJets();
#endif
}

