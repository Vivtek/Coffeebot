#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "ov2640_regs.h"

const int CS = 10;
ArduCAM cam(OV2640, CS);
int cam_ok = 1;
int capture = 0;



void setup() {
  Serial.begin(115200);           // set up Serial library at 9600 bps
  Serial.println("Eye starting");

  digitalWrite (CS, HIGH);
  delay(100);
  Wire.begin();
  SPI.begin();
  cam_init();
  
  Serial.println ("OK");
}

void cam_init() {
    uint8_t vid,pid,temp=0;
    cam.clear_bit(ARDUCHIP_GPIO, GPIO_PWDN_MASK);
    delay(100);
    cam.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = cam.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) { Serial.println ("Cam not ready"); cam_ok = 0; return; }
    if (temp == 0x55) { Serial.println ("Cam talking"); }

    cam.wrSensorReg8_8(0xff, 0x01);
    cam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    cam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26) || (pid != 0x42)) {
      cam_ok = 0;
      Serial.println ("Cam not ok");
      return;
    } else {
      Serial.println ("cam ok, OV2640");
    }

    cam.set_format(JPEG);
    cam.InitCAM();
    cam.OV2640_set_JPEG_size(OV2640_320x240);
    cam.clear_fifo_flag();
    cam.write_reg(ARDUCHIP_FRAMES, 0x00);
}


void loop() {
   // Very simple here. We just look for an "o" command on input (looks like an eye)
   // and we take a picture and send it back.
   // Later we can worry about how to set parameters and stuff.
   if (Serial.available() ) {
      int char_in = Serial.read();
      if (char_in == 'o') {
         // Snap!
         if (!cam_ok) {
            Serial.println ("no cam");
         } else if (capture) {
            Serial.println ("cam busy");
         } else {
            capture = 1;
            cam.flush_fifo();
            cam.clear_fifo_flag();
            cam.start_capture();
            Serial.println ("picture coming");
         }
       }
    }

    if (capture && cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
       Serial.print ("img ");
       uint32_t imglen = cam.read_fifo_length();
       Serial.print (imglen, HEX);
       Serial.println (":");
       cam.CS_LOW();
       cam.set_fifo_burst();
       uint8_t temp = 0;
       uint8_t temp_last = 0;

       while (imglen) {
          imglen--;
          temp_last = temp;
          temp = SPI.transfer(0x00);
          Serial.write(temp);
          if ((temp==0xD9) && (temp_last==0xFF)) break;
          delayMicroseconds(12);
       }

       cam.CS_HIGH();
       capture = 0;
    }
}


