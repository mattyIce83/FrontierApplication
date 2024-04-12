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
 * @brief      ADC read test utility program for the Henny Penny UHC.
 * @file       adcread.cpp
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
#include <sstream>
#include <algorithm>

/********************************************    User   ************************************************/

using namespace std;

/*
 ********************************************************************************************************
 *                                               DEFINES
 ********************************************************************************************************
 */
/****************************************** Symbolic Constants ******************************************/
#define NUM_GPIOS 16

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
/** A list of available GPIO pins. */
static array<int, NUM_GPIOS> available_gpios =
{
   20, 26, 27, 34, 36, 38, 45, 47, 48, 49, 60, 62, 63, 70, 115, 117
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
   int c;
   int channel;
   int gpionum = -1;
   int retval = 0;

   while (((c = getopt(argc, argv, "ht:")) != EOF) && (0 == retval))
   {
      switch (c)
      {
         case 't':
         {
            gpionum = atoi(optarg);

            int *gpio = find(available_gpios.begin(), available_gpios.end(), gpionum);
            if (available_gpios.end() == gpio)
            {
               cerr << "Invalid -t argument: " << optarg << endl;
               print_usage(argv[0]);
               retval = -1;
            }

            break;
         }

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
         cerr << "Too many arguments" << endl;
         print_usage(argv[0]);
         retval = 1;
      }
      else
      {
         channel = atoi(argv[optind]);
         if ((0 != channel) && (1 != channel))
         {
            cerr << "Invalid channel: " << argv[optind] << endl;
            retval = -1;
         }
      }
   }

   if (0 == retval)
   {
      string gpio_path;

      if (-1 != gpionum)
      {
         stringstream ss;
         ss << "/sys/class/gpio/gpio" << gpionum << "/value";
         gpio_path = ss.str();
         string on_val = "1";
         retval = set_gpio_output(gpio_path, on_val.c_str());
      }

      if (0 != retval)
      {
         cerr << "Failed to set GPIO " << gpionum << " high" << endl;
      }
      else
      {
         // Take ADC reading.
      }

      if (0 == retval)
      {
         if (-1 != gpionum)
         {
            string off_val = "0";
            retval = set_gpio_output(gpio_path, off_val.c_str());
            if (0 != retval)
            {
               cerr << "Failed to set GPIO " << gpionum << " low" << endl;
            }
         }
      }

      if (0 == retval)
      {
         // Print ADC reading. Do this after turning the GPIO output off so that the cout time doesn't get
         // reflected in that GPIO pulse.
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
   cout << "usage: " << program_name << " [-t <gpio>] <channel>" << endl;
   cout << "    where <channel> is the ADC channel to read (0 or 1)" << endl;
   cout << "    and <gpio> is the GPIO number on which to indicate start and stop of the ADC conversion" << endl;
   cout << "    and must be one of:" << endl << "    ";
   for (array<int, NUM_GPIOS>::iterator iag = available_gpios.begin(); available_gpios.end() != iag; ++iag)
   {
      cout << *iag << " ";
   }

   cout << endl;
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
