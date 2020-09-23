/*

Todo: https://github.com/ToniA/arduino-heatpumpir

Based on: https://github.com/z3t0/Arduino-IRremote
version 2.2.1 is used

 !!! BEWARE !!! 
 Not all Arduino IRremote code is implemented to Zabbuino. Compilation may stops or hangs.
 #define's for Timer2 & Timer4  are included only to this .ino file. 

*/

#if defined(ARDUINO_ARCH_AVR)

// Config & common included files
#include "sys_includes.h"

#include "service.h"

#include "ir.h"


#ifdef FEATURE_IR_ENABLE
static uint8_t sendRawByIR(const uint16_t _frequency, unsigned int _nBits, const char* _data)
{
   uint16_t i=0, packet;

   if (!haveHexPrefix(_data)) {
      return false;
   }

    // Skip "0x"
    _data += 2; 

    // Set IR carrier frequency
    enableIROut(_frequency);

    while((*_data && *(_data+1) && *(_data+2) && *(_data+3)) || (i < _nBits)) {
      // restore full int from HEX nibbles
      packet = htod(*_data);
      packet <<= 4;
      packet = htod(*(_data+1));
      packet <<= 4;
      packet = htod(*(_data+2));
      packet <<= 4;
      packet = htod(*(_data+3));
      if (i & 1)  space(packet) ;
      else        mark (packet) ;
      i++;
    }
    space(0);  // Always end with the LED off
    return RESULT_IS_OK;
    
}

//+=============================================================================
// Sends an IR mark for the specified number of microseconds.
// The mark output is modulated at the PWM frequency.
//
static void mark (unsigned int time)
{
	TIMER_ENABLE_PWM; // Enable pin 3 PWM output
	if (time > 0) custom_delay_usec(time);
}

//+=============================================================================
// Leave pin off for time (given in microseconds)
// Sends an IR space for the specified number of microseconds.
// A space is no output, so the PWM output is disabled.
//
static void space (unsigned int time)
{
	TIMER_DISABLE_PWM; // Disable pin 3 PWM output
	if (time > 0) custom_delay_usec(time);
}

//+=============================================================================
// Enables IR output.  The khz value controls the modulation frequency in kilohertz.
// The IR output will be on pin 3 (OC2B).
// This routine is designed for 36-40KHz; if you use it for other values, it's up to you
// to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
// TIMER2 is used in phase-correct PWM mode, with OCR2A controlling the frequency and OCR2B
// controlling the duty cycle.
// There is no prescaling, so the output frequency is 16MHz / (2 * OCR2A)
// To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
// A few hours staring at the ATmega documentation and this will all make sense.
// See my Secrets of Arduino PWM at http://arcfn.com/2009/07/secrets-of-arduino-pwm.html for details.
//
static void  enableIROut (int khz)
{
//        extern uint8_t irPWMPin
  	// Disable the Timer2 Interrupt (which is used for receiving IR)
	TIMER_DISABLE_INTR; //Timer2 Overflow Interrupt

//	pinMode(irPWMPin, OUTPUT);
	pinMode(TIMER_PWM_PIN, OUTPUT);
	digitalWrite(TIMER_PWM_PIN, LOW); // When not sending PWM, we want it low

	// COM2A = 00: disconnect OC2A
	// COM2B = 00: disconnect OC2B; to send signal set to 10: OC2B non-inverted
	// WGM2 = 101: phase-correct PWM with OCRA as top
	// CS2  = 000: no prescaling
	// The top value for the timer.  The modulation frequency will be SYSCLOCK / 2 / OCR2A.
	TIMER_CONFIG_KHZ(khz);
}

//+=============================================================================
// Custom delay function that circumvents Arduino's delayMicroseconds limit
static void custom_delay_usec(unsigned long uSecs) {

  if (uSecs > 4) {
    unsigned long start = micros();
    unsigned long endMicros = start + uSecs - 4;
    if (endMicros < start) { // Check if overflow
      while ( micros() > start ) {} // wait until overflow
    }
    while ( micros() < endMicros ) {} // normal wait
  } 
  //else {
  //  __asm__("nop\n\t"); // must have or compiler optimizes out
  //}

}
#endif

