/**********************************************************/
/* Optiboot bootloader for Arduino                        */
/*                                                        */
/* http://optiboot.googlecode.com                         */
/*                                                        */
/* Arduino-maintained version : See README.TXT            */
/* http://code.google.com/p/arduino/                      */
/*  It is the intent that changes not relevant to the     */
/*  Arduino production envionment get moved from the      */
/*  optiboot project to the arduino project in "lumps."   */
/*                                                        */
/* Heavily optimised bootloader that is faster and        */
/* smaller than the Arduino standard bootloader           */
/*                                                        */
/* Enhancements:                                          */
/*   Fits in 512 bytes, saving 1.5K of code space         */
/*   Background page erasing speeds up programming        */
/*   Higher baud rate speeds up programming               */
/*   Written almost entirely in C                         */
/*   Customisable timeout with accurate timeconstant      */
/*   Optional virtual UART. No hardware UART required.    */
/*   Optional virtual boot partition for devices without. */
/*                                                        */
/* What you lose:                                         */
/*   Implements a skeleton STK500 protocol which is       */
/*     missing several features including EEPROM          */
/*     programming and non-page-aligned writes            */
/*   High baud rate breaks compatibility with standard    */
/*     Arduino flash settings                             */
/*                                                        */
/* Fully supported:                                       */
/*   ATmega168 based devices  (Diecimila etc)             */
/*   ATmega328P based devices (Duemilanove etc)           */
/*                                                        */
/* Beta test (believed working.)                          */
/*   ATmega8 based devices (Arduino legacy)               */
/*   ATmega328 non-picopower devices                      */
/*   ATmega644P based devices (Sanguino)                  */
/*   ATmega1284P based devices                            */
/*                                                        */
/* Alpha test                                             */
/*   ATmega1280 based devices (Arduino Mega)              */
/*                                                        */
/* Work in progress:                                      */
/*   ATtiny84 based devices (Luminet)                     */
/*                                                        */
/* Does not support:                                      */
/*   USB based devices (eg. Teensy)                       */
/*                                                        */
/* Assumptions:                                           */
/*   The code makes several assumptions that reduce the   */
/*   code size. They are all true after a hardware reset, */
/*   but may not be true if the bootloader is called by   */
/*   other means or on other hardware.                    */
/*     No interrupts can occur                            */
/*     UART and Timer 1 are set to their reset state      */
/*     SP points to RAMEND                                */
/*                                                        */
/* Code builds on code, libraries and optimisations from: */
/*   stk500boot.c          by Jason P. Kyle               */
/*   Arduino bootloader    http://arduino.cc              */
/*   Spiff's 1K bootloader http://spiffie.org/know/arduino_1k_bootloader/bootloader.shtml */
/*   avr-libc project      http://nongnu.org/avr-libc     */
/*   Adaboot               http://www.ladyada.net/library/arduino/bootloader.html */
/*   AVR305                Atmel Application Note         */
/*                                                        */
/* This program is free software; you can redistribute it */
/* and/or modify it under the terms of the GNU General    */
/* Public License as published by the Free Software       */
/* Foundation; either version 2 of the License, or        */
/* (at your option) any later version.                    */
/*                                                        */
/* This program is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the  */
/* implied warranty of MERCHANTABILITY or FITNESS FOR A   */
/* PARTICULAR PURPOSE.  See the GNU General Public        */
/* License for more details.                              */
/*                                                        */
/* You should have received a copy of the GNU General     */
/* Public License along with this program; if not, write  */
/* to the Free Software Foundation, Inc.,                 */
/* 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */
/*                                                        */
/* Licence can be viewed at                               */
/* http://www.fsf.org/licenses/gpl.txt                    */
/*                                                        */
/**********************************************************/


