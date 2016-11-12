/*
  Analog Input
 Demonstrates analog input by reading an analog sensor on analog pin 0 and
 turning on and off a light emitting diode(LED)  connected to digital pin 13. 
 The amount of time the LED will be on and off depends on
 the value obtained by analogRead(). 
 
 The circuit:
 * Potentiometer attached to analog input 0
 * center pin of the potentiometer to the analog pin
 * one side pin (either one) to ground
 * the other side pin to +5V
 * LED anode (long leg) attached to digital output 13
 * LED cathode (short leg) attached to ground
 
 * Note: because most Arduinos have a built-in LED attached 
 to pin 13 on the board, the LED is optional.
 
 
 Created by David Cuartielles
 modified 30 Aug 2011
 By Tom Igoe
 
 This example code is in the public domain.
 
 http://arduino.cc/en/Tutorial/AnalogInput
 
 */

int sensorPin = A0;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
int sensorValue = 0;  // variable to store the value coming from the sensor
float voltage = 0;
float R2 = 0;
int R22 = 0;
int R1 = 1000;

void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
//Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial.println("Setup");  
}

void loop() {
  // read the value from the sensor:
  sensorValue = analogRead(sensorPin);// 0 to 1023
  voltage = (float)(1023 - sensorValue) / 1023.0 * 5.0;
  R2 = R1* (5.0/voltage - 1.0);
  R22 = 1023 * R1 / (1023-sensorValue) - R1;
  Serial.print("SensorValue: ");
  Serial.println(sensorValue);
  Serial.print("voltage: ");
  Serial.println(voltage);
  Serial.print("R2: ");
  Serial.println(R22);
  // turn the ledPin on
  //digitalWrite(ledPin, HIGH);  
  // stop the program for <sensorValue> milliseconds:
  delay(sensorValue);          
  // turn the ledPin off:        
  //digitalWrite(ledPin, LOW);   
  // stop the program for for <sensorValue> milliseconds:
  delay(sensorValue);                  
}
