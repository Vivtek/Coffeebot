#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "ov2640_regs.h"

//#define HOSTCONN
//#include <AltSoftSerial.h>  // Uncomment all three lines to enable software serial debugger (bug in IDE)
//AltSoftSerial SWserial;

void handle_host ();
void xlog (char * origin, char * text);
void xlognum (char * origin, char * text, int number);
void w_handle_line ();
void rsend (char * msg);
void send_image_packet();

void state_0_1 ();
void state_1_2 ();
void state_2_3 ();
void state_3_4 ();
void state_4_5 ();
void state_5_6 ();
void state_6_7 ();
void state_7_6 ();
void state_8_7 ();

void handle_command (char * command);
void handle_command_ok (char * command);
void handle_command_pic (char * command);

const int CS = 6;
ArduCAM cam(OV2640, CS);
int cam_ok = 1;

int capture = 0;
uint32_t imglen;

void setup() 
{
    Serial.begin(115200);  // ESP8266
#if defined HOSTCONN
    SWserial.begin(9600);  // Host connection/log
#endif
    xlog ("H", "starting");
    pinMode(2, OUTPUT); // LED indicator
    pinMode(7, OUTPUT); // SS for motor ganglion
    pinMode(CS, OUTPUT);

    digitalWrite (7, HIGH);
    digitalWrite (CS, HIGH);
    
    Wire.begin();
    SPI.begin();
}

void cam_init() {
    uint8_t vid,pid,temp;
    
    cam.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = cam.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) { cam_ok = 0; }
    if (temp == 0x55) { xlog ("H","spi ok"); }
    cam.wrSensorReg8_8(0xff, 0x01);
    cam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    cam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26) || (pid != 0x42)) {
      cam_ok = 0;
      xlog ("H", "cam not ok");
    } else {
      xlog ("H", "cam ok, OV2640");
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
    handle_host();
 
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
        capture = 2;
        imglen = cam.read_fifo_length();
        xlognum ("H","image ready", imglen);
        cam.CS_LOW();
        cam.set_fifo_burst();
      }
      if (capture == 3) {
        rsend ("done");
        capture = 0;
      }

      if (capture == 2) {
        send_image_packet();
      }
    }
}

// Base64 encoder adapted from https://github.com/adamvr/arduino-base64
void base64_write(char * a3, int *i, int ch);
void base64_write_end(char * a3, int *i);
const char PROGMEM b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      	                            "abcdefghijklmnopqrstuvwxyz"
                                    "0123456789+/";

void send_image_packet() {
    xlognum ("H","packet",imglen);
    uint8_t temp, temp_last;
 
    char a3[3];
    int i = 0;
    int found_end = 0;

    uint32_t len = imglen;
    if (len > 1024) len = 1024; // Cap this at 1k at a time.

    Serial.print ("AT+CIPSEND=0,");
    Serial.print (((len+2)/3)*4+9);  // Break our packet into chunks of 3 bytes, base64-encode to 4-byte chunks.
    Serial.print ("\r\n");
    delay(10);
    Serial.print ("+img:\r\n");

    while (len) {
      imglen--;
      temp_last = temp;
      temp = SPI.transfer(0x00);
      base64_write (a3, &i, temp);
      if ((temp == 0xD9) && (temp_last == 0xFF)) break;
      delayMicroseconds(12);
      len--;
    }

    if (len) {
      xlognum ("H","imgend", len);
      found_end = 1;
      len--;
      while (len--) { base64_write(a3, &i, 0x00); }
    }
    base64_write_end(a3, &i);

    Serial.print ("\r\n");
    xlog("H","done sending");
    state = 8;

    if (found_end || !imglen) {
      xlog ("H","image done");
      cam.CS_HIGH();
      capture = 3; // Set state machine to send "done" after current packet is finished sending.
    }
}

inline void a3_to_a4(char * a4, char * a3) {
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}
void base64_write (char *a3, int *i, int ch) { 
  a3[*i] = ch; *i += 1;
  if (*i == 3) {
    char a4[4];
    a3_to_a4(a4, a3);
    Serial.write(pgm_read_byte(&b64_alphabet[a4[0]]));
    Serial.write(pgm_read_byte(&b64_alphabet[a4[1]]));
    Serial.write(pgm_read_byte(&b64_alphabet[a4[2]]));
    Serial.write(pgm_read_byte(&b64_alphabet[a4[3]]));
    *i = 0;
  }
}
void base64_write_end (char *a3, int *i) {
  if (!*i) return;
  int j;
  char a4[4];
  for (j=*i; j < 3; j++) {
    a3[j] = '\0';
  }
  a3_to_a4(a4, a3);
  for (j=0; j < *i + 1; j++) {
    Serial.write(pgm_read_byte(&b64_alphabet[a4[j]]));
  }
  while (*i < 3) {
    Serial.write('=');
    *i += 1;
  }
  *i=0;
}