#ifdef SUPPORT_IR_RC5

//==============================================================================
// RRRR    CCCC  55555
// R   R  C      5
// RRRR   C      5555
// R  R   C          5
// R   R   CCCC  5555
//
// NB: First bit must be a one (start bit)
//

//+=============================================================================
static void  sendRC5 (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(36);

	// Start
	mark(RC5_T1);
	space(RC5_T1);
	mark(RC5_T1);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			space(RC5_T1); // 1 is space, then mark
			mark(RC5_T1);
		} else {
			mark(RC5_T1);
			space(RC5_T1);
		}
	}

	space(0);  // Always end with the LED off
}
#endif

#ifdef SUPPORT_IR_RC6
//+=============================================================================
// RRRR    CCCC   6666
// R   R  C      6
// RRRR   C      6666
// R  R   C      6   6
// R   R   CCCC   666
//
// NB : Caller needs to take care of flipping the toggle bit
//

static void  sendRC6 (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(36);

	// Header
	mark(RC6_HDR_MARK);
	space(RC6_HDR_SPACE);

	// Start bit
	mark(RC6_T1);
	space(RC6_T1);

	// Data
	for (unsigned long  i = 1, mask = 1UL << (nbits - 1);  mask;  i++, mask >>= 1) {
		// The fourth bit we send is a "double width trailer bit"
		int  t = (i == 4) ? (RC6_T1 * 2) : (RC6_T1) ;
		if (data & mask) {
			mark(t);
			space(t);
		} else {
			space(t);
			mark(t);
		}
	}

	space(0);  // Always end with the LED off
}
#endif

#ifdef SUPPORT_IR_NEC
//==============================================================================
//                           N   N  EEEEE   CCCC
//                           NN  N  E      C
//                           N N N  EEE    C
//                           N  NN  E      C
//                           N   N  EEEEE   CCCC
//==============================================================================

//+=============================================================================
static void  sendNEC (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Header
	mark(NEC_HDR_MARK);
	space(NEC_HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(NEC_BIT_MARK);
			space(NEC_ONE_SPACE);
		} else {
			mark(NEC_BIT_MARK);
			space(NEC_ZERO_SPACE);
		}
	}

	// Footer
	mark(NEC_BIT_MARK);
	space(0);  // Always end with the LED off
}
#endif

#ifdef SUPPORT_IR_SONY
//==============================================================================
//                           SSSS   OOO   N   N  Y   Y
//                          S      O   O  NN  N   Y Y
//                           SSS   O   O  N N N    Y
//                              S  O   O  N  NN    Y
//                          SSSS    OOO   N   N    Y
//==============================================================================

//+=============================================================================
static void  sendSony (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(40);

	// Header
	mark(SONY_HDR_MARK);
	space(SONY_HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(SONY_ONE_MARK);
			space(SONY_HDR_SPACE);
		} else {
			mark(SONY_ZERO_MARK);
			space(SONY_HDR_SPACE);
    	}
  	}

	// We will have ended with LED off
}
#endif

#ifdef SUPPORT_IR_SAMSUNG
//==============================================================================
//              SSSS   AAA    MMM    SSSS  U   U  N   N   GGGG
//             S      A   A  M M M  S      U   U  NN  N  G
//              SSS   AAAAA  M M M   SSS   U   U  N N N  G  GG
//                 S  A   A  M   M      S  U   U  N  NN  G   G
//             SSSS   A   A  M   M  SSSS    UUU   N   N   GGG
//==============================================================================

//+=============================================================================
static void  sendSAMSUNG (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Header
	mark(SAMSUNG_HDR_MARK);
	space(SAMSUNG_HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(SAMSUNG_BIT_MARK);
			space(SAMSUNG_ONE_SPACE);
		} else {
			mark(SAMSUNG_BIT_MARK);
			space(SAMSUNG_ZERO_SPACE);
		}
	}

	// Footer
	mark(SAMSUNG_BIT_MARK);
    space(0);  // Always end with the LED off
}
#endif


