#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x62); 
Adafruit_DCMotor *M1 = AFMS.getMotor(4);
Adafruit_DCMotor *M2 = AFMS.getMotor(3);
Adafruit_DCMotor *M3 = AFMS.getMotor(2);
Adafruit_DCMotor *M4 = AFMS.getMotor(1);


void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  Serial.println("Arm starting");

  AFMS.begin();  // create with the default frequency 1.6KHz
  Serial.println ("OK");
}


// ----- state ------
int speed = 100;

int M1_on = 0;
int M2_on = 0;
int M3_on = 0;
int M4_on = 0;

unsigned long counter = 0;

void loop() {
  int command;
  if (millis() - counter > 10000) {
     counter = millis();
     Serial.print   ("=A0 ");
     Serial.println (analogRead(0));
     Serial.print   ("=A1 ");
     Serial.println (analogRead(1));
     Serial.print   ("=A2 ");
     Serial.println (analogRead(2));
  }
  if (Serial.available()) {
     command = Serial.read();
     switch (command) {
       case '1':
         M1_on = M1_on ? 0 : 1;
         if (M1_on) {
            M1->setSpeed(speed);
            M1->run(FORWARD);
         } else {
            M1->run(RELEASE);
         }
         break;
       case '2':
         M2_on = M2_on ? 0 : 1;
         if (M2_on) {
            M2->setSpeed(speed);
            M2->run(FORWARD);
         } else {
            M2->run(RELEASE);
         }
         break;
       case '3':
         M3_on = M3_on ? 0 : 1;
         if (M3_on) {
            M3->setSpeed(speed);
            M3->run(FORWARD);
         } else {
            M3->run(RELEASE);
         }
         break;
       case '4':
         M4_on = M4_on ? 0 : 1;
         if (M4_on) {
            M4->setSpeed(speed);
            M4->run(FORWARD);
         } else {
            M4->run(RELEASE);
         }
         break;
       case 'q':
         M1_on = M1_on ? 0 : 1;
         if (M1_on) {
            M1->setSpeed(speed);
            M1->run(BACKWARD);
         } else {
            M1->run(RELEASE);
         }
         break;
       case 'w':
         M2_on = M2_on ? 0 : 1;
         if (M2_on) {
            M2->setSpeed(speed);
            M2->run(BACKWARD);
         } else {
            M2->run(RELEASE);
         }
         break;
       case 'e':
         M3_on = M3_on ? 0 : 1;
         if (M3_on) {
            M3->setSpeed(speed);
            M3->run(BACKWARD);
         } else {
            M3->run(RELEASE);
         }
         break;
       case 'r':
         M4_on = M4_on ? 0 : 1;
         if (M4_on) {
            M4->setSpeed(speed);
            M4->run(BACKWARD);
         } else {
            M4->run(RELEASE);
         }
         break;
       case 'x':
         do_stop();
         break;
     }
  }

}

void do_stop() {
  M1_on = 0;
  M1->run(RELEASE);
  M2_on = 0;
  M2->run(RELEASE);
  M3_on = 0;
  M3->run(RELEASE);
  M4_on = 0;
  M4->run(RELEASE);
}


