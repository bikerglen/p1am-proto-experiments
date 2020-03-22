// loosely based on LXSAMD21DMX by Claude Heintz

#ifndef __samd21dmx_h_
#define __samd21dmx_h_

#define DMX_SLOTS       96

#define DMX_BREAK_BAUD  90000
#define DMX_DATA_BAUD   250000

#define DMX_STATE_BREAK 0
#define DMX_STATE_START 1
#define DMX_STATE_DATA 2
#define DMX_STATE_IDLE 3

class samd21dmx {

  public:

	samd21dmx ();
	~samd21dmx ();

	void begin (void);
	void tx (uint8_t *txslots);
	void transmissionComplete (void);
	void dataRegisterEmpty (void);

	bool busy;
	uint8_t state;
	uint16_t next_send_slot;
    uint8_t slots[DMX_SLOTS];
};

extern samd21dmx dmx;
extern Uart SerialDMX;

#endif
