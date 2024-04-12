/*
 ********************************************************************************************************
 *
 * (c) COPYRIGHT, 2023 USA Firmware Corporation
 *
 * All rights reserved. This file is the intellectual property of USA Firmware Corporation and it may
 * not be disclosed to others or used for any purposes without the written consent of USA Firmware
 * Corporation.
 *
 ********************************************************************************************************
 */

/**
 ********************************************************************************************************
 *
 * @brief      Power monitor read test utility program for the Henny Penny UHC.
 * @file       pwrmon.cpp
 * @author     Roger Chaplin
 *
 ********************************************************************************************************
 */

/*
 ********************************************************************************************************
 *                                           INCLUDE FILES
 ********************************************************************************************************
 */
/********************************************** System *************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <array>
#include <map>
#include <string>

/********************************************    User   ************************************************/
#include "atm90e26.h"


using namespace std;

/*
 ********************************************************************************************************
 *                                               DEFINES
 ********************************************************************************************************
 */
/****************************************** Symbolic Constants ******************************************/
#define PWRMON_SPI   "/dev/spidev1.0"
#define SPI_SPEED    15625U

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static void print_usage(char const * const program_name);


/*
 ********************************************************************************************************
 *                                         PUBLIC FUNCTIONS DEFINITIONS
 ********************************************************************************************************
 */

/**
 * @brief The program's main entry point.
 *
 * @param[in] argc The number of command line arguments.
 *
 * @param[in] argv The command line arguments array.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int main(int argc, char **argv)
{
   int c;
   int reg;
   bool print_hex = false;
   int retval = 0;

   while (((c = getopt(argc, argv, "hx")) != EOF) && (0 == retval))
   {
      switch (c)
      {
         case 'x':
            print_hex = true;
            break;

         case 'h':
         case '?':
            print_usage(argv[0]);
            retval = 1;
            break;
      }
   }

   if (0 == retval)
   {
      if ((optind + 1) != argc)
      {
         cerr << "Invalid number of arguments" << endl;
         retval = 1;
      }
      else
      {
         reg = toupper(*argv[optind]);
         if (('I' != reg) && ('V' != reg))
         {
            cerr << "Invalid reg: " << argv[optind] << endl;
            retval = -1;
         }
      }
   }

   if (0 != retval)
   {
      print_usage(argv[0]);
   }
   else
   {
      retval = atm90e26_init(PWRMON_SPI, SPI_SPEED);
      if (0 != retval)
      {
         cerr << "Error initializing ATM90E26" << endl;
      }

      if (0 == retval)
      {
         retval = atm90e26_start(lgain_24, 20.0F);
         if (0 != retval)
         {
            cerr << "Error starting ATM90E26 metering and measurement" << endl;
         }
      }

      uint16_t ui_regval;
      if (0 == retval)
      {
         if ('I' == reg)
         {
            retval =  atm90e26_irms_raw_get(&ui_regval);
         }
         else
         {
            // reg must be V
            retval = atm90e26_vrms_raw_get(&ui_regval);
         }

         if (0 != retval)
         {
            cerr << "Error reading ATM90E26" << endl;
         }
         else
         {
            if (!print_hex)
            {
               cout << ui_regval << endl;
            }
            else
            {
               cout << "0x" << hex << ui_regval << dec << endl;
            }
         }
      }

   }

   return retval;
}


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DEFINITIONS
 ********************************************************************************************************
 */

/**
 * @brief Print the usage instructions.
 *
 * @param[in] program_name The name by which the program was invoked.
 */
static void print_usage(char const * const program_name)
{
   // Make sure the list of signal names is in the same order as the map initializer above.
   cout << "usage: " << program_name << " [-x] <reg>" << endl;
   cout << "    where <reg> is teither I (reads the Irms register) or V (reads the Urms register)" << endl;
   cout << "    -x causes the value to be printed in hexadecimal (the default is decimal)" << endl;
}


/******************************************      End of file        ************************************/
