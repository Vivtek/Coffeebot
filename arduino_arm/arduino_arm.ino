#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x62); 
Adafruit_DCMotor *M[4] = { AFMS.getMotor(4),
                           AFMS.getMotor(3),
                           AFMS.getMotor(2),
                           AFMS.getMotor(1) };


void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  Serial.println("Arm starting");

  AFMS.begin();  // create with the default frequency 1.6KHz
  Serial.println ("OK");
}


// ----- state ------
int speed = 100;

int M_on[4] = { 0, 0, 0, 0 };
char label[4][4] = { "MA1", "MA2", "MA3", "MA4" };

unsigned long counter = 0;

void loop() {
  int command;
  if (millis() - counter > 10000) {
     counter = millis();
     announce ((char*)"A0", analogRead(0));
     announce ((char*)"A1", analogRead(1));
     announce ((char*)"A2", analogRead(2));
  }
  if (Serial.available()) {
     command = Serial.read();
     switch (command) {
       case '1':
         set_motor_speed (0, M_on[0] ? 0.0 : 1.0);
         break;
       case '2':
         set_motor_speed (1, M_on[1] ? 0.0 : 1.0);
         break;
       case '3':
         set_motor_speed (2, M_on[2] ? 0.0 : 1.0);
         break;
       case '4':
         set_motor_speed (3, M_on[3] ? 0.0 : 1.0);
         break;
       case 'q':
         set_motor_speed (0, M_on[0] ? 0.0 : -1.0);
         break;
       case 'w':
         set_motor_speed (1, M_on[1] ? 0.0 : -1.0);
         break;
       case 'e':
         set_motor_speed (2, M_on[2] ? 0.0 : -1.0);
         break;
       case 'r':
         set_motor_speed (3, M_on[3] ? 0.0 : -1.0);
         break;
       case 'x':
         do_stop();
         break;
     }
  }

}

void do_stop() {
  set_motor_speed (0, 0.0);
  set_motor_speed (1, 0.0);
  set_motor_speed (2, 0.0);
  set_motor_speed (3, 0.0);
}

void set_motor_speed (int motor, float mspeed) {
  int new_speed = (int) (mspeed * (float) speed);
  if (abs(new_speed) < 5) { new_speed = 0; }
  if (new_speed == 0) { M[motor]->run(RELEASE); } // If the motor should be shut off, don't get confused.
  if (new_speed == M_on[motor]) { return; }
  announce (label[motor], new_speed);
  M_on[motor] = new_speed;
  if (new_speed < 0) {
     M[motor]->setSpeed (-new_speed);
     M[motor]->run (BACKWARD);
  } else {
     M[motor]->setSpeed (new_speed);
     M[motor]->run (FORWARD);
  }
}

void announce (char* name, int value) {
  Serial.print ("=");
  Serial.print (name);
  Serial.print (" ");
  Serial.println (value);
}


