//---------------------------------------------------------------------------------------------
// INCLUDES
//

#include <P1AM.h>
#include <SERCOM.h>
#include <Uart.h>
#include <SPI.h>
#include <timer.h>
#include "macaddr.h"


//---------------------------------------------------------------------------------------------
// DEFINES
//

#define PIN_E48_CS_n    2
#define PIN_XCVR_DE     1
#define PIN_XCVR_RE_n   0


//---------------------------------------------------------------------------------------------
// PROTOTYPES
//

bool tick_Run (void *);


//---------------------------------------------------------------------------------------------
// GLOBALS
//

// serial number / Ethernet MAC address
uint8_t MAC_ADDR[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// re-enable serial 1 because I commented out this code in the P1AM-100 variants.cpp file
Uart Serial1(&sercom5, PIN_SERIAL1_RX, PIN_SERIAL1_TX, PAD_SERIAL1_RX, PAD_SERIAL1_TX);
void SERCOM5_Handler() { Serial1.IrqHandler(); }

// 20 ms / 50 Hz tick timer
auto tickTimer = timer_create_default(); 


//---------------------------------------------------------------------------------------------
// setup
//

void setup (void)
{
  //----------------------------------------
  // initialize system
  //----------------------------------------

  // initialize built-in LED pin as an output
  pinMode (LED_BUILTIN, OUTPUT);
  digitalWrite (LED_BUILTIN, LOW);

  // intialize built-in toggle switch as an input
  pinMode (SWITCH_BUILTIN, INPUT);

  // deassert serial eeprom chip select
  pinMode (PIN_E48_CS_n, OUTPUT);
  digitalWrite (PIN_E48_CS_n, HIGH);

  // enable transceiver output
  pinMode (PIN_XCVR_DE, OUTPUT);
  digitalWrite (PIN_XCVR_DE, HIGH);

  // disable receiver input
  pinMode (PIN_XCVR_RE_n, OUTPUT);
  digitalWrite (PIN_XCVR_RE_n, HIGH);

  // initialize SPI
  SPI.begin ();
  
  // enable console serial port
  Serial.begin (9600);

  // enable transceiver serial port
  Serial1.begin (9600);

  // wait 500 ms for serial port to open
  delay (500);

  // display hello world
  Serial.printf ("Hello, world!\n\r");
  
  //----------------------------------------
  // read and display serial number / Ethernet MAC address
  //----------------------------------------

  ReadMacAddress (PIN_E48_CS_n, MAC_ADDR);
  Serial.printf ("Board serial number / MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n\r",
      MAC_ADDR[0], MAC_ADDR[1], MAC_ADDR[2], MAC_ADDR[3], MAC_ADDR[4], MAC_ADDR[5]);

  //----------------------------------------
  // initialize P1 bus modules
  //----------------------------------------

  while (!P1.init()){ 
    ; //Wait for Modules to Sign on   
  }

  //----------------------------------------
  // initialize tick timer
  //----------------------------------------
  
  tickTimer.every (20, tick_Run);
}


//---------------------------------------------------------------------------------------------
// loop
//

void loop (void)
{
  uint8_t ch;

  tickTimer.tick ();

  if (digitalRead (SWITCH_BUILTIN)) {
    // turn built-in LED on
    digitalWrite (LED_BUILTIN, HIGH);
  } else {
    // turn built-in LED off
    digitalWrite (LED_BUILTIN, LOW);
  }
}


//---------------------------------------------------------------------------------------------
// tick_Run
//

bool tick_Run (void *)
{
  uint8_t ioModuleStatus;

  // read slot 1
  ioModuleStatus = P1.readDiscrete (1);

  // send to console serial port
  Serial.printf ("%02x\n\r", ioModuleStatus);

  // send over RS-422 / RS-485 to remote module
  Serial1.write (ioModuleStatus);

  return true;
}
