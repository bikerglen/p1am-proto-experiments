class Display
{
  private:
 
    uint8_t cs_pin, rs_pin, blank_pin, reset_pin;
	

  public:

	Display (void);

	~Display (void);

	void begin (uint8_t cs_pin, uint8_t rs_pin, uint8_t blank_pin, uint8_t reset_pin);

	void write (String s);

	void scrollMessage (String s);
	bool scrollBusy (void);

	bool tick (void *);
};