void handle_host () {
#ifdef HOSTCONN
      if (SWserial.available() ) {
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
              xlog("H", "line too long, ignoring");
              hlineoff = 0;
           }
        }
      }
#endif
}

void xlog (char * origin, char * text) {
#ifdef HOSTCONN
    SWserial.print (origin);
    SWserial.print (":");
    SWserial.println (text);
#endif
}
void xlognum (char * origin, char * text, int number) {
#ifdef HOSTCONN
    SWserial.print (origin);
    SWserial.print (":");
    SWserial.print (text);
    SWserial.print (" ");
    SWserial.println (number);
#endif
}

void wsend (char * text) {
    Serial.print (text);
    Serial.print("\r\n");
}

void w_handle_line () {
    wlinebuf[wlineoff] = '\0';
    xlog("W", wlinebuf);

         if (state==0 && !strcmp (wlinebuf,"ready"))     { state_0_1 (); }
    else if (state==1 && !strcmp (wlinebuf,"OK"))        { state_1_2 (); }
    else if (state==2 && !strcmp (wlinebuf,"OK"))        { state_2_3 (); }
    else if (state==3 && !strcmp (wlinebuf,"OK"))        { state_3_4 (); }
    else if (state==4 && !strcmp (wlinebuf,"OK"))        { state_4_5 (); }
    else if (state==5 && !strcmp (wlinebuf,"OK"))        { state_5_6 (); }
    else if (state==6 && !strcmp (wlinebuf,"0,CONNECT")) { state_6_7 (); }
    else if (state==7 && !strcmp (wlinebuf,"0,CLOSED"))  { state_7_6 (); }
    else if (state==8 && !strcmp (wlinebuf,"SEND OK"))   { state_8_7 (); }
    
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
    cam_init();
    
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
    xlog("H","listening");
    digitalWrite(2,HIGH);
    state = 6;
}
void state_6_7 () {
    xlog("H", "connect");
    rsend ("Welcome to Coffeebot");
    state = 7;
}
void state_7_6 () {
    xlog("H", "disconnect");
    state = 6;
}
void state_8_7 () {
    state = 7;
}

void h_handle_line () {
    hlinebuf[hlineoff] = '\0';
    xlog (">", hlinebuf);
    wsend(hlinebuf);
    hlineoff = 0;
}

void wait_for_data () {
    unsigned long time = millis();
    while (millis() - time < 200) {
      if ( Serial.available() ) {
         int char_in = Serial.read();
         //SWserial.write(char_in);
         if (char_in == '>') {
            xlog ("H","waited");
            return;
         }
      }
      delay (1);
    }
    xlog ("H","timeout");
}
void rsend (char * msg) {
    Serial.print ("AT+CIPSEND=0,");
    Serial.print (strlen(msg)+4);
    Serial.print ("\r\n");
    delay (10);
    Serial.print (msg);
    Serial.print ("\r\n");
    Serial.print ("? ");
    state = 8;
}
void handle_command (char * command) {
    xlog ("!",command);
         if (!strcmp (command, "OK?")) { handle_command_ok(command); }
    else if (!strcmp (command, "pic")) { handle_command_pic(command); }
    else if (strlen (command) == 1) {
       digitalWrite(7, LOW); // Select motor ganglion on SPI bus
       SPI.transfer(command[0]);
       digitalWrite(7, HIGH);
    } else {
       rsend ("unknown");

       // Test code to base64-encode every unknown string and send it back.
       //Serial.print ("AT+CIPSEND=0,");
       //Serial.print (((strlen(command)+2)/3)*4+2);
       //Serial.print ("\r\n");
       //delay(10);
       //char a3[3];
       //int i=0;
       //int j;
       //for (j=0; j < strlen(command); j++) {
       //   base64_write (a3, &i, command[j]);
       //}
       //base64_write_end (a3, &i);
       //Serial.print ("\r\n");
       //state = 8;
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
