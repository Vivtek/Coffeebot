#include <Adafruit_MotorShield.h>
//#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor *Mrf = AFMS.getMotor(1);
Adafruit_DCMotor *Mlf = AFMS.getMotor(4);
Adafruit_DCMotor *Mrb = AFMS.getMotor(2);
Adafruit_DCMotor *Mlb = AFMS.getMotor(3);

void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  Serial.println("OK!");
  
  AFMS.begin();  // create with the default frequency 1.6KHz
}

// ----- state ------
int speed = 200;
int suddenness = 100;
int trim = 0;

int Vrf = 0;
int Vlf = 0;
int Vrb = 0;
int Vlb = 0;
int speed_rf = 0;
int speed_lf = 0;
int speed_rb = 0;
int speed_lb = 0;

unsigned long current_time = 0;

void loop() {
  int command;
  if (Serial.available()) {
     command = Serial.read();
     switch (command) {
       case 'A':
         Vlf = 10;
         break;
       case 'B':
         Vrf = 10;
         break;
       case 'C':
         Vlb = 10;
         break;
       case 'D':
         Vrb = 10;
         break;
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

  if (millis() - current_time > suddenness) {
     current_time = millis();
     
     adjust_speed ("rf", Mrf, speed, Vrf, &speed_rf);
     adjust_speed ("lf", Mlf, speed, Vlf, &speed_lf);
     adjust_speed ("rb", Mrb, speed, Vrb, &speed_rb);
     adjust_speed ("lb", Mlb, speed, Vlb, &speed_lb);
  }
}

void adjust_speed (char * m, Adafruit_DCMotor *M, int speed, int V, int *speed_m) {
  int delta = speed * V - *speed_m * 10;
  
  if (!delta) {
    return;
  }
  delta = delta / 40 + 1;
  *speed_m = *speed_m + delta;
  if (*speed_m < 10 && -*speed_m < 10) {
     *speed_m = 0;
  }
  if (!strcmp (m, "lf")) {
     Serial.print(m);
     Serial.print(" ");
     Serial.println(*speed_m);
  }
  if (*speed_m > 0) {
     M->setSpeed(*speed_m);
     M->run(FORWARD);
  } else if (*speed_m < 0) {
     M->setSpeed(-*speed_m);
     M->run(BACKWARD);
  } else {
     M->run(RELEASE);
  }
}

void do_forward() {
  Vlf = 10;
  Vrf = 10;
  Vlb = 10;
  Vrb = 10;
}

void do_reverse() {
  Vlf = -10;
  Vrf = -10;
  Vlb = -10;
  Vrb = -10;
}

void do_right_turn() {
  Vlf = 0;
  Vrf = 10;
  Vlb = 0;
  Vrb = 10;
}

void do_left_turn() {
  Vlf = 10;
  Vrf = 0;
  Vlb = 10;
  Vrb = 0;
}

void do_pivot_left() {
  Vlf = -10;
  Vrf = 10;
  Vlb = -10;
  Vrb = 10;
}

void do_stop() {
  Vlf = 0;
  Vrf = 0;
  Vlb = 0;
  Vrb = 0;
}


