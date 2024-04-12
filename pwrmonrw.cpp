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
 * @brief      Power monitor register read/write test utility program for the Henny Penny UHC.
 * @file       pwrmonrw.cpp
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
#include <vector>
#include <map>
#include <cctype>

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
#define REG_READ_ALL 0xFFFFFFFFU

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
static vector<uint32_t> registers =
{
   REG_SOFTRESET,
   REG_SYSSTATUS,
   REG_FUNCEN,
   REG_SAGTH,
   REG_SMALLPMOD,
   REG_CHIPID,
   REG_LASTDATA,
   REG_LSB,
   REG_TYPE,
   REG_CALSTART,
   REG_PLCONSTH,
   REG_PLCONSTL,
   REG_LGAIN,
   REG_LPHI,
   REG_NGAIN,
   REG_NPHI,
   REG_PSTARTTH,
   REG_PNOLTH,
   REG_QSTARTTH,
   REG_QNOLTH,
   REG_MMODE,
   REG_CS1,
   REG_ADJSTART,
   REG_UGAIN,
   REG_IGAINL,
   REG_IGAINN,
   REG_UOFFSET,
   REG_IOFFSETL,
   REG_IOFFSETN,
   REG_POFFSETL,
   REG_QOFFSETL,
   REG_POFFSETN,
   REG_QOFFSETN,
   REG_CS2,
   REG_APENERGY,
   REG_ANENERGY,
   REG_ATENERGY,
   REG_RPENERGY,
   REG_RNENERGY,
   REG_RTENERGY,
   REG_ENSTATUS,
   REG_IRMS,
   REG_URMS,
   REG_PMEAN,
   REG_QMEAN,
   REG_FREQ,
   REG_POWERF,
   REG_PANGLE,
   REG_SMEAN,
   REG_IRMS2,
   REG_PMEAN2,
   REG_QMEAN2,
   REG_POWERF2,
   REG_PANGLE2,
   REG_SMEAN2
};

/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static void print_usage(char const * const program_name);
static void do_reg_read(uint32_t reg, bool show_status);


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
   uint32_t reg;
   uint32_t value;
   bool do_read = false;
   bool do_write = false;
   int retval = 0;

   while (((c = getopt(argc, argv, "r:w:h")) != EOF) && (0 == retval))
   {
      switch (c)
      {
         case 'r':
            do_read = true;
            if (toupper(*optarg) == 'A')
            {
               reg = REG_READ_ALL;
            }
            else
            {
               reg = strtoul(optarg, NULL, 0);
            }

            break;

         case 'w':
            do_write = true;
            reg = strtoul(optarg, NULL, 0);
            break;

         case 'h':
         case '?':
            retval = 1;
            break;
      }
   }

   if ((do_read && do_write) || (!do_read && !do_write))
   {
      cerr << "Must specify either read or write" << endl;
      retval = -1;
   }

   if (0 == retval)
   {
      if (do_write)
      {
         if ((optind + 1) != argc)
         {
            cerr << "Invalid number of arguments" << endl;
            retval = 1;
         }
         else
         {
            value = strtoul(argv[optind], NULL, 0);
         }
      }
      else
      {
         // Must be do_read
         if (optind != argc)
         {
            cerr << "Invalid number of arguments" << endl;
            retval = 1;
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
         if (do_read)
         {
            if (REG_READ_ALL != reg)
            {
               do_reg_read(reg, true);
            }
            else
            {
               for (vector<uint32_t>::iterator ir = registers.begin(); registers.end() != ir; ++ir)
               {
                  do_reg_read(*ir, false);
               }
            }
         }
         else
         {
            // Must be do_write
            retval = atm90e26_reg_write((uint8_t)reg, (uint16_t)value);
            cout << "status: " << retval << endl;
         }
      }

      atm90e26_deinit();
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
   cout << "usage: " << program_name << " -r <reg>" << endl;
   cout << "usage: " << program_name << " -r a" << endl;
   cout << "       " << program_name << " -w <reg> <value>" << endl;
   cout << "    where -r means read, -w means write" << endl;
   cout << "    <reg> is the address of the register to read or write" << endl;
   cout << "    <value> is the value to write to the register" << endl;
   cout << "    -r a dumps all registers" << endl;
}

static void do_reg_read(uint32_t reg, bool show_status)
{
   uint16_t regval;
   int retval = atm90e26_reg_read((uint8_t)reg, &regval);

   if (show_status)
   {
      cout << " status: " << retval << ", ";
   }

   cout << "reg: 0x" << hex << reg << ", value: 0x" << regval << dec << endl;
}

/******************************************      End of file        ************************************/