#ifdef SUPPORT_IR_WHYNTER
//==============================================================================
//               W   W  H   H  Y   Y N   N TTTTT EEEEE  RRRRR
//               W   W  H   H   Y Y  NN  N   T   E      R   R
//               W W W  HHHHH    Y   N N N   T   EEE    RRRR
//               W W W  H   H    Y   N  NN   T   E      R  R
//                WWW   H   H    Y   N   N   T   EEEEE  R   R
//==============================================================================

//+=============================================================================
static void  sendWhynter (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Start
	mark(WHYNTER_ZERO_MARK);
	space(WHYNTER_ZERO_SPACE);

	// Header
	mark(WHYNTER_HDR_MARK);
	space(WHYNTER_HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(WHYNTER_ONE_MARK);
			space(WHYNTER_ONE_SPACE);
		} else {
			mark(WHYNTER_ZERO_MARK);
			space(WHYNTER_ZERO_SPACE);
		}
	}

	// Footer
	mark(WHYNTER_ZERO_MARK);
	space(WHYNTER_ZERO_SPACE);  // Always end with the LED off
}
#endif


#ifdef SUPPORT_IR_LG
//==============================================================================
//                               L       GGGG
//                               L      G
//                               L      G  GG
//                               L      G   G
//                               LLLLL   GGG
//==============================================================================

//+=============================================================================
static void  sendLG (unsigned long data,  int nbits)
{
    // Set IR carrier frequency
    enableIROut(38);

    // Header
    mark(LG_HDR_MARK);
    space(LG_HDR_SPACE);
    mark(LG_BIT_MARK);

    // Data
    for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
        if (data & mask) {
            space(LG_ONE_SPACE);
            mark(LG_BIT_MARK);
        } else {
            space(LG_ZERO_SPACE);
            mark(LG_BIT_MARK);
        }
    }
    space(0);  // Always end with the LED off
}
#endif

#ifdef SUPPORT_IR_DISH
//==============================================================================
//                       DDDD   IIIII   SSSS  H   H
//                        D  D    I    S      H   H
//                        D  D    I     SSS   HHHHH
//                        D  D    I        S  H   H
//                       DDDD   IIIII  SSSS   H   H
//==============================================================================

// Sharp and DISH support by Todd Treece ( http://unionbridge.org/design/ircommand )
//
// The sned function needs to be repeated 4 times
//
// Only send the last for characters of the hex.
// I.E.  Use 0x1C10 instead of 0x0000000000001C10 as listed in the LIRC file.
//
// Here is the LIRC file I found that seems to match the remote codes from the
// oscilloscope:
//   DISH NETWORK (echostar 301):
//   http://lirc.sourceforge.net/remotes/echostar/301_501_3100_5100_58xx_59xx

//+=============================================================================
static void  sendDISH (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(56);

	mark(DISH_HDR_MARK);
	space(DISH_HDR_SPACE);

	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(DISH_BIT_MARK);
			space(DISH_ONE_SPACE);
		} else {
			mark(DISH_BIT_MARK);
			space(DISH_ZERO_SPACE);
		}
	}
	mark(DISH_HDR_MARK); //added 26th March 2016, by AnalysIR ( https://www.AnalysIR.com )
}
#endif

#ifdef SUPPORT_IR_SHARP
//==============================================================================
//                       SSSS  H   H   AAA   RRRR   PPPP
//                      S      H   H  A   A  R   R  P   P
//                       SSS   HHHHH  AAAAA  RRRR   PPPP
//                          S  H   H  A   A  R  R   P
//                      SSSS   H   H  A   A  R   R  P
//==============================================================================