/**********************************************************/
/*                                                        */
/* Optional defines:                                      */
/*                                                        */
/**********************************************************/
/*                                                        */
/* BIG_BOOT:                                              */
/* Build a 1k bootloader, not 512 bytes. This turns on    */
/* extra functionality.                                   */
/*                                                        */
/* BAUD_RATE:                                             */
/* Set bootloader baud rate.                              */
/*                                                        */
/* LUDICROUS_SPEED:                                       */
/* 230400 baud :-)                                        */
/*                                                        */
/* SOFT_UART:                                             */
/* Use AVR305 soft-UART instead of hardware UART.         */
/*                                                        */
/* LED_START_FLASHES:                                     */
/* Number of LED flashes on bootup.                       */
/*                                                        */
/* LED_DATA_FLASH:                                        */
/* Flash LED when transferring data. For boards without   */
/* TX or RX LEDs, or for people who like blinky lights.   */
/*                                                        */
/* SUPPORT_EEPROM:                                        */
/* Support reading and writing from EEPROM. This is not   */
/* used by Arduino, so off by default.                    */
/*                                                        */
/* TIMEOUT_MS:                                            */
/* Bootloader timeout period, in milliseconds.            */
/* 500,1000,2000,4000,8000 supported.                     */
/*                                                        */
/* UART:                                                  */
/* UART number (0..n) for devices with more than          */
/* one hardware uart (644P, 1284P, etc)                   */
/*                                                        */
/**********************************************************/

/**********************************************************/
/* Version Numbers!                                       */
/*                                                        */
/* Arduino Optiboot now includes this Version number in   */
/* the source and object code.                            */
/*                                                        */
/* Version 3 was released as zip from the optiboot        */
/*  repository and was distributed with Arduino 0022.     */
/* Version 4 starts with the arduino repository commit    */
/*  that brought the arduino repository up-to-date with   */
/* the optiboot source tree changes since v3.             */
/*                                                        */
/**********************************************************/

/**********************************************************/
/* Edit History:					  */
/*							  */
/* Nov 2012																							  */
/* Specific version for 9x voice module									  */
/* by Mike Blandford																		  */
/* Mar 2012                                               */
/* 4.5 WestfW: add infrastructure for non-zero UARTS.     */
/* 4.5 WestfW: fix SIGNATURE_2 for m644 (bad in avr-libc) */
/* Jan 2012:                                              */
/* 4.5 WestfW: fix NRWW value for m1284.                  */
/* 4.4 WestfW: use attribute OS_main instead of naked for */
/*             main().  This allows optimizations that we */
/*             count on, which are prohibited in naked    */
/*             functions due to PR42240.  (keeps us less  */
/*             than 512 bytes when compiler is gcc4.5     */
/*             (code from 4.3.2 remains the same.)        */
/* 4.4 WestfW and Maniacbug:  Add m1284 support.  This    */
/*             does not change the 328 binary, so the     */
/*             version number didn't change either. (?)   */
/* June 2011:                                             */
/* 4.4 WestfW: remove automatic soft_uart detect (didn't  */
/*             know what it was doing or why.)  Added a   */
/*             check of the calculated BRG value instead. */
/*             Version stays 4.4; existing binaries are   */
/*             not changed.                               */
/* 4.4 WestfW: add initialization of address to keep      */
/*             the compiler happy.  Change SC'ed targets. */
/*             Return the SW version via READ PARAM       */
/* 4.3 WestfW: catch framing errors in getch(), so that   */
/*             AVRISP works without HW kludges.           */
/*  http://code.google.com/p/arduino/issues/detail?id=368n*/
/* 4.2 WestfW: reduce code size, fix timeouts, change     */
/*             verifySpace to use WDT instead of appstart */
/* 4.1 WestfW: put version number in binary.		  */
/**********************************************************/


#define OPTIBOOT_MAJVER 4
#define OPTIBOOT_MINVER 5

#define MAKESTR(a) #a
#define MAKEVER(a, b) MAKESTR(a*256+b)

//asm("  .section .version\n"
//    "optiboot_version:  .word " MAKEVER(OPTIBOOT_MAJVER, OPTIBOOT_MINVER) "\n"
//    "  .section .text\n");

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

// <avr/boot.h> uses sts instructions, but this version uses out instructions
// This saves cycles and program memory.
#include "boot.h"


// We don't use <avr/wdt.h> as those routines have interrupt overhead we don't need.

#include "pin_defs.h"
#include "stk500.h"


asm(" .section .bootjump,\"ax\"\n jmp bootmain\n") ;

//#ifndef LED_START_FLASHES
#define LED_START_FLASHES 0
//#endif

#ifdef LUDICROUS_SPEED
#define BAUD_RATE 230400L
#endif

/* set the UART baud rate defaults */
#ifndef BAUD_RATE
#if F_CPU >= 8000000L
#define BAUD_RATE   38400L // Highest rate Avrdude win32 will support
#elif F_CPU >= 1000000L
#define BAUD_RATE   9600L   // 19200 also supported, but with significant error
#elif F_CPU >= 128000L
#define BAUD_RATE   4800L   // Good for 128kHz internal RC
#else
#define BAUD_RATE 1200L     // Good even at 32768Hz
#endif
#endif

