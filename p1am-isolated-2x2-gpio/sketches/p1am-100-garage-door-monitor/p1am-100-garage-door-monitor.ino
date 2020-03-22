#include <P1AM.h>
#include <timer.h>
#include <SPI.h>
#include <Ethernet.h>
#include "macaddr.h"
#include "charrom.h"
#include "display.h"

#define RS485_DIR_PIN A0
#define EUI48_CS_PIN   2
#define IN1_PIN        3
#define IN2_PIN        4
#define OUT1_PIN       6
#define OUT2_PIN       7

Display display;
auto displayScrollTimer = timer_create_default (); 
bool displayTickWrapper (void *a);
auto mainLoopTimer = timer_create_default ();
bool mainLoopTick (void *a);

uint8_t in1_state = 0;
uint8_t in2_state = 0;
uint8_t out1_state = 0;
uint8_t out2_state = 0;
uint8_t heartbeat_timer = 0;
uint8_t heartbeat_state = 0;

bool lightsOffTimerEnabled = false;
uint16_t lightsOffTimer = 0;

// This MAC address is overwritten with the value from the EUI-48 serial EEPROM
uint8_t mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip (192,168,180,177);
IPAddress dns (1,1,1,1);
EthernetClient client;
void SetDoorOpenLight (bool state);
void SetOverheadLights (bool state);

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
#ifndef NOPE
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#endif
  Serial.printf ("Hello, world!\n\r");

  // initialize opto inputs, relay outputs
  pinMode (IN1_PIN, INPUT);
  pinMode (IN2_PIN, INPUT);
  pinMode (OUT1_PIN, OUTPUT);
  digitalWrite (OUT1_PIN, 0);
  pinMode (OUT2_PIN, OUTPUT);
  digitalWrite (OUT2_PIN, 0);
  pinMode (LED_BUILTIN, OUTPUT);
  digitalWrite (LED_BUILTIN, 0);

  // initialize RS-485 direction pin
  pinMode (RS485_DIR_PIN, OUTPUT);
  digitalWrite (RS485_DIR_PIN, 1);

  // initialize EUI-48 chip select pin
  pinMode (EUI48_CS_PIN, OUTPUT);
  digitalWrite (EUI48_CS_PIN, 1);

  // initialize display then display hello world message
  display.begin (A1, A2, A5, A6);
  displayScrollTimer.every (200, displayTickWrapper);  
  display.scrollMessage ("Hello, world! ", true);

  // initialize main loop timer to execute at 50 Hz
  mainLoopTimer.every (20, mainLoopTick);

  // initialize input state variables based on state of garage door
  // so that things don't trigger immediately after power up
  in1_state = (!digitalRead (IN1_PIN)) ? 2 : 0;
  in2_state = (!digitalRead (IN2_PIN)) ? 2 : 0;

  // read mac address from serial EEPROM
  ReadMacAddress (EUI48_CS_PIN, mac);
  Serial.printf ("%02x:%02x:%02x:%02x:%02x:%02x\n\r", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  char s[32];
  snprintf (s, 32, "MAC: %02x:%02x:%02x:%02x:%02x:%02x ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  display.scrollMessage (s, false);

  // initialize Ethernet
  Ethernet.begin (mac);

  IPAddress ip = Ethernet.localIP ();
  snprintf (s, 32, "IP: %d.%d.%d.%d ", ip[0], ip[1], ip[2], ip[3]);
  display.scrollMessage (s, false);
  
  ip = Ethernet.dnsServerIP ();
  snprintf (s, 32, "DNS: %d.%d.%d.%d ", ip[0], ip[1], ip[2], ip[3]);
  display.scrollMessage (s, false);
}


void loop()
{
  displayScrollTimer.tick ();
  mainLoopTimer.tick ();
}


bool displayTickWrapper (void *a)
{
  return display.tick (a);
}


bool mainLoopTick (void *a)
{
  //========================================
  // read inputs and debounce
  //========================================

  // read inputs
  uint8_t in1_raw = !digitalRead (IN1_PIN);
  uint8_t in2_raw = !digitalRead (IN2_PIN);

  // set state change triggers to no action
  uint8_t in1_turned_on = 0;
  uint8_t in2_turned_on = 0;
  uint8_t in1_turned_off = 0;
  uint8_t in2_turned_off = 0;

  // debounce input 1 and detect state changes
  switch (in1_state) {
    case 0:
      if (in1_raw) {
        in1_state = 1;
      }
      break;
    case 1:
      if (in1_raw) {
        in1_state = 2;
        in1_turned_on = 1;
      } else {
        in1_state = 0;
      }
      break;
    case 2:
      if (!in1_raw) {
        in1_state = 3;
      }
      break;
    case 3:
      if (in1_raw) {
        in1_state = 2;
      } else {
        in1_state = 0;
        in1_turned_off = 1;
      }
      break;
    default:
      in1_state = 0;
      break;
  }

  // debounce input 2
  switch (in2_state) {
    case 0:
      if (in2_raw) {
        in2_state = 1;
      }
      break;
    case 1:
      if (in2_raw) {
        in2_state = 2;
        in2_turned_on = 1;
      } else {
        in2_state = 0;
      }
      break;
    case 2:
      if (!in2_raw) {
        in2_state = 3;
      }
      break;
    case 3:
      if (in2_raw) {
        in2_state = 2;
      } else {
        in2_state = 0;
        in2_turned_off = 1;
      }
      break;
    default:
      in2_state = 0;
      break;
  }

  // compute on/off state of inputs from debounce state machines
  uint8_t in1_on = in1_state >= 2;
  uint8_t in2_on = in2_state >= 2;

  // builtin led is on when door is open
  digitalWrite (LED_BUILTIN, !in1_on);

  // check if door closed
  if (in1_turned_on) {
    display.scrollMessage ("Garage door closed.", true);

    // turn off indoor door open warning light
    SetDoorOpenLight (false);

    // enable lights off timer
    lightsOffTimerEnabled = true;
    lightsOffTimer = 5 * 60 * 50; // 5 minutes with a 50 Hz tick rate

    // send an IFTTT maker event
    // TODO
  }

  // check if door open
  if (in1_turned_off) {
    display.scrollMessage ("Garage door opened.", true);

    // turn on indoor door open warning light
    SetDoorOpenLight (true);

    // turn on overhead lights
    SetOverheadLights (true);

    // disable lights off timer
    lightsOffTimerEnabled = false;
    lightsOffTimer = 0;

    // send a text message
    // TODO

    // send an IFTTT maker event
    // TODO
  }

  // run timer and turn off overhead lights after five minutes
  if (lightsOffTimerEnabled) {
    lightsOffTimer--;
    if (lightsOffTimer == 0) {
      lightsOffTimerEnabled = false;
      display.scrollMessage ("Turning off overhead lights.", true);
      SetOverheadLights (false);
    } else {
      if (!display.scrollBusy ()) {
        uint16_t seconds = lightsOffTimer / 50;
        uint16_t minutes = seconds / 60;
        seconds = seconds % 60;
        char s[5];
        snprintf (s, 5, "%01d:%02d", minutes, seconds);
        String msg = s;
        display.write (msg);
      }
    }
  }

  // increment heartbeat state and send data to display 
  // if nothing else going on
  heartbeat_timer++;
  if (heartbeat_timer >= 4) {
    heartbeat_timer = 0;
    heartbeat_state++;
    if (heartbeat_state >= 140) {
      heartbeat_state = 0;
    }
  
    if (!display.scrollBusy () && !lightsOffTimerEnabled) {
      uint8_t col = heartbeat_state / 7;
      uint8_t row = heartbeat_state % 7;
      display.heartbeat (col, row);    
    }
  }
  
  // eturn true to continue running timer
  return true;
}


void SetDoorOpenLight (bool state)
{
  client.connect (IPAddress (192, 168, 180, 92), 2000);
  if (state) {
    // on
    client.print ("\r\n#ffffff\r\n#ffffff\r\n");
  } else {
    // off
    client.print ("\r\n#000000\r\n#000000\r\n");
  }
  delay (1);
  client.stop ();
}


void SetOverheadLights (bool state)
{
  client.connect (IPAddress (192, 168, 180, 20), 80);
  if (state) {
    // on
    client.println ("GET /rest/nodes/3D%202E%2014%201/cmd/DON HTTP/1.1");
  } else {
    // off
    client.println ("GET /rest/nodes/3D%202E%2014%201/cmd/DOF HTTP/1.1");
  }
  client.println ("Authorization: Basic YWRtaW46eDNmZjcyaGs=");
  client.println ("Host: 192.168.180.20");
  client.println ("Accept: \"/\"");
  client.println ();
  int startTime = millis ();
  int currTime;
  do {
    if (client.available ()) {
      char c = client.read ();
    }
    currTime = millis ();
  } while (client.connected () && (abs (currTime - startTime) < 1000));
  client.stop ();
}
