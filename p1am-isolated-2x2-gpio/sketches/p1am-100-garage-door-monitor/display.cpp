#include <P1AM.h>
#include <RingBuffer.h>
#include <timer.h>
#include "charrom.h"
#include "display.h"

static SPISettings displaySPISettings (1000000, MSBFIRST, SPI_MODE0);

uint8_t scrollBusyFlag = 0;
uint8_t scrollIndex = 0;
uint8_t scrollBuffer[5];
RingBuffer scrollMessageBuffer;
extern Timer<> displayScrollTimer;
extern Timer<> mainLoopTimer;

//---------------------------------------------------------------------------------------------
// constructor
//

Display::Display (void)
{
}


//---------------------------------------------------------------------------------------------
// destructor
//

Display::~Display (void)
{
}


//---------------------------------------------------------------------------------------------
// initialize everything
//

void Display::begin (uint8_t cs_pin, uint8_t rs_pin, uint8_t blank_pin, uint8_t reset_pin)
{
	// save pins
	this->cs_pin = cs_pin;
	this->rs_pin = rs_pin;
	this->blank_pin = blank_pin;
	this->reset_pin = reset_pin;

	// initialize SPI
	SPI.begin ();

	// set pins as outputs
	pinMode (this->cs_pin, OUTPUT);
	pinMode (this->rs_pin, OUTPUT);
	pinMode (this->blank_pin, OUTPUT);
	pinMode (this->reset_pin, OUTPUT);

	// set pin defaults
	digitalWrite (this->blank_pin, 1);
	digitalWrite (this->cs_pin, 1);
	digitalWrite (this->rs_pin, 0);

	// reset display
	digitalWrite (this->reset_pin, 0);
	delay (100);
	digitalWrite (this->reset_pin, 1);
	delay (100);

	// configure display
	SPI.beginTransaction (displaySPISettings);
	digitalWrite (this->rs_pin, 1);
	digitalWrite (this->cs_pin, 0);
	SPI.transfer (0x7F);
	digitalWrite (this->cs_pin, 1);
	delay (5);
	digitalWrite (this->rs_pin, 1);
	digitalWrite (this->cs_pin, 0);
	SPI.transfer (0x80);
	digitalWrite (this->cs_pin, 1);
	SPI.endTransaction ();
	
	// clear display data
	SPI.beginTransaction (displaySPISettings);
	digitalWrite (this->rs_pin, 0);
	digitalWrite (this->cs_pin, 0);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 5; j++) {
			SPI.transfer (0x00);
		}
	}
	digitalWrite (this->cs_pin, 1);
	SPI.endTransaction ();

/*
	// display some data
	SPI.beginTransaction (displaySPISettings);
	digitalWrite (this->rs_pin, 0);
	digitalWrite (this->cs_pin, 0);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 5; j++) {
			SPI.transfer (charrom['A'+i][j]);
		}
	}
	digitalWrite (this->cs_pin, 1);
	SPI.endTransaction ();
*/

	// unblank display
	digitalWrite (this->blank_pin, 0);

	// initializie scrolling message state and buffer
	scrollBuffer[0] = ' ';
	scrollBuffer[1] = ' ';
	scrollBuffer[2] = ' ';
	scrollBuffer[3] = ' ';
	scrollBuffer[4] = ' ';
	scrollMessageBuffer.clear ();
}


//---------------------------------------------------------------------------------------------
// display a message immediately
// shouldn't be called while scrollBusyFlag is set
//

void Display::write (String s)
{
	uint8_t i, j, ch;

	// display some data
	SPI.beginTransaction (displaySPISettings);
	digitalWrite (this->rs_pin, 0);
	digitalWrite (this->cs_pin, 0);
	for (int i = 0; i < 4; i++) {
		if (i >= s.length ()) {
			ch = ' ';
		} else {
			ch = s.charAt (i);
			if (ch >= 128) {
				ch = ' ';
			}
		}
		for (int j = 0; j < 5; j++) {
			SPI.transfer (charrom[ch][j]);
		}
	}
	digitalWrite (this->cs_pin, 1);
	SPI.endTransaction ();
}


//---------------------------------------------------------------------------------------------
// add message to scrolling message buffer
// if buffer is full, blocks until not full any more
// calls display scroll and main loop timer tick functions so timers
// continue to operate while blocked
//

void Display::scrollMessage (String s, bool reset)
{
	int i;

	scrollBusyFlag = 2;

  if (reset) {
    scrollMessageBuffer.clear ();
  }
  
	for (i = 0; i < s.length (); i++) {
		while (!scrollMessageBuffer.availableForStore ()) {
			displayScrollTimer.tick ();
      // mainLoopTimer.tick ();
		}
		scrollMessageBuffer.store_char (s.charAt (i));
	}
}


