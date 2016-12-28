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
int trim = 0;

int speed_rf = 0;
int speed_lf = 0;
int speed_rb = 0;
int speed_lb = 0;

int BS = 0;
int FS = 0;
int PS = 0;

unsigned long current_time = 0;

char linebuf[256];
int  lineoff = 0;

void loop() {
   if (Serial.available() ) {
      int char_in = Serial.read();
      switch (char_in) {
        case '\n': break; // ignore linefeed
        case '\r':
           linebuf[lineoff++] = '\0';
           lineoff = 0;
           handle_line();
           break;
        default:
           linebuf[lineoff++] = char_in;
           if (lineoff > 254) {  // Line too long - discard.
              lineoff = 0;
           }
        }
    }
}

void handle_line() {
   if (linebuf[2] == '=') {
      linebuf[2] = '\0';
      if (!strcmp(linebuf, "BS")) {
         BS = atoi (linebuf + 3);
      } else if (!strcmp (linebuf, "FS")) {
         FS = atoi (linebuf + 3);
      } else if (!strcmp (linebuf, "PS")) {
         PS = atoi (linebuf + 3);
      } else {
         Serial.print   ("? var ");
         Serial.println (linebuf);
         lineoff = 0;
         return;
      }
      update_motors();
   } else {
      Serial.print   ("? cmd ");
      Serial.println (linebuf);
   }
}

void update_motors() {
   if (FS) {
      int speed = FS * 10;
      if (speed > 255) { speed = 255; }
      int lspeed = speed;
      int rspeed = speed;
      if (PS < 0) {
         lspeed = - lspeed * PS / 12;
      } else if (PS > 0) {
         rspeed =   rspeed * PS / 12;
      }
      set_motor ("lf", Mlf, &speed_lf, lspeed);
      set_motor ("rf", Mrf, &speed_rf, rspeed);
      set_motor ("lb", Mlb, &speed_lb, lspeed);
      set_motor ("rb", Mrb, &speed_rb, rspeed);
   } else if (BS) {
      int speed = BS * 10;
      if (speed > 255) { speed = 255; }
      int lspeed = speed;
      int rspeed = speed;
      if (PS < 0) {
         lspeed = - lspeed * PS / 12;
      } else if (PS > 0) {
         rspeed =   rspeed * PS / 12;
      }
      set_motor ("lf", Mlf, &speed_lf, -lspeed);
      set_motor ("rf", Mrf, &speed_rf, -rspeed);
      set_motor ("lb", Mlb, &speed_lb, -lspeed);
      set_motor ("rb", Mrb, &speed_rb, -rspeed);
   } else if (PS) {
      int speed = 255 * PS / 12;
      set_motor ("lf", Mlf, &speed_lf, -speed);
      set_motor ("rf", Mrf, &speed_rf,  speed);
      set_motor ("lb", Mlb, &speed_lb, -speed);
      set_motor ("rb", Mrb, &speed_rb,  speed);
   } else {
     set_motor ("lf", Mlf, &speed_lf, 0);
     set_motor ("rf", Mrf, &speed_rf, 0);
     set_motor ("lb", Mlb, &speed_lb, 0);
     set_motor ("rb", Mrb, &speed_rb, 0);
   }
}

void set_motor(char * m, Adafruit_DCMotor *M, int * speed_m, int speed) {
   if (speed != *speed_m) {
      Serial.print(m);
      Serial.print(" ");
      Serial.println(speed);
   }
   *speed_m = speed;
   M->setSpeed(speed);
   if (speed > 0) {
     M->setSpeed(speed);
     M->run(FORWARD);
   } else if (speed < 0) {
     M->setSpeed(-speed);
     M->run(BACKWARD);
   } else {
     M->run(RELEASE);
   }
}