// Sharp and DISH support by Todd Treece: http://unionbridge.org/design/ircommand
//
// The send function has the necessary repeat built in because of the need to
// invert the signal.
//
// Sharp protocol documentation:
//   http://www.sbprojects.com/knowledge/ir/sharp.htm
//
// Here is the LIRC file I found that seems to match the remote codes from the
// oscilloscope:
//   Sharp LCD TV:
//   http://lirc.sourceforge.net/remotes/sharp/GA538WJSA


//+=============================================================================
static void  sendSharpRaw (unsigned long data,  int nbits)
{
	enableIROut(38);

	// Sending codes in bursts of 3 (normal, inverted, normal) makes transmission
	// much more reliable. That's the exact behaviour of CD-S6470 remote control.
	for (int n = 0;  n < 3;  n++) {
		for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
			if (data & mask) {
				mark(SHARP_BIT_MARK);
				space(SHARP_ONE_SPACE);
			} else {
				mark(SHARP_BIT_MARK);
				space(SHARP_ZERO_SPACE);
			}
		}

		mark(SHARP_BIT_MARK);
		space(SHARP_ZERO_SPACE);
		delay(40);

		data = data ^ SHARP_TOGGLE_MASK;
	}
}

//+=============================================================================
// Sharp send compatible with data obtained through decodeSharp()
//                                                  ^^^^^^^^^^^^^ FUNCTION MISSING!
//
void  sendSharp (unsigned int address,  unsigned int command)
{
	sendSharpRaw((address << 10) | (command << 2) | 2, SHARP_BITS);
}
#endif



#ifdef SUPPORT_IR_DENON
// Reverse Engineered by looking at RAW dumps generated by IRremote

// I have since discovered that Denon publish all their IR codes:
//  https://www.google.co.uk/search?q=DENON+MASTER+IR+Hex+Command+Sheet
//  -> http://assets.denon.com/documentmaster/us/denon%20master%20ir%20hex.xls

// Having looked at the official Denon Pronto sheet and reverse engineered
// the timing values from it, it is obvious that Denon have a range of
// different timings and protocols ...the values here work for my AVR-3801 Amp!

//==============================================================================
//                    DDDD   EEEEE  N   N   OOO   N   N
//                     D  D  E      NN  N  O   O  NN  N
//                     D  D  EEE    N N N  O   O  N N N
//                     D  D  E      N  NN  O   O  N  NN
//                    DDDD   EEEEE  N   N   OOO   N   N
//==============================================================================


//+=============================================================================
//
static void  sendDenon (unsigned long data,  int nbits)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Header
	mark (HDR_MARK);
	space(HDR_SPACE);

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark (BIT_MARK);
			space(ONE_SPACE);
		} else {
			mark (BIT_MARK);
			space(ZERO_SPACE);
		}
	}

	// Footer
	mark(BIT_MARK);
    space(0);  // Always end with the LED off
}
#endif

















//==============================================================================
//       PPPP    AAA   N   N   AAA    SSSS   OOO   N   N  IIIII   CCCC
//       P   P  A   A  NN  N  A   A  S      O   O  NN  N    I    C
//       PPPP   AAAAA  N N N  AAAAA   SSS   O   O  N N N    I    C
//       P      A   A  N  NN  A   A      S  O   O  N  NN    I    C
//       P      A   A  N   N  A   A  SSSS    OOO   N   N  IIIII   CCCC
//==============================================================================

#define PANASONIC_BITS          48
#define PANASONIC_HDR_MARK    3502
#define PANASONIC_HDR_SPACE   1750
#define PANASONIC_BIT_MARK     502
#define PANASONIC_ONE_SPACE   1244
#define PANASONIC_ZERO_SPACE   400

