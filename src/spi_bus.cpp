#include "sys_includes.h"
#include "service.h"
#include "system.h"

#include "spi_bus.h"
#include <SPI.h>

/*****************************************************************************************************************************
*
*   Read one byte from SPI compatible bus
*
*****************************************************************************************************************************/
uint8_t spiReadByte(volatile uint8_t* _sclkPortOutputRegister, const uint8_t _sclkPinBit, volatile uint8_t* _misoPortInputRegister, const uint8_t _misoPinBit) { 
  int8_t i;
  uint8_t value = 0x00;

  for (i = 0x07; i >= 0x00; i--) {
    *_sclkPortOutputRegister &= ~_sclkPinBit;
    _delay_ms(1);
    if (*_misoPortInputRegister & _misoPinBit) {
      //set the bit to 0 no matter what
      value |= (1 << i);
    }
    *_sclkPortOutputRegister |= _sclkPinBit;
    _delay_ms(1);
  }

  return value;
}


void spiWriteByte(const uint8_t _spiType, volatile uint8_t* _sclkPortOutputRegister, const uint8_t _sclkPinBit, volatile uint8_t* _mosiPortInputRegister, const uint8_t _mosiPinBit, const uint8_t _data) {
  if (SPI_TYPE_SOFTWARE == _spiType) {
     int8_t i = 0x08;
     while(i) {
       i--;                                      
       *_sclkPortOutputRegister &= ~_sclkPinBit;
       (_data & (0x01 << i)) ? *_mosiPortInputRegister |= _mosiPinBit : *_mosiPortInputRegister &= ~_mosiPinBit;
       *_sclkPortOutputRegister |= _sclkPinBit;                             
     }
  } else {
     SPI.transfer(_data);
  }
}
