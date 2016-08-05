/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2004  Anil Kumar, Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Basic.h"
#include "ExampleTests.h"

/*
int main(int argc, char* argv[])
{
  CU_BasicRunMode mode = CU_BRM_VERBOSE;
  CU_ErrorAction error_action = CUEA_IGNORE;
  int i;

  setvbuf(stdout, NULL, _IONBF, 0);

  for (i=1 ; i<argc ; i++) {
    if (!strcmp("-i", argv[i])) {
      error_action = CUEA_IGNORE;
    }
    else if (!strcmp("-f", argv[i])) {
      error_action = CUEA_FAIL;
    }
    else if (!strcmp("-A", argv[i])) {
      error_action = CUEA_ABORT;
    }
    else if (!strcmp("-s", argv[i])) {
      mode = CU_BRM_SILENT;
    }
    else if (!strcmp("-n", argv[i])) {
      mode = CU_BRM_NORMAL;
    }
    else if (!strcmp("-v", argv[i])) {
      mode = CU_BRM_VERBOSE;
    }
    else if (!strcmp("-e", argv[i])) {
      print_example_results();
      return 0;
    }
    else {
      printf("\nUsage:  BasicTest [options]\n\n"
               "Options:   -i   ignore framework errors [default].\n"
               "           -f   fail on framework error.\n"
               "           -A   abort on framework error.\n\n"
               "           -s   silent mode - no output to screen.\n"
               "           -n   normal mode - standard output to screen.\n"
               "           -v   verbose mode - max output to screen [default].\n\n"
               "           -e   print expected test results and exit.\n"
               "           -h   print this message and exit.\n\n");
      return 0;
    }
  }

  if (CU_initialize_registry()) {
    printf("\nInitialization of Test Registry failed.");
  }
  else {
    AddTests();
    CU_basic_set_mode(mode);
    CU_set_error_action(error_action);
    printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    CU_cleanup_registry();
  }

  return 0;
}
*/

#include <reg51.h>
#include <stdio.h>

#define    XTAL 11059200                        // CPU Oscillator Frequency
#define    baudrate 9600                        // 9600 bps communication baudrate

#define    OLEN  8                              // size of serial transmission buffer
unsigned   char  ostart;                        // transmission buffer start index
unsigned   char  oend;                          // transmission buffer end index
char idata outbuf[OLEN];                        // storage for transmission buffer

#define    ILEN  8                              // size of serial receiving buffer
unsigned   char  istart;                        // receiving buffer start index
unsigned   char  iend;                          // receiving buffer end index
char idata inbuf[ILEN];                         // storage for receiving buffer

bit sendfull;                                   // flag: marks transmit buffer full
bit sendactive;                                 // flag: marks transmitter active


/*--------------------------------------------------------------------------------
 * Serial Interrupt Service Routine
 *------------------------------------------------------------------------------*/
static void com_isr (void) interrupt 4 using 1 {
char c;

  //----- Received data interrupt. -----
  if (RI) {
    c = SBUF;                                   // read character
    RI = 0;                                     // clear interrupt request flag
    if (istart + ILEN != iend) {
      inbuf[iend++ & (ILEN-1)] = c;             // put character into buffer
    }
  }

  //----- Transmitted data interrupt. -----
  if (TI != 0) {
    TI = 0;                                     // clear interrupt request flag
    if (ostart != oend) {                       // if characters in buffer and
      SBUF = outbuf[ostart++ & (OLEN-1)];       // transmit character
      sendfull = 0;                             // clear 'sendfull' flag
    }
    else {                                      // if all characters transmitted
      sendactive = 0;                           // clear 'sendactive'
    }
  }
}


/*--------------------------------------------------------------------------------
 * Function to initialize the serial port and the UART baudrate.
 *------------------------------------------------------------------------------*/
void com_initialize (void) {
  istart = 0;                                  // empty transmit buffers
  iend = 0;
  ostart = 0;                                  // empty transmit buffers
  oend = 0;
  sendactive = 0;                              // transmitter is not active
  sendfull = 0;                                // clear 'sendfull' flag

                                               // Configure timer 1 as a baud rate generator
  PCON |= 0x80;                                // 0x80=SMOD: set serial baudrate doubler
  TMOD |= 0x20;                                // put timer 1 into MODE 2

  TH1 = (unsigned char) (256 - (XTAL / (16L * 12L * baudrate)));

  TR1 = 1;                                     // start timer 1

  SCON = 0x50;                                 // serial port MODE 1, enable serial receiver
  ES = 1;                                      // enable serial interrupts
}


/*--------------------------------------------------------------------------------
 * putbuf: write a character to SBUF or transmission buffer
 *------------------------------------------------------------------------------*/
void putbuf (char c) {
  if (!sendfull) {                             // transmit only if buffer not full
    if (!sendactive) {                         // if transmitter not active:
      sendactive = 1;                          // transfer first character direct
      SBUF = c;                                // to SBUF to start transmission
    }
    else {
      ES = 0;                                  // disable serial interrupts during buffer update
      outbuf[oend++ & (OLEN-1)] = c;           // put char to transmission buffer
      if (((oend ^ ostart) & (OLEN-1)) == 0) {
         sendfull = 1;
      }                                        // set flag if buffer is full
      ES = 1;                                  // enable serial interrupts again
    }
  }
}


/*--------------------------------------------------------------------------------
 * Replacement routine for the standard library putchar routine.
 * The printf function uses putchar to output a character.
 *------------------------------------------------------------------------------*/
char putchar (char c) {
  if (c == '\n') {                             // expand new line character:
    while (sendfull);                          // wait until there is space in buffer
    putbuf (0x0D);                             // send CR before LF for <new line>
  }
  while (sendfull);                            // wait until there is space in buffer
  putbuf (c);                                  // place character into buffer
  return (c);
}


/*--------------------------------------------------------------------------------
 * Replacement routine for the standard library _getkey routine.
 * The getchar and gets functions uses _getkey to read a character.
 *------------------------------------------------------------------------------*/
char _getkey (void) {
  char c;
  while (iend == istart) {
     ;                                         // wait until there are characters
  }
  ES = 0;                                      // disable serial interrupts during buffer update
  c = inbuf[istart++ & (ILEN-1)];
  ES = 1;                                      // enable serial interrupts again
  return (c);
}


/*--------------------------------------------------------------------------------
 * Main C function that start the interrupt-driven serial I/O.
 *------------------------------------------------------------------------------*/
void main (void) {
  CU_BasicRunMode mode = CU_BRM_VERBOSE;
  CU_ErrorAction error_action = CUEA_IGNORE;
	unsigned char xdata malloc_mempool [1024*10];
	init_mempool (&malloc_mempool, sizeof(malloc_mempool));
	
  EA = 1;                                      // enable global interrupts
  com_initialize ();                           // initialize interrupt driven serial I/O
		
  if (CU_initialize_registry()) {
    printf("\nInitialization of Test Registry failed.");
  }
  else {
    AddTests();
    CU_basic_set_mode(mode);
    CU_set_error_action(error_action);
    printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    CU_cleanup_registry();
  }
	
  while (1) {
//    char c;
//    c = getchar ();
//    printf ("\nYou typed the character %c.\n", c);
  }
}