//+=============================================================================
#ifdef SUPPORT_IR_PANASONIC
static void  sendPanasonic(unsigned int address,  unsigned long data)
{
	// Set IR carrier frequency
	enableIROut(35);

	// Header
	mark(PANASONIC_HDR_MARK);
	space(PANASONIC_HDR_SPACE);

	// Address
	for (unsigned long  mask = 1UL << (16 - 1);  mask;  mask >>= 1) {
		mark(PANASONIC_BIT_MARK);
		if (address & mask)  space(PANASONIC_ONE_SPACE) ;
		else                 space(PANASONIC_ZERO_SPACE) ;
    }

	// Data
	for (unsigned long  mask = 1UL << (32 - 1);  mask;  mask >>= 1) {
        mark(PANASONIC_BIT_MARK);
        if (data & mask)  space(PANASONIC_ONE_SPACE) ;
        else              space(PANASONIC_ZERO_SPACE) ;
    }

	// Footer
    mark(PANASONIC_BIT_MARK);
    space(0);  // Always end with the LED off
}
#endif

//==============================================================================
//                             JJJJJ  V   V   CCCC
//                               J    V   V  C
//                               J     V V   C
//                             J J     V V   C
//                              J       V     CCCC
//==============================================================================

#define JVC_BITS           16
#define JVC_HDR_MARK     8000
#define JVC_HDR_SPACE    4000
#define JVC_BIT_MARK      600
#define JVC_ONE_SPACE    1600
#define JVC_ZERO_SPACE    550
#define JVC_RPT_LENGTH  60000

//+=============================================================================
// JVC does NOT repeat by sending a separate code (like NEC does).
// The JVC protocol repeats by skipping the header.
// To send a JVC repeat signal, send the original code value
//   and set 'repeat' to true
//
#ifdef SUPPORT_IR_JVC
static void  sendJVC (unsigned long data,  int nbits,  bool repeat)
{
	// Set IR carrier frequency
	enableIROut(38);

	// Only send the Header if this is NOT a repeat command
	if (!repeat){
		mark(JVC_HDR_MARK);
		space(JVC_HDR_SPACE);
	}

	// Data
	for (unsigned long  mask = 1UL << (nbits - 1);  mask;  mask >>= 1) {
		if (data & mask) {
			mark(JVC_BIT_MARK);
			space(JVC_ONE_SPACE);
		} else {
			mark(JVC_BIT_MARK);
			space(JVC_ZERO_SPACE);
		}
	}

	// Footer
    mark(JVC_BIT_MARK);
    space(0);  // Always end with the LED off
}
#endif


uint8_t sendCommandByIR(const uint8_t _irPacketType, const uint8_t _nbits, const uint32_t _data, const uint8_t _repeat, const uint32_t _address)
{
  __SUPPRESS_WARNING_UNUSED(_nbits);
  __SUPPRESS_WARNING_UNUSED(_data);
  __SUPPRESS_WARNING_UNUSED(_repeat);
  __SUPPRESS_WARNING_UNUSED(_address);

//void  sendJVC (unsigned long data,  int nbits,  bool repeat)
//void  sendAiwaRCT501 (int code)
//void  sendDenon (unsigned long data,  int nbits)  so long codes, no example


//DEBUG_PORT.print("_irPacketType: "); DEBUG_PORT.println(_irPacketType);
//DEBUG_PORT.print("_nbits: "); DEBUG_PORT.println(_nbits);
//DEBUG_PORT.print("_data: "); DEBUG_PORT.println(_data);
#ifndef FEATURE_IR_ENABLE
  return false;
#endif

 switch (_irPacketType) {
#ifdef SUPPORT_IR_RC5
    case IR_RC5:
      sendRC5(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_RC6
    case IR_RC6:
      sendRC6(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_NEC
    case IR_NEC:
      sendNEC(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_SONY
    case IR_SONY:
      sendSony(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_SAMSUNG
    case IR_SAMSUNG:
      sendSAMSUNG(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_WHYNTER
    case IR_WHYNTER:
      sendWhynter(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_LG
    case IR_LG:
      sendLG(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_DISH
    case IR_DISH:
      sendDISH(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_SHARP
    case IR_SHARP:
      sendSharp(_data, _nbits);
      break;
#endif
#ifdef SUPPORT_IR_DENON
    case IR_DENON:
      sendDenon(_data, _nbits);
      break;
#endif
    default:  
      return RESULT_IS_OK;
  }
  return RESULT_IS_FAIL;
}
#endif