#include <SPI.h>
#include "macaddr.h"

static SPISettings eepromSPISettings (1000000, MSBFIRST, SPI_MODE0);

void ReadMacAddress (uint8_t cs_pin, uint8_t *address)
{
  SPI.beginTransaction (eepromSPISettings);
  digitalWrite (cs_pin, 0);
  SPI.transfer (0x03);
  SPI.transfer (0xFA);
  for (uint8_t i = 0; i < 6; i++) {
    address[i] = SPI.transfer (0x00);
  }
  digitalWrite (cs_pin, 1);
  SPI.endTransaction ();
}
