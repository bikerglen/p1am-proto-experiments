#include <P1AM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <timer.h>
#include "charrom.h"
#include "display.h"
#include "samd21dmx.h"

Display display;
auto displayScrollTimer = timer_create_default (); 
auto mainLoopTimer = timer_create_default ();

#define IN1_PIN 3
#define IN2_PIN 4
#define OUT1_PIN 6
#define OUT2_PIN 7
#define DMX_DIR_PIN A0
#define E48_CS_PIN 2

// TODO -- read from serial EEPROM
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// TODO -- get via DHCP
IPAddress ip(192, 168, 180, 177);

EthernetServer server(80);

uint8_t effects_data[DMX_SLOTS];

String msg;
uint8_t in1_state = 0;
uint8_t in2_state = 0;
uint8_t out1_state = 0;
uint8_t out2_state = 0;

File myFile;

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
#ifdef NOPE
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#endif
  Serial.printf ("Hello, world!\n\r");

  // initialize opto inputs, relay outputs, EUI-48 EEPROM CS
  pinMode (IN1_PIN, INPUT);
  pinMode (IN2_PIN, INPUT);
  pinMode (OUT1_PIN, OUTPUT);
  digitalWrite (OUT1_PIN, 0);
  pinMode (OUT2_PIN, OUTPUT);
  digitalWrite (OUT2_PIN, 0);
  pinMode (E48_CS_PIN, OUTPUT);
  digitalWrite (E48_CS_PIN, 1);

  // initialize RS-485 / DMX
  pinMode (DMX_DIR_PIN, OUTPUT);
  digitalWrite (DMX_DIR_PIN, 1);
  dmx.begin ();
  for (int i = 0; i < DMX_SLOTS; i++) {
    effects_data[i] = 0x00;
  }
  dmx.tx (effects_data);

  // read MAC address
  // TODO

  // initialize display and display something
  display.begin (A1, A2, A5, A6);
  displayScrollTimer.every (200, displayTickWrapper);  
  display.scrollMessage ("Hello, world! ");

  // initialize main loop timer to execute at 50 Hz
  mainLoopTimer.every (20, mainLoopTick);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  Serial.print("Starting SD card...");
  if (!SD.begin(28)) {
    Serial.println("SD card start failed!");
    while (1);
  }
  Serial.println("SD card started."); 
}


void loop()
{
  displayScrollTimer.tick ();
  mainLoopTimer.tick ();

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          // client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("Hello, world!<br />");
          client.printf ("IN 1 is %d<br />", in1_state >= 2);
          client.printf ("IN 2 is %d<br />", in2_state >= 2);
          client.printf ("OUT 1 is %d<br />", out1_state);
          client.printf ("OUT 2 is %d<br />", out2_state);
          client.println ("test.txt contents: ");
          myFile = SD.open("test.txt", FILE_READ);
          while (myFile.available()) {
            client.write (myFile.read ());
          }
          client.print ("<br />");
          myFile.close();
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected"); 
  } 
}


bool displayTickWrapper (void *a)
{
  return display.tick (a);
}


bool mainLoopTick (void *a)
{
  uint8_t in1_raw = !digitalRead (IN1_PIN);
  uint8_t in2_raw = !digitalRead (IN2_PIN);
  uint8_t in1_pressed = 0;
  uint8_t in2_pressed = 0;

  switch (in1_state) {
    case 0:
      if (in1_raw) in1_state = 1;
      break;
    case 1:
      if (in1_raw) {
        in1_state = 2;
        in1_pressed = 1;
      }
      else in1_state = 0;
      break;
    case 2:
      if (!in1_raw) in1_state = 3;
      break;
    case 3:
      if (in1_raw) in1_state = 2;
      else in1_state = 0;
      break;
    default:
      in1_state = 0;
      break;
  }

  switch (in2_state) {
    case 0:
      if (in2_raw) in2_state = 1;
      break;
    case 1:
      if (in2_raw) {
        in2_state = 2;
        in2_pressed = 1;
      }
      else in2_state = 0;
      break;
    case 2:
      if (!in2_raw) in2_state = 3;
      break;
    case 3:
      if (in2_raw) in2_state = 2;
      else in2_state = 0;
      break;
    default:
      in2_state = 0;
      break;
  }

  uint8_t in1_down = in1_state >= 2;
  uint8_t in2_down = in2_state >= 2;

  if (in1_pressed) {
    out1_state = !out1_state;
  }
  
  if (in2_pressed) {
    out2_state = !out2_state;
  }

  digitalWrite (OUT1_PIN, out1_state);
  digitalWrite (OUT2_PIN, out2_state);
  
  msg = "";
  msg.concat (in1_down ? "1" : "0");
  msg.concat (in2_down ? "1" : "0");
  msg.concat (out1_state ? "1" : "0");
  msg.concat (out2_state ? "1" : "0");
  if (!display.scrollBusy ()) {
    display.write (msg);
  }

  effects_data[0] = out1_state ? 0xff : 0x00;
  effects_data[1] = out2_state ? 0xff : 0x00;
  dmx.tx (effects_data);

  return true;
}
