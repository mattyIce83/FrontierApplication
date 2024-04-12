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
 * @brief      RTD multiplexer test utility program for the Henny Penny UHC.
 * @file       rtdmux.cpp
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
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/spi/spidev.h>
#include <iostream>
#include <string>

/********************************************    User   ************************************************/
#include "PGA117.h"

using namespace std;

/*
 ********************************************************************************************************
 *                                               DEFINES
 ********************************************************************************************************
 */
/****************************************** Symbolic Constants ******************************************/
/** GPIO mappings. */
#define MUX_DIS_VALUE         "/sys/class/gpio/gpio86/value"   // P8/27 GPIO2_22

/** SPI port for the RTD mux. */
#define MUX_SPI_DEVICE        "/dev/spidev0.0"
#define MUX_SPI_SPEED			1000000

/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
static int fd_spi_bus_0 = -1;


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static void print_usage(char const * const program_name);
static int set_gpio_output(string gpio_path, char const *value);
static int init_PGA117_spi();
static int deinit_PGA117_spi();
static int select_mux_channel(int mux, int channel);

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

   // There must be either zero or two arguments (not counting the program name).
   if ((3 != argc) && (1 != argc))
   {
      cerr << "Invalid number of arguments: " << argc - 1 << endl;
      print_usage(argv[0]);
      retval = -1;
   }
   else if (1 == argc)
   {
      // Zero arguments: turn off all mux channels and disable the mux ICs.
      string on_val = "1";
      retval = set_gpio_output(MUX_DIS_VALUE, on_val.c_str());
   }
   else
   {
      // There must be two arguments: validate them.
      int mux = atoi(argv[1]);
      int channel = atoi(argv[2]);
      if ((0 != mux) && (1 != mux))
      {
         cerr << "Invalid mux: " << argv[1] << endl;
         retval = -1;
      }

      if ((0 > channel) || (9 < channel))
      {
         cerr << "Invalid channel: " << argv[2] << endl;
         retval = -1;
      }

      if (0 != retval)
      {
         print_usage(argv[0]);
      }
      else
      {
         // Arguments are valid: enable the mux ICs and close the specified channel.
         string off_val = "0";
         retval = set_gpio_output(MUX_DIS_VALUE, off_val.c_str());
         if (0 == retval)
         {
            retval = init_PGA117_spi();
         }

         if (0 == retval)
         {
            retval = select_mux_channel(mux, channel);
         }
      }

      if (-1 != fd_spi_bus_0)
      {
         (void)deinit_PGA117_spi();
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
   cout << "usage: " << program_name << " [<mux> <channel>]" << endl;
   cout << "    close <channel> (0 - 9) on <mux> (0 - 1)" << endl;
   cout << "    with no arguments, open all channels on both muxes" << endl;
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

static int init_PGA117_spi()
{
   int retval = 0;
   uint32_t spifreqHz = MUX_SPI_SPEED;

  	fd_spi_bus_0 = open(MUX_SPI_DEVICE, O_RDWR);

   if (0 > fd_spi_bus_0)
   {
      retval = 1;
   }

   if (0 == retval)
   {
      retval = ioctl(fd_spi_bus_0, SPI_IOC_WR_MAX_SPEED_HZ, &spifreqHz);
   }

   // The PGA117 uses SPI mode 0.
   if (0 == retval)
   {
      uint32_t spi_mode = SPI_MODE_0;
      retval = ioctl(fd_spi_bus_0, SPI_IOC_WR_MODE, &spi_mode);
   }

   return retval;
}

static int deinit_PGA117_spi()
{
   int retval = close(fd_spi_bus_0);

   return retval;
}

static int select_mux_channel(int mux, int channel)
{
	int retVal = 0;
	uint16_t mux1Command = 0;
	uint16_t mux2Command = 0;
	char buf[4];

	/* Format the commands */
	mux1Command = PGA117_CMD_WRITE;
	mux2Command = PGA117_DCCMD_WRITE;

   if (1 == mux)
   {
      mux2Command |= (channel << PGA117_CHANNEL_SHIFT);
   }
   else
   {
      // Must be mux 0
      mux1Command |= (channel << PGA117_CHANNEL_SHIFT);
   }

	buf[0] = (mux2Command >> 8) & 0xFF;
	buf[1] = mux2Command & 0xFF;
	buf[2] = (mux1Command >> 8) & 0xFF;
	buf[3] = mux1Command & 0xFF;

	// write the commands
	int ret = write(fd_spi_bus_0, buf, sizeof(buf));
	if (ret != 4)
	{
		retVal = -1;
	}

	return retVal;
}


/******************************************      End of file        ************************************/
