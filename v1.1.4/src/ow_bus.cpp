#include "ow_bus.h"

/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
*
*                                                                     COMMON 1-WIRE SECTION
*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/


/*****************************************************************************************************************************
*
*   Scan 1-Wire bus and print to ethernet client ID's (Addresses) of all detected devices
*
*   Returns: 
*     - RESULT_IS_PRINTED on success
*     - RESULT_IS_FAIL of no devices found 
*
*****************************************************************************************************************************/
int8_t scanOneWire(const uint8_t _pin, EthernetClient *_ethClient) {
  uint8_t dsAddr[8], numDevices = 0, i;
  OneWire *owDevice;
  owDevice = new OneWire(_pin);
  owDevice->reset_search();
  delay(250);
  owDevice->reset();
  while (owDevice->search(dsAddr)) {
    numDevices++;
    _ethClient->print("0x");
    for (i = 0; i < arraySize(dsAddr); i++ ) {
      if (dsAddr[i] < 0x10){ _ethClient->print("0"); }
      _ethClient->print(dsAddr[i], HEX);
    }
    _ethClient->print('\n');
  }
  delete owDevice;
  return ((0 < numDevices) ? RESULT_IS_PRINTED : RESULT_IS_FAIL);
}


