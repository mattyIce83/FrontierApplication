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
 * @brief      GPIO input read test utility program for the Henny Penny UHC.
 * @file       gpioread.cpp
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
#define CF1_PULSE_OUT_VALUE   "/sys/class/gpio/gpio39/value"   // P8/4  GPIO1_7
#define CF2_PULSE_OUT_VALUE   "/sys/class/gpio/gpio35/value"   // P8/6  GPIO1_3
#define WARN_OUT_PULSE_VALUE  "/sys/class/gpio/gpio44/value"   // P8/12 GPIO1_12
#define ZERO_CROSS_VALUE      "/sys/class/gpio/gpio46/value"   // P8/16 GPIO1_14
#define IRQ_VALUE             "/sys/class/gpio/gpio65/value"   // P8/18 GPIO2_1
#define POWER_GOOD_VALUE      "/sys/class/gpio/gpio87/value"   // P8/29 GPIO2_23
#define ID0_VALUE             "/sys/class/gpio/gpio9/value"    // P8/33 GPIO0_9
#define ID1_VALUE             "/sys/class/gpio/gpio8/value"    // P8/35 GPIO0_8
#define ID2_VALUE             "/sys/class/gpio/gpio10/value"   // P8/31 GPIO0_10
#define FAN_1_TACH_VALUE      "/sys/class/gpio/gpio78/value"   // P8/37 GPIO2_14
#define FAN_2_TACH_VALUE      "/sys/class/gpio/gpio76/value"   // P8/39 GPIO2_12

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
/** An associative array mapping schematic signal name to GPIO device.
 * Make sure this initializer list is in alphabetical order based on the key. That way, when running an
 * iterator over the map they will be in this order. Without this, the program output with no arguments
 * makes no sense.
 */
static map<string, string> gpios =
{
   { "CF1_PULSE_OUT",         CF1_PULSE_OUT_VALUE  },
   { "CF2_PULSE_OUT",         CF2_PULSE_OUT_VALUE  },
   { "FAN_1_TACH",            FAN_1_TACH_VALUE     },
   { "FAN_2_TACH",            FAN_2_TACH_VALUE     },
   { "ID0",                   ID0_VALUE            },
   { "ID1",                   ID1_VALUE            },
   { "ID2",                   ID2_VALUE            },
   { "IRQ",                   IRQ_VALUE            },
   { "POWER_GOOD",            POWER_GOOD_VALUE     },
   { "WARN_OUT_PULSE",        WARN_OUT_PULSE_VALUE },
   { "ZERO_CROSS",            ZERO_CROSS_VALUE     }
};


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static void print_usage(char const * const program_name);
static int print_gpio_input(string gpio_path);


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

   if (1 == argc)
   {
      // No command line arguments, so print all the GPIO input values.
      map<string, string>::iterator igpios;
      for (igpios = gpios.begin(); (gpios.end() != igpios) && (0 == retval); ++igpios)
      {
         retval = print_gpio_input(igpios->second);
         if (0 != retval)
         {
            cerr << endl << "Error reading GPIO " << igpios->first << endl;
         }
      }
   }
   else
   {
      int argindex = 1;
      while ((argc != argindex) && (0 == retval))
      {
         string gpio_path = string(gpios[argv[argindex]]);
         if (gpio_path.empty())
         {
            cerr << endl << "Invalid argument: " << argv[argindex] << endl;
            print_usage(argv[0]);
            retval = -1;
         }
         else
         {
            retval = print_gpio_input(gpio_path);
            if (0 != retval)
            {
               cerr << endl << "Error reading GPIO " << argv[argindex] << endl;
            }
         }

         ++argindex;
      }
   }

   if (0 == retval)
   {
      // No error messages have been produced, so output a final newline.
      cout << endl;
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
   cout << "usage: " << program_name << " [<signal>[ <signal> ...]]" << endl;
   cout << "    where <signal> is one of:" << endl;
   map<string, string>::iterator igpios;
   for (igpios = gpios.begin(); gpios.end() != igpios; ++igpios)
   {
      cout << "    " << igpios->first << endl;
   }

   cout << "With no arguments, outputs all signals in the order shown." << endl;
}

/**
 * @brief Read and print the input value of the given GPIO.
 *
 * @param[in] gpio_path The path to the GPIO device's value.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
static int print_gpio_input(string gpio_path)
{
   int retval = 0;

   int fd = open(gpio_path.c_str(), O_RDONLY);
   if (0 > fd)
   {
      retval = -1;
   }
   else
   {
      uint8_t buf;
      retval = read(fd, (void *)&buf, 1);
      if (1 == retval)
      {
         // Read one character.
         retval = 0;
         cout << buf << ' ';
      }
      else if (0 <= retval)
      {
         // Read zero or > 1 characters.
         retval = -1;
      }
      else
      {
         // read() failed, just report the return value as is
      }

      close(fd);
   }

   return retval;
}


/******************************************      End of file        ************************************/
