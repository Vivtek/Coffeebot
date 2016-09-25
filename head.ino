#include <AltSoftSerial.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "ov2640_regs.h"

AltSoftSerial SWserial;

const int CS = 6;
ArduCAM cam(OV2640, CS);
int cam_ok = 1;

int capture = 0;
uint32_t imglen;

void setup() 
{
    uint8_t vid,pid,temp;
    
    Serial.begin(115200);  // ESP8266
    SWserial.begin(9600);  // Host connection/log
    log ("H", "starting");

    Wire.begin();
    pinMode(CS, OUTPUT);
    SPI.begin();
    cam.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = cam.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) { cam_ok = 0; }
    if (temp == 0x55) { log ("H","spi ok"); }
    cam.wrSensorReg8_8(0xff, 0x01);
    cam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    cam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26) || (pid != 0x42)) {
      cam_ok = 0;
      log ("H", "cam not ok");
    } else {
      log ("H", "cam ok, OV2640");
    }
    if (cam_ok) {
       cam.set_format(JPEG);
       cam.InitCAM();
       cam.OV2640_set_JPEG_size(OV2640_320x240);
       cam.clear_fifo_flag();
       cam.write_reg(ARDUCHIP_FRAMES, 0x00);
    }
}

char wlinebuf[256];
int  wlineoff = 0;

char hlinebuf[256];
int  hlineoff = 0;

int state = 0;

void loop() 
{
    // Mirror host input to the ESP8266
    if ( SWserial.available() ) {
      int char_in = SWserial.read();
      switch (char_in) {
        case '\n': break; // ignore linefeed
        case '\r':
           SWserial.print ("\n\r");
           h_handle_line();
           break;
        default:
           SWserial.write (char_in);  // Mirror to host for human consumption
           hlinebuf[hlineoff++] = char_in;
           if (hlineoff > 254) {
              log("H", "line too long, ignoring");
              hlineoff = 0;
           }
      }
    }
 
    // Collect input from the ESP8266 into lines and do things when appropriate.
    if ( Serial.available() ) {
      int char_in = Serial.read();
      switch (char_in) {
          case '\r': break;  // ignore carriage return
          case '\n':         // on linefeed, handle the line read so far.
             w_handle_line();
             break;
          default:
             wlinebuf[wlineoff++] = char_in;
             if (wlineoff > 254) {
                w_handle_line();
             }
      }
    }
    
    // If a capture is in progress, handle that state machine.
    if (state == 7 && capture) { // ignore if we're still sending something else.
      if (capture == 1 && cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
        log ("H","image ready");
        capture = 2;
        imglen = cam.read_fifo_length();
        cam.CS_LOW();
        cam.set_fifo_burst();
      }
      if (capture == 2) {
        log ("H","packet");
        uint8_t temp, temp_last;
        uint32_t len = imglen;
        int found_end = 0;
        if (len > 1024) len = 1024; // Cap this at 1k at a time.
        Serial.print ("AT+CIPSEND=0,");
        Serial.print (len+7);
        Serial.print ("\r\n");
        wait_for_data();
        Serial.print ("+img:");

        while (len--) {
          imglen--;
          temp_last = temp;
          temp = SPI.transfer(0x00);
          Serial.write(temp);
          if ((temp == 0xD9) && (temp_last == 0xFF)) break;
          delayMicroseconds(12);
        }
        if (len) {
          found_end = 1;
          //while (len--) { Serial.write(0x00); }
        }
        Serial.write("A");
        Serial.write("A"); // note: this is bad.
        Serial.write("A");
        Serial.write("A");
        Serial.write("A");
        Serial.write("A");
        Serial.write("A");
        Serial.write("A");
        Serial.write("A");
        log("H","done sending");
        Serial.print ("\r\n");
        state = 8;
        if (found_end || !imglen) {
          log ("H","image done");
          cam.CS_HIGH();
          capture = 0;
        }
      }
    }
}