#ifdef NOPE
//---------------------------------------------------------------------------------------------
// scroll column by column
//

// This scrolling doesn't look good because each character is exactly 5x7 and
// they squish together when scrolled then unsuish with a bit of space 
// between them when column = 0. Going to scroll by whole character only.

bool Display::tick (void *)
{
	uint8_t i, adj, idx, col, ch;

	SPI.beginTransaction (displaySPISettings);
	digitalWrite (this->rs_pin, 0);
	digitalWrite (this->cs_pin, 0);

	// Serial.printf ("scrollIndex: %d\n\r", scrollIndex);
	// Serial.printf ("scrollBuffer: '%c%c%c%c%c\n\r'",
	//		scrollBuffer[0], scrollBuffer[1], scrollBuffer[2], scrollBuffer[3], scrollBuffer[4]);

	for (int i = 0; i < 20; i++) {

		adj = i + scrollIndex;
		idx = adj / 5;
		col = adj % 5;
		ch = scrollBuffer[idx];
		SPI.transfer (charrom[ch][col]);
		// Serial.printf ("   %2d %2d %2d\n\r", adj, idx, col);
	}

	digitalWrite (this->cs_pin, 1);
	SPI.endTransaction ();

	scrollIndex++;
	if (scrollIndex >= 5) {
		scrollIndex = 0;
		for (i = 0; i < 4; i++) {
			scrollBuffer[i] = scrollBuffer[i+1];
		}
		if (scrollMessageBuffer.available ()) {
			scrollBuffer[4] = scrollMessageBuffer.read_char ();
		} else {
			scrollBuffer[4] = ' ';
			if ((scrollBuffer[0] == ' ') && (scrollBuffer[1] == ' ') && (scrollBuffer[2] == ' ') &&
					(scrollBuffer[3] == ' ') && (scrollBuffer[4] == ' ')) {
				scrollBusyFlag = false;
			}
		}
	}

	return true;
}
#endif


//---------------------------------------------------------------------------------------------
// scroll character by character
//

bool Display::tick (void *)
{
	uint8_t i, j;

  if (scrollBusyFlag) {
  	SPI.beginTransaction (displaySPISettings);
  	digitalWrite (this->rs_pin, 0);
  	digitalWrite (this->cs_pin, 0);
  
  	for (int i = 0; i < 4; i++) {
  		for (int j = 0; j < 5; j++) {
  			SPI.transfer (charrom[scrollBuffer[i]][j]);
  		}
  	}
  
  	digitalWrite (this->cs_pin, 1);
  	SPI.endTransaction ();
  
  	for (i = 0; i < 4; i++) {
  		scrollBuffer[i] = scrollBuffer[i+1];
  	}
  	if (scrollMessageBuffer.available ()) {
  		scrollBuffer[4] = scrollMessageBuffer.read_char ();
      // Serial.printf ("1: %c%c%c%c%c %d\n\r", scrollBuffer[0], scrollBuffer[1], scrollBuffer[2], scrollBuffer[3], scrollBuffer[4], scrollBusyFlag);
  	} else {
  		scrollBuffer[4] = ' ';
  		if ((scrollBuffer[0] == ' ') && (scrollBuffer[1] == ' ') && (scrollBuffer[2] == ' ') &&
  				(scrollBuffer[3] == ' ') && (scrollBuffer[4] == ' ')) {
  			scrollBusyFlag--;
  		}
      // Serial.printf ("2: %c%c%c%c%c %d\n\r", scrollBuffer[0], scrollBuffer[1], scrollBuffer[2], scrollBuffer[3], scrollBuffer[4], scrollBusyFlag);
  	}
  }

	return true;
}


//---------------------------------------------------------------------------------------------
// return state of scrollBusyFlag
//

bool Display::scrollBusy (void)
{
	return (scrollBusyFlag != 0);
}


void Display::heartbeat (uint8_t col, uint8_t row)
{
  SPI.beginTransaction (displaySPISettings);
  digitalWrite (this->rs_pin, 0);
  digitalWrite (this->cs_pin, 0);
  for (uint8_t i = 0; i < 20; i++) {
    if (i != col) {
      SPI.transfer (0x00);
    } else {
      SPI.transfer (0x01 << row);
    }
  }
  digitalWrite (this->cs_pin, 1);
  SPI.endTransaction ();
}
