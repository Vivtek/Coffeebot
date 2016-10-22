#include <Adafruit_MotorShield.h>
//#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor *M1 = AFMS.getMotor(1);
Adafruit_DCMotor *M2 = AFMS.getMotor(2);
Adafruit_DCMotor *M3 = AFMS.getMotor(3);
Adafruit_DCMotor *M4 = AFMS.getMotor(4);

void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  Serial.println("OK!");
  
  AFMS.begin();  // create with the default frequency 1.6KHz
}

void loop() {
  int command;
  if (Serial.available()) {
     command = Serial.read();
     switch (command) {
       case 'f':
         do_forward();
         break;
       case 'b':
         do_reverse();
         break;
       case 'r':
         do_right_turn();
         break;
       case 'l':
         do_left_turn();
         break;
       case 'p':
         do_pivot_left();
         break;
       case 'x':
         do_stop();
         break;
     }
  }
}

void do_forward() {
  M1->setSpeed(200);
  M2->setSpeed(200);
  M3->setSpeed(200);
  M4->setSpeed(200);
  M1->run(FORWARD);
  M2->run(FORWARD);
  M3->run(FORWARD);
  M4->run(FORWARD);
}

void do_reverse() {
  M1->setSpeed(200);
  M2->setSpeed(200);
  M3->setSpeed(200);
  M4->setSpeed(200);
  M1->run(BACKWARD);
  M2->run(BACKWARD);
  M3->run(BACKWARD);
  M4->run(BACKWARD);
}

void do_right_turn() {
  M1->setSpeed(200);
  M2->setSpeed(0);
  M3->setSpeed(200);
  M4->setSpeed(0);
  M1->run(FORWARD);
  M2->run(FORWARD);
  M3->run(FORWARD);
  M4->run(FORWARD);
}

void do_left_turn() {
  M1->setSpeed(0);
  M2->setSpeed(200);
  M3->setSpeed(0);
  M4->setSpeed(200);
  M1->run(FORWARD);
  M2->run(FORWARD);
  M3->run(FORWARD);
  M4->run(FORWARD);
}

void do_pivot_left() {
  M1->setSpeed(100);
  M2->setSpeed(100);
  M3->setSpeed(100);
  M4->setSpeed(100);
  M1->run(BACKWARD);
  M2->run(FORWARD);
  M3->run(BACKWARD);
  M4->run(FORWARD);
}

void do_stop() {
  M1->setSpeed(0);
  M2->setSpeed(0);
  M3->setSpeed(0);
  M4->setSpeed(0);
  M1->run(FORWARD);
  M2->run(FORWARD);
  M3->run(FORWARD);
  M4->run(FORWARD);
}


