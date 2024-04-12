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
 * @brief      GPIO output set test utility program for the Henny Penny UHC.
 * @file       gpioset.cpp
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
#include <map>
#include <string>

/********************************************    User   ************************************************/

using namespace std;

/*
 ********************************************************************************************************
 *                                               DEFINES
 ********************************************************************************************************
 */
/****************************************** Symbolic Constants ******************************************/
/** GPIO mappings. */
#define PWR_MON_CS_VALUE      "/sys/class/gpio/gpio7/value"    // P9/42 GPIO0_7
#define MUX_CS_VALUE          "/sys/class/gpio/gpio4/value"    // P9/17 GPIO0_4
#define VAC220_RELAY_VALUE    "/sys/class/gpio/gpio22/value"   // P8/19 GPIO0_22
#define MUX_DIS_VALUE         "/sys/class/gpio/gpio86/value"   // P8/27 GPIO2_22
#define FAN_1_ON_OFF_VALUE    "/sys/class/gpio/gpio72/value"   // P8/43 GPIO2_8
#define FAN_2_ON_OFF_VALUE    "/sys/class/gpio/gpio74/value"   // P8/41 GPIO2_10
#define HEATER_1_SW_VALUE     "/sys/class/gpio/gpio47/value"   // P8/15 GPIO1_47
#define HEATER_2_SW_VALUE     "/sys/class/gpio/gpio27/value"   // P8/17 GPIO0_27
#define HEATER_3_SW_VALUE     "/sys/class/gpio/gpio61/value"   // P8/26 GPIO1_61
#define HEATER_4_SW_VALUE     "/sys/class/gpio/gpio88/value"   // P8/28 GPIO2_88
#define HEATER_5_SW_VALUE     "/sys/class/gpio/gpio89/value"   // P8/30 GPIO2_89
#define HEATER_6_SW_VALUE     "/sys/class/gpio/gpio81/value"   // P8/34 GPIO2_81
#define HEATER_7_SW_VALUE     "/sys/class/gpio/gpio80/value"   // P8/36 GPIO2_80
#define HEATER_8_SW_VALUE     "/sys/class/gpio/gpio79/value"   // P8/38 GPIO2_79
#define HEATER_9_SW_VALUE     "/sys/class/gpio/gpio77/value"   // P8/40 GPIO2_77
#define HEATER_10_SW_VALUE    "/sys/class/gpio/gpio75/value"   // P8/42 GPIO2_75
#define HEATER_11_SW_VALUE    "/sys/class/gpio/gpio70/value"   // P8/45 GPIO2_70
#define HEATER_12_SW_VALUE    "/sys/class/gpio/gpio71/value"   // P8/46 GPIO2_71

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
/** An associative array mapping schematic signal name to GPIO device. */
static map<string, string> gpios =
{
   { "VAC220",    VAC220_RELAY_VALUE   },
   { "MUX_DIS",   MUX_DIS_VALUE        },
   { "FAN1",      FAN_1_ON_OFF_VALUE   },
   { "FAN2",      FAN_2_ON_OFF_VALUE   },
   { "H1",        HEATER_1_SW_VALUE    },
   { "H2",        HEATER_2_SW_VALUE    },
   { "H3",        HEATER_3_SW_VALUE    },
   { "H4",        HEATER_4_SW_VALUE    },
   { "H5",        HEATER_5_SW_VALUE    },
   { "H6",        HEATER_6_SW_VALUE    },
   { "H7",        HEATER_7_SW_VALUE    },
   { "H8",        HEATER_8_SW_VALUE    },
   { "H9",        HEATER_9_SW_VALUE    },
   { "H10",       HEATER_10_SW_VALUE   },
   { "H11",       HEATER_11_SW_VALUE   },
   { "H12",       HEATER_12_SW_VALUE   }
};


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static void print_usage(char const * const program_name);
static int set_gpio_output(string gpio_path, char const *value);


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
   int retval = 0;

   // There must be at least two arguments, and an even number of them (not counting the program name).
   if ((3 > argc) || ((argc % 2) != 1))
   {
      cerr << "Invalid number of arguments: " << argc - 1 << endl;
      print_usage(argv[0]);
      retval = -1;
   }
   else
   {
      int argindex = 1;
      while ((argc != argindex) && (0 == retval))
      {
         string gpio_path = gpios[argv[argindex]];
         char *value = argv[argindex + 1];

         if (gpio_path.empty())
         {
            cerr << "Invalid argument: " << argv[argindex] << endl;
            retval = -1;
         }

         if (('0' != *value) && ('1' != *value))
         {
            cerr << "Invalid argument: " << *value << endl;
            retval = -1;
         }

         if (0 != retval)
         {
            print_usage(argv[0]);
         }
         else
         {
            retval = set_gpio_output(gpio_path, value);
            if (0 != retval)
            {
               cerr << endl << "Error writing GPIO " << argv[argindex] << endl;
            }
         }

         argindex += 2;
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
   cout << "usage: " << program_name << " <signal> <state>[ <signal> <state> ...]" << endl;
   cout << "    where <signal> is one of:" << endl;
   map<string, string>::iterator igpios;
   for (igpios = gpios.begin(); gpios.end() != igpios; ++igpios)
   {
      cout << "    " << igpios->first << endl;
   }

   cout << "    and <state> is either 0 or 1" << endl;
}

/**
 * @brief Set the output value of the given GPIO.
 *
 * @param[in] gpio_path The path to the GPIO device's value.
 *
 * @param[in] value The value to which to set the output.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
static int set_gpio_output(string gpio_path, char const *value)
{
   int retval = 0;

   int fd = open(gpio_path.c_str(), O_WRONLY);
   if (0 > fd)
   {
      retval = -1;
   }
   else
   {
      retval = write(fd, (const void *)value, 1);
      if (1 == retval)
      {
         // Wrote one character.
         retval = 0;
      }
      else if (0 <= retval)
      {
         // Wrote zero or > 1 characters.
         retval = -1;
      }
      else
      {
         // write() failed, just report the return value as is
      }

      close(fd);
   }

   return retval;
}


/******************************************      End of file        ************************************/