#ifndef UART
#define UART 0
#endif

#if 0
/* Switch in soft UART for hard baud rates */
/*
 * I don't understand what this was supposed to accomplish, where the
 * constant "280" came from, or why automatically (and perhaps unexpectedly)
 * switching to a soft uart is a good thing, so I'm undoing this in favor
 * of a range check using the same calc used to config the BRG...
 */
#if (F_CPU/BAUD_RATE) > 280 // > 57600 for 16MHz
#ifndef SOFT_UART
#define SOFT_UART
#endif
#endif
#else // 0
#if (F_CPU + BAUD_RATE * 4L) / (BAUD_RATE * 8L) - 1 > 250
#error Unachievable baud rate (too slow) BAUD_RATE 
#endif // baud rate slow check
#if (F_CPU + BAUD_RATE * 4L) / (BAUD_RATE * 8L) - 1 < 3
#error Unachievable baud rate (too fast) BAUD_RATE 
#endif // baud rate fastn check
#endif

/* Watchdog settings */
#define WATCHDOG_OFF    (0)
#define WATCHDOG_16MS   (_BV(WDE))
#define WATCHDOG_32MS   (_BV(WDP0) | _BV(WDE))
#define WATCHDOG_64MS   (_BV(WDP1) | _BV(WDE))
#define WATCHDOG_125MS  (_BV(WDP1) | _BV(WDP0) | _BV(WDE))
#define WATCHDOG_250MS  (_BV(WDP2) | _BV(WDE))
#define WATCHDOG_500MS  (_BV(WDP2) | _BV(WDP0) | _BV(WDE))
#define WATCHDOG_1S     (_BV(WDP2) | _BV(WDP1) | _BV(WDE))
#define WATCHDOG_2S     (_BV(WDP2) | _BV(WDP1) | _BV(WDP0) | _BV(WDE))
#ifndef __AVR_ATmega8__
#define WATCHDOG_4S     (_BV(WDP3) | _BV(WDE))
#define WATCHDOG_8S     (_BV(WDP3) | _BV(WDP0) | _BV(WDE))
#endif

/* Function Prototypes */
/* The main function is in init9, which removes the interrupt vector table */
/* we don't need. It is also 'naked', which means the compiler does not    */
/* generate any entry or exit code itself. */
extern "C" void bootmain(void) __attribute__ ((OS_main)) __attribute__ ((noreturn)) __attribute__ ((section (".boottext")));
void bootmain(void) ;
static void putch(char);
static uint8_t getch(void);
static inline void bgetNch(uint8_t); /* "static inline" is a compiler hint to reduce code size */
void verifySpace(void);
#if LED_START_FLASHES > 0
static inline void flash_led(uint8_t);
#endif
uint8_t getLen(void);
//static inline void watchdogReset(void);
void watchdogConfig(uint8_t x);
#ifdef SOFT_UART
void uartDelay(void) __attribute__ ((naked));
#endif
void appStart(void) ; // __attribute__ ((naked));

/*
 * NRWW memory
 * Addresses below NRWW (Non-Read-While-Write) can be programmed while
 * continuing to run code from flash, slightly speeding up programming
 * time.  Beware that Atmel data sheets specify this as a WORD address,
 * while optiboot will be comparing against a 16-bit byte address.  This
 * means that on a part with 128kB of memory, the upper part of the lower
 * 64k will get NRWW processing as well, even though it doesn't need it.
 * That's OK.  In fact, you can disable the overlapping processing for
 * a part entirely by setting NRWWSTART to zero.  This reduces code
 * space a bit, at the expense of being slightly slower, overall.
 *
 * RAMSTART should be self-explanatory.  It's bigger on parts with a
 * lot of peripheral registers.
 */