void log (char * origin, char * text) {
    SWserial.print (origin);
    SWserial.print (":");
    SWserial.println (text);
}
void wsend (char * text) {
    Serial.print (text);
    Serial.print("\r\n");
}

void w_handle_line () {
    wlinebuf[wlineoff] = '\0';
    log("W", wlinebuf);

         if (state==0 && !strcmp (wlinebuf,"ready"))     { state_0_1 (); }
    else if (state==1 && !strcmp (wlinebuf,"OK"))        { state_1_2 (); }
    else if (state==2 && !strcmp (wlinebuf,"OK"))        { state_2_3 (); }
    else if (state==3 && !strcmp (wlinebuf,"OK"))        { state_3_4 (); }
    else if (state==4 && !strcmp (wlinebuf,"OK"))        { state_4_5 (); }
    else if (state==5 && !strcmp (wlinebuf,"OK"))        { state_5_6 (); }
    else if (state==6 && !strcmp (wlinebuf,"0,CONNECT")) { state_6_7 (); }
    else if (state==7 && !strcmp (wlinebuf,"0,CLOSED"))  { state_7_6 (); }
    else if (state==8 && !strcmp (wlinebuf,"OK"))        { state_8_7 (); }
    
    if (!strncmp (wlinebuf,"+IPD,",5)) {  // incoming command (or CR/LF)
        char * mark = wlinebuf + 6;
        while(*mark && *mark != ':') mark++;
        if (*mark) {
           mark++;
           char *mmark = mark;
           while (*mmark && *mmark != '\n') mmark++;
           if (*mmark == '\n') { *mmark = '\0'; }
           
           if (*mark) { handle_command (mark); }
        }
    }

    wlineoff = 0;
}

void state_0_1 () {
    wsend("AT+CWMODE=3");
    state = 1;
}
void state_1_2 () {
    wsend("AT+CWSAP=\"Coffeebot\",\"123\",3,0");
    state = 2;
}
void state_2_3 () {
    delay(50); // seems to need a little time here
    wsend("AT+CIPAP?");
    state = 3;
}
void state_3_4 () {
    wsend("AT+CIPMUX=1");
    state = 4;
}
void state_4_5 () {
    wsend("AT+CIPSERVER=1");
    state = 5;
}
void state_5_6 () {
    log("H","listening");
    state = 6;
}
void state_6_7 () {
    log("H", "connect");
    rsend ("Welcome to Coffeebot");
    state = 7;
}
void state_7_6 () {
    log("H", "disconnect");
    state = 6;
}
void state_8_7 () {
    state = 7;
}

void h_handle_line () {
    hlinebuf[hlineoff] = '\0';
    log (">", hlinebuf);
    wsend(hlinebuf);
    hlineoff = 0;
}

void wait_for_data () {
    unsigned long time = millis();
    while (millis() - time < 200) {
      if ( Serial.available() ) {
         int char_in = Serial.read();
         SWserial.write(char_in);
         if (char_in == '>') {
            log ("H","waited");
            return;
         }
      }
      delay (1);
    }
    log ("H","timeout");
}
void rsend (char * msg) {
    Serial.print ("AT+CIPSEND=0,");
    Serial.print (strlen(msg)+4);
    Serial.print ("\r\n");
    wait_for_data();
    Serial.print (msg);
    Serial.print ("\r\n");
    Serial.print ("? ");
    state = 8;
}
void handle_command (char * command) {
    log ("!",command);
         if (!strcmp (command, "OK?")) { handle_command_ok(command); }
    else if (!strcmp (command, "pic")) { handle_command_pic(command); }
    else {
       rsend ("unknown");
    }
}

void handle_command_ok (char * command) {
    rsend ("OK!");
}

void handle_command_pic (char * command) {
    if (!cam_ok) {
       rsend ("no cam");
       return;
    }
    if (capture) {
       rsend ("cam busy");
       return;
    }
    capture = 1;
    cam.flush_fifo();
    cam.clear_fifo_flag();
    cam.start_capture();

    rsend ("cap");
}
