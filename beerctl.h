// BEGIN beerctl stuff
// threshold values in degrees F
#define RED_TARGET_1 102 // straight port
#define BLUE_HOTTERN_RED 15 // how much hotter it is at the elbow port
#define BLUE_TARGET_1 RED_TARGET_1 + BLUE_HOTTERN_RED // elbow port
#define CORRECTION -6.5 // 116.49 actual reports (blue) as 123.0F (532) 
// constants for conversion to degrees F
// blue adc = -6.4288x + 1271.529
// (adc - 1271.529) / -6.4288 = temp in F
#define BLUE_OFFSET 1322.95 // 1271.529
#define BLUE_SLOPE -6.4288
// red adc = -6.2782x + 1292.983
#define RED_OFFSET 1311.81 // 1292.983
#define RED_SLOPE -6.2782

// higher temp -> lower ADC value, higher ADC value -> lower temp. if adc is
// too high (temp is too low), assume sensor is disconnected and fail off.
#define ADC_MAX 1000

#define REDPIN A1
#define BLUEPIN A0
#define HEATER_PIN 4

#define DOWNTIME 10000 // minimum heater off time before turning on again
#define PRINTTIME 1000 // how many milliseconds between print temperatures
#define AVG_CYCLES 50 // how many times to take a reading at a time

unsigned long lastOn, lastPrinted = 0;
float red_target, blue_target;
float red = 0;  // red temp sensor
float blue = 0; // blue temp sensor value
int redADC,blueADC = 0; // for calibrating
float val = 0; // global val for looking at averaged ADC value

void beerctl_init() {
  pinMode(HEATER_PIN, OUTPUT);
  red_target = RED_TARGET_1;
  blue_target = BLUE_TARGET_1;
}

float readSensor(int sensor_pin) {
  unsigned long adder = 0;
  for (int i = 0; i < AVG_CYCLES; i++) adder += analogRead(sensor_pin);
  // delay(5);
  val = adder / AVG_CYCLES;
  if (val > ADC_MAX) return -1000.0;
// (adc - 1271.529) / -6.4288 = temp in F
  if (sensor_pin == REDPIN) return (val - RED_OFFSET)/RED_SLOPE;
  if (sensor_pin == BLUEPIN) return (val - BLUE_OFFSET)/BLUE_SLOPE;
}

void beerctl_loop(float farenheit) {
  red = readSensor(REDPIN);
  redADC = val; // val is the most recent readSensor() adc value
  red += CORRECTION;
    red = 99.0; // RED SENSOR IS BROKEN SO WE'LL IGNORE IT
  blue = readSensor(BLUEPIN);
  blueADC = val; // val is the most recent readSensor() adc value
  blue += CORRECTION;

  red_target = farenheit;
  blue_target = red_target + BLUE_HOTTERN_RED; // the outlet is always hotter than the inlet
  if (time - lastPrinted > PRINTTIME) {
#ifdef DEBUG
    Serial.print("beerctl: RED: (");
//    Serial.print(red,1);
//    Serial.print("F (");
    Serial.print(redADC);
    Serial.print(")  BLUE: ");
    Serial.print(blue,1);
    Serial.print("F (");
    Serial.print(blueADC);
    Serial.print(") red_target: F ");
    Serial.print(red_target,1);
    Serial.print(" blue_target: F ");
    Serial.println(blue_target,1);
#endif
    lastPrinted = time;
  }
  if (red < -99 || blue < -99 ) {
    if (time - lastPrinted > PRINTTIME) {
#ifdef DEBUG
      Serial.println("ERROR:  one of the heater's sensors is disconnected!");
#endif
      lastPrinted = time;
    }
    if (digitalRead(HEATER_PIN)) {
      lastOn = time; // if heater was on
#ifdef DEBUG
      Serial.println("TURNING OFF HEATER!");
#endif
    }
    digitalWrite(HEATER_PIN, LOW); // turn heater off
  }
  else if ((red > red_target) || (blue > blue_target)) {
    if (digitalRead(HEATER_PIN)) {
      lastOn = time; // if heater was on
#ifdef DEBUG
      Serial.println("TURNING OFF HEATER");
#endif
    }
    digitalWrite(HEATER_PIN, LOW); // turn heater off
  }
  else if ((red < red_target) && (blue < blue_target)) {
    if (time - lastOn > DOWNTIME) {
#ifdef DEBUG
      Serial.print("1");
      if (!digitalRead(HEATER_PIN)) Serial.println("TURNING HEATER ON");
#endif
      digitalWrite(HEATER_PIN, HIGH); // turn heater on
    }
  }
}