#if defined(__AVR_ATmega168__)
#define RAMSTART (0x100)
#define NRWWSTART (0x3800)
#elif defined(__AVR_ATmega328P__)
#define RAMSTART (0x100)
#define NRWWSTART (0x7000)
#elif defined(__AVR_ATmega328__)
#define RAMSTART (0x100)
#define NRWWSTART (0x7000)
#elif defined (__AVR_ATmega64__)
#define BOOTSTART (0x7D00)
#define RAMSTART (0x100)
#define NRWWSTART (0xE000)
#elif defined (__AVR_ATmega128__)
#define BOOTSTART (0xFD00)
#define RAMSTART (0x100)
#define NRWWSTART (0xE000)
// correct for a bug in avr-libc
#undef SIGNATURE_2
#define SIGNATURE_2 0x02
#elif defined (__AVR_ATmega2561__)
#define BOOTSTART (0x1FD00)
#define RAMSTART (0x200)
#define NRWWSTART (0xE000)
#elif defined (__AVR_ATmega1284P__)
#define RAMSTART (0x100)
#define NRWWSTART (0xE000)
#elif defined(__AVR_ATtiny84__)
#define RAMSTART (0x100)
#define NRWWSTART (0x0000)
#elif defined(__AVR_ATmega1280__)
#define RAMSTART (0x200)
#define NRWWSTART (0xE000)
#elif defined(__AVR_ATmega8__) || defined(__AVR_ATmega88__)
#define RAMSTART (0x100)
#define NRWWSTART (0x1800)
#endif

/* C zero initialises all global variables. However, that requires */
/* These definitions are NOT zero initialised, but that doesn't matter */
/* This allows us to drop the zero init code, saving us memory */
#define buff    ((uint8_t*)(RAMSTART))
#ifdef VIRTUAL_BOOT_PARTITION
#define rstVect (*(uint16_t*)(RAMSTART+SPM_PAGESIZE*2+4))
#define wdtVect (*(uint16_t*)(RAMSTART+SPM_PAGESIZE*2+6))
#endif

/*
 * Handle devices with up to 4 uarts (eg m1280.)  Rather inelegantly.
 * Note that mega8 still needs special handling, because ubrr is handled
 * differently.
 */
#if UART == 0
# define UART_SRA UCSR0A
# define UART_SRB UCSR0B
# define UART_SRC UCSR0C
# define UART_SRL UBRR0L
# define UART_UDR UDR0
#elif UART == 1
# define UART_SRA UCSR1A
# define UART_SRB UCSR1B
# define UART_SRC UCSR1C
# define UART_SRL UBRR1L
# define UART_UDR UDR1
#elif UART == 2
# define UART_SRA UCSR2A
# define UART_SRB UCSR2B
# define UART_SRC UCSR2C
# define UART_SRL UBRR2L
# define UART_UDR UDR2
#elif UART == 3
# define UART_SRA UCSR3A
# define UART_SRB UCSR3B
# define UART_SRC UCSR3C
# define UART_SRL UBRR3L
# define UART_UDR UDR3
#endif

//#if defined (__AVR_ATmega128__) || defined (__AVR_ATmega2561__)
#if defined (__AVR_ATmega128__)
#define WDTCSR WDTCR
#endif

//struct flags_struct
//{
//  uint8_t eeprom ;
//} ;

static inline void __attribute__ ((always_inline))
eeprom_write_byte_cmp (uint8_t dat, uint16_t pointer_eeprom)
{
  //see /home/thus/work/avr/avrsdk4/avr-libc-1.4.4/libc/misc/eeprom.S:98 143
#ifdef CPUM2561
  while(EECR & (1<<EEPE)) /* make sure EEPROM is ready */
#else
  while(EECR & (1<<EEWE)) /* make sure EEPROM is ready */
#endif
  {
  } ;
  EEAR  = pointer_eeprom;

  EECR |= 1<<EERE;
  if(dat == EEDR) return;

  EEDR  = dat;
//  uint8_t flags=SREG;
//  cli();
#ifdef CPUM2561
  EECR |= 1<<EEMPE;
  EECR |= 1<<EEPE;
#else
  EECR |= 1<<EEMWE;
  EECR |= 1<<EEWE;
#endif
//  SREG = flags;
}

// Watchdog functions. These are only safe with interrupts turned off.
//__attribute__ ((section(".boottext"), used))  __attribute__ ((always_inline))
//static inline void watchdogReset() {
//  __asm__ __volatile__ (
//    "wdr\n"
//  );
//}

/* main program starts here */
//__attribute__ ((section(".boottext"), used))
void bootmain(void) {
  uint8_t ch;
	uint8_t eeprom ;
#if defined (__AVR_ATmega128__) || defined (__AVR_ATmega2561__)
	uint8_t GPIOR0 ;
#endif

#if defined (__AVR_ATmega64__)
	uint8_t GPIOR0 ;
#endif
  /*
   * Making these local and in registers prevents the need for initializing
   * them, and also saves space because code no longer stores to memory.
   * (initializing address keeps the compiler happy, but isn't really
   *  necessary, and uses 4 bytes of flash.)
   */
  register uint16_t address = 0 ;

  // After the zero init loop, this is the first code to run.
  //
  // This code makes the following assumptions:
  //  No interrupts will execute
  //  SP points to RAMEND
  //  r1 contains zero
  //
  // If not, uncomment the following instructions:
  // cli();
//  asm volatile ("clr __zero_reg__");
#ifdef __AVR_ATmega8__
  SP=RAMEND;  // This is done by hardware reset
#endif

  // Adaboot no-wait mod
#if defined (__AVR_ATmega128__)
  ch = MCUCSR;
  MCUCSR = 0;
#if defined (__AVR_ATmega128__)
#define TIFR1 TIFR
#endif
#else
  ch = MCUSR;
  MCUSR = 0;
#endif

	// Here, if power on, wait 0.5 secs, then check for
	// serial Rx signal low, if so, stay in bootloader
	// else go to application
	DDRE&= ~0x01 ;		// RX pin as input
	PORTE|= 0x01 ;		// With a pullup

//  if ((ch & _BV(PORF)))
//	{
//		TCNT1 = 65535-5859 ;		
//  	TCCR1B = _BV(CS12) | _BV(CS10); // div 1024
//    TIFR1 = _BV(TOV1);
//    while(!(TIFR1 & _BV(TOV1)))
//			;
//  	TCCR1B = 0 ; // Stop timer
//		if ( PIND & 1 )
//		{
//			appStart() ;	// Power on, go to voice application
//										// if loaded
//		}
//	}

#if LED_START_FLASHES > 0
  // Set up Timer 1 for timeout counter
  TCCR1B = _BV(CS12) | _BV(CS10); // div 1024
#endif

#ifndef SOFT_UART
  UART_SRA = _BV(U2X0); //Double speed mode USART0
  UART_SRB = _BV(RXEN0) | _BV(TXEN0);
  UART_SRC = _BV(UCSZ00) | _BV(UCSZ01);
//  UART_SRL = (uint8_t)( (F_CPU + BAUD_RATE * 4L) / (BAUD_RATE * 8L) - 1 );
  UART_SRL = 16 ;	// 115200 baud
#endif

  // Set up watchdog to trigger after 500ms


//  watchdogConfig(WATCHDOG_1S);

  /* Set LED pin as output */
//  LED_DDR |= _BV(LED);

#ifdef SOFT_UART
  /* Set TX pin as output */
  UART_DDR |= _BV(UART_TX_BIT);
#endif

#if LED_START_FLASHES > 0
  /* Flash onboard LED to signal entering of bootloader */
  flash_led(LED_START_FLASHES * 2);
#endif

  /* Forever loop */
  for (;;)
	{
    /* get character from UART */
    ch = getch();

    if(ch == STK_GET_PARAMETER)
		{
      GPIOR0 = getch();
      verifySpace();
      if (GPIOR0 == 0x82)
			{
	/*
	 * Send optiboot version as "minor SW version"
	 */
				putch(OPTIBOOT_MINVER);
      }
			else if (GPIOR0 == 0x81)
			{
	  		putch(OPTIBOOT_MAJVER);
      }
			else
			{
	/*
	 * GET PARAMETER returns a generic 0x03 reply for
         * other parameters - enough to keep Avrdude happy
	 */
				putch(0x03);
      }
    }
    else if(ch == STK_SET_DEVICE) {
      // SET DEVICE is ignored
      bgetNch(20);
    }
    else if(ch == STK_SET_DEVICE_EXT)
		{
      // SET DEVICE EXT is ignored
      bgetNch(5);
    }
    else if(ch == STK_LOAD_ADDRESS)
		{
      // LOAD ADDRESS
      uint16_t newAddress ;
      newAddress = getch() ;
      newAddress = (newAddress & 0xff) | (getch() << 8);
#if defined (__AVR_ATmega128__) || defined (__AVR_ATmega2561__)
#ifdef RAMPZ
      // Transfer top bit to RAMPZ
      RAMPZ = (newAddress & 0x8000) ? 1 : 0;
#endif
#endif
      newAddress += newAddress; // Convert from word address to byte address
      address = newAddress;
      verifySpace();
    }
    else if(ch == STK_UNIVERSAL)
		{
      // UNIVERSAL command is ignored
      bgetNch(4);
      putch(0x00);
    }
    /* Write memory, length is big endian and is in bytes */
    else if(ch == STK_PROG_PAGE)
		{
      // PROGRAM PAGE - we support flash programming only, not EEPROM
      uint8_t *bufPtr;
      uint16_t addrPtr;
  		register uint8_t length ;
  		register uint8_t count ;

      getch();			/* getlen() */
      length = getch();
	    eeprom = 0 ;
	    if (getch() == 'E') eeprom = 1 ; //      getch();

      // If we are in RWW section, immediately start page erase
	    if ( eeprom == 0 )
			{
#ifdef __AVR_ATmega2561__
      	__boot_page_erase_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#else
#if defined (__AVR_ATmega128__) || defined (__AVR_ATmega2561__)
      	if ( (RAMPZ == 0 ) || (address < NRWWSTART)) __boot_page_erase_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#else
      	if ( address < NRWWSTART) __boot_page_erase_normal((uint16_t)(void*)address);
#endif
#endif
			}

      // While that is going on, read in page contents
			count = length ;
      bufPtr = buff;
      do *bufPtr++ = getch();
      while (--count) ;

      // If we are in NRWW section, page erase has to be delayed until now.
      // Todo: Take RAMPZ into account
#ifndef __AVR_ATmega2561__
#if defined (__AVR_ATmega128__)
      if ( ( RAMPZ==0) || (address < BOOTSTART) )
			{
      	if ( RAMPZ && (address >= NRWWSTART) ) __boot_page_erase_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#else
      if ( address < BOOTSTART)
			{
      	if ( address >= NRWWSTART) __boot_page_erase_normal((uint16_t)(void*)address);
#endif
#endif

#ifdef __AVR_ATmega2561__
      if ( ( RAMPZ==0) || (address < BOOTSTART) )
			{
      	if ( RAMPZ && (address >= NRWWSTART) ) __boot_page_erase_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#endif

      	// Read command terminator, start reply
      	verifySpace();

				if (eeprom)
				{		                //Write to EEPROM one byte at a time
#ifndef __AVR_ATmega2561__
					address >>= 1 ; // Back to byte address
#endif
		  	  for(count=0;count<length ; count += 1 )
					{
						eeprom_write_byte_cmp( buff[count], (uint16_t)address ) ;
						address += 1 ;
		  	  }			
				}
				else
				{					        //Write to FLASH one page at a time
      		// If only a partial page is to be programmed, the erase might not be complete.
      		// So check that here
      		boot_spm_busy_wait();

		#ifdef VIRTUAL_BOOT_PARTITION
      		if ((uint16_t)(void*)address == 0) {
      		  // This is the reset vector page. We need to live-patch the code so the
      		  // bootloader runs.
      		  //
      		  // Move RESET vector to WDT vector
      		  uint16_t vect = buff[0] | (buff[1]<<8);
      		  rstVect = vect;
      		  wdtVect = buff[8] | (buff[9]<<8);
      		  vect -= 4; // Instruction is a relative jump (rjmp), so recalculate.
      		  buff[8] = vect & 0xff;
      		  buff[9] = vect >> 8;

      		  // Add jump to bootloader at RESET vector
      		  buff[0] = 0x7f;
      		  buff[1] = 0xce; // rjmp 0x1d00 instruction
      		}
		#endif

      		// Copy buffer into programming buffer
      		bufPtr = buff;
      		addrPtr = (uint16_t)(void*)address;
      		ch = SPM_PAGESIZE / 2;
      		do {
      		  uint16_t a;
		//        a = *bufPtr++;
		//        a |= (*bufPtr++) << 8;

						a = *((uint16_t *)bufPtr) ;
						bufPtr += 2 ;

#ifdef __AVR_ATmega2561__
      		  __boot_page_fill_extended((uint32_t)(uint16_t)(void*)addrPtr|(uint32_t)RAMPZ<<16,a);
#else
#if defined (__AVR_ATmega128__)
      		  __boot_page_fill_extended((uint32_t)(uint16_t)(void*)addrPtr|(uint32_t)RAMPZ<<16,a);
#else
      		  __boot_page_fill_normal((uint16_t)(void*)addrPtr,a);
#endif
#endif
      		  addrPtr += 2;
      		} while (--ch);

      		// Write from programming buffer
#ifdef __AVR_ATmega2561__
      		__boot_page_write_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#else
#if defined (__AVR_ATmega128__)
      		__boot_page_write_extended((uint32_t)(uint16_t)(void*)address|(uint32_t)RAMPZ<<16);
#else
      		__boot_page_write_normal((uint16_t)(void*)address);
#endif
#endif
      		boot_spm_busy_wait();

		#if defined(RWWSRE)
      		// Reenable read access to flash
      		boot_rww_enable();
		#endif
				}
			}
			else
			{
      	verifySpace();
			}
    }
    /* Read memory block mode, length is big endian.  */
    else if(ch == STK_READ_PAGE)
		{
		  register uint8_t  length;
      // READ PAGE - we only read flash
      getch();			/* getlen() */
      length = getch();
			eeprom = 0;
	    if (getch() == 'E') // getch();
			{
				eeprom = 1 ;
#ifndef __AVR_ATmega2561__
				address >>= 1 ;
#endif
			}
      verifySpace();
	    if (eeprom)
			{	                        // Byte access EEPROM read
				do
				{
  				EEAR = address ;
					address += 1 ;
				  EECR |= 1<<EERE ;
					putch( EEDR ) ;
	      } while (--length) ;
			}
			else
			{
	#ifdef VIRTUAL_BOOT_PARTITION
	      do {
	        // Undo vector patch in bottom page so verify passes
	        if (address == 0)       ch=rstVect & 0xff;
	        else if (address == 1)  ch=rstVect >> 8;
	        else if (address == 8)  ch=wdtVect & 0xff;
	        else if (address == 9) ch=wdtVect >> 8;
	        else ch = pgm_read_byte_near(address);
	        address++;
	        putch(ch);
	      } while (--length);
	#else
	#ifdef RAMPZ
	// Since RAMPZ should already be set, we need to use EPLM directly.
	//      do putch(pgm_read_byte_near(address++));
	//      while (--length);
	      do {
	        uint8_t result;
	        __asm__ ("elpm %0,Z\n":"=r"(result):"z"(address));
	        putch(result);
	        address++;
	      }
	      while (--length);
	#else
	      do putch(pgm_read_byte_near(address++));
	      while (--length);
	#endif
	#endif
			}
    }

    /* Get device signature bytes  */
    else if(ch == STK_READ_SIGN)
		{
      // READ SIGN - return what Avrdude wants to hear
      verifySpace();
      putch(SIGNATURE_0);
      putch(SIGNATURE_1);
      putch(SIGNATURE_2);
    }
    else if (ch == STK_LEAVE_PROGMODE) { /* 'Q' */
      // Adaboot no-wait mod

//      watchdogConfig(WATCHDOG_16MS);
      
			verifySpace();
    }
    else
		{
      // This covers the response to commands like STK_ENTER_PROGMODE
      verifySpace();
    }
    putch(STK_OK);
  }
}

__attribute__ ((section(".boottext"), used))
void putch(char ch) {
#ifndef SOFT_UART
  while (!(UART_SRA & _BV(UDRE0)));
  UART_UDR = ch;
#else
  __asm__ __volatile__ (
    "   com %[ch]\n" // ones complement, carry set
    "   sec\n"
    "1: brcc 2f\n"
    "   cbi %[uartPort],%[uartBit]\n"
    "   rjmp 3f\n"
    "2: sbi %[uartPort],%[uartBit]\n"
    "   nop\n"
    "3: rcall uartDelay\n"
    "   rcall uartDelay\n"
    "   lsr %[ch]\n"
    "   dec %[bitcnt]\n"
    "   brne 1b\n"
    :
    :
      [bitcnt] "d" (10),
      [ch] "r" (ch),
      [uartPort] "I" (_SFR_IO_ADDR(UART_PORT)),
      [uartBit] "I" (UART_TX_BIT)
    :
      "r25"
  );
#endif
}

__attribute__ ((section(".boottext"), used))
uint8_t getch(void) {
  uint8_t ch;

#ifdef LED_DATA_FLASH
#ifdef __AVR_ATmega8__
//  LED_PORT ^= _BV(LED);
#else
//  LED_PIN |= _BV(LED);
#endif
#endif

#ifdef SOFT_UART
  __asm__ __volatile__ (
    "1: sbic  %[uartPin],%[uartBit]\n"  // Wait for start edge
    "   rjmp  1b\n"
    "   rcall uartDelay\n"          // Get to middle of start bit
    "2: rcall uartDelay\n"              // Wait 1 bit period
    "   rcall uartDelay\n"              // Wait 1 bit period
    "   clc\n"
    "   sbic  %[uartPin],%[uartBit]\n"
    "   sec\n"
    "   dec   %[bitCnt]\n"
    "   breq  3f\n"
    "   ror   %[ch]\n"
    "   rjmp  2b\n"
    "3:\n"
    :
      [ch] "=r" (ch)
    :
      [bitCnt] "d" (9),
      [uartPin] "I" (_SFR_IO_ADDR(UART_PIN)),
      [uartBit] "I" (UART_RX_BIT)
    :
      "r25"
);
#else
  while(!(UART_SRA & _BV(RXC0)))
		
    wdt_reset();
//		watchdogReset()
    
		;
//  if (!(UART_SRA & _BV(FE0))) {
      /*
       * A Framing Error indicates (probably) that something is talking
       * to us at the wrong bit rate.  Assume that this is because it
       * expects to be talking to the application, and DON'T reset the
       * watchdog.  This should cause the bootloader to abort and run
       * the application "soon", if it keeps happening.  (Note that we
       * don't care that an invalid char is returned...)
       */
//    wdt_reset();
//    watchdogReset();
//  }
  
  ch = UART_UDR;
#endif

#ifdef LED_DATA_FLASH
#ifdef __AVR_ATmega8__
//  LED_PORT ^= _BV(LED);
#else
//  LED_PIN |= _BV(LED);
#endif
#endif

  return ch;
}

#ifdef SOFT_UART
// AVR305 equation: #define UART_B_VALUE (((F_CPU/BAUD_RATE)-23)/6)
// Adding 3 to numerator simulates nearest rounding for more accurate baud rates
#define UART_B_VALUE (((F_CPU/BAUD_RATE)-20)/6)
#if UART_B_VALUE > 255
#error Baud rate too slow for soft UART
#endif

void uartDelay() {
  __asm__ __volatile__ (
    "ldi r25,%[count]\n"
    "1:dec r25\n"
    "brne 1b\n"
    "ret\n"
    ::[count] "M" (UART_B_VALUE)
  );
}
#endif

__attribute__ ((section(".boottext"), used))
void bgetNch(uint8_t count) {
  do getch(); while (--count);
  verifySpace();
}

__attribute__ ((section(".boottext"), used))
void verifySpace()
{
  if ( getch() != CRC_EOP) {
    
//		watchdogConfig(WATCHDOG_16MS);    // shorten WD timeout
    
		while (1)			      // and busy-loop so that WD causes
      ;				      //  a reset and app start.
  }
  putch(STK_INSYNC);
}

#if LED_START_FLASHES > 0
void flash_led(uint8_t count) {
  do {
    TCNT1 = -(F_CPU/(1024*16));
    TIFR1 = _BV(TOV1);
    while(!(TIFR1 & _BV(TOV1)));
//#ifdef __AVR_ATmega8__
    LED_PORT ^= _BV(LED);
//#else
//    LED_PIN |= _BV(LED);
//#endif
    wdt_reset();
//    watchdogReset();
  } while (--count);
}
#endif


//__attribute__ ((section(".boottext"))) 

//__attribute__ ((section(".bootjump"))) __asm__(" jmp 0x1FC00\n") ;

//void watchdogConfig(uint8_t x) {
//  WDTCSR = _BV(WDCE) | _BV(WDE);
//  WDTCSR = x;
//}

//void appStart()
//{
//  watchdogConfig(WATCHDOG_OFF);
////  __asm__ __volatile__ (
////#ifdef VIRTUAL_BOOT_PARTITION
////    // Jump to WDT vector
////    "ldi r30,4\n"
////    "clr r31\n"
////#else
////    // Jump to RST vector
////    "clr r30\n"
////    "clr r31\n"
////#endif
////    "ijmp\n"
////  );

//	register void (*p)() ;
//	p = 0 ;

//	if ( *(uint8_t*)p != 0xFF )
//	{
//		(*p)() ;
//	}
//}

