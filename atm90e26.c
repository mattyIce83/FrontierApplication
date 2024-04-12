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
 * @brief       ATM90E26 power monitor driver implementation.
 * @file        atm90e26.c
 * @author      Roger Chaplin
 *
 ********************************************************************************************************
 * This is the implementation of a simple driver for the ATM90E26 energy metering IC. The initial
 * implementation provides only those functions needed by the Henny Penny Universal Heating Cabinet.
 *
 * The connection to the IC is via SPI.
 ********************************************************************************************************
 */
// TODO: Take out section headers of sections that are not used when finished developing this module.

/*
 ********************************************************************************************************
 *                                           INCLUDE FILES
 ********************************************************************************************************
 */
/********************************************** System *************************************************/
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

/********************************************    User   ************************************************/
#include "atm90e26.h"

// HACK: We don't know why the measured current is too small by a factor of 10.
#define NEED_I_BOOST

/*
 ********************************************************************************************************
 *                                               EXTERNAL
 ********************************************************************************************************
 */
/********************************************** Functions **********************************************/

/*********************************************  Variables **********************************************/


/*
 ********************************************************************************************************
 *                                               DEFINES
 ********************************************************************************************************
 */
/****************************************** Symbolic Constants ******************************************/
#define READ_BIT           0x80U
#define SOFTRESET_KEY      0x789AU
#define CHIPID_90E26       0x2614U
#define LGAIN_MASK         0xE000U
#define LGAIN_SHIFT        13
#define LGAIN_1            4U
#define LGAIN_4            0U
#define LGAIN_8            1U
#define LGAIN_16           2U
#define LGAIN_24           3U
#define START_NO_CHECK     0x5678U
#define START_WITH_CHECK   0x8765U
#define IGAINL_DEF         0x21C1U
#define CS2_DEF            0xE20EU
#define SAG_EN             0x0020U
#define SAG_WO             0x0010U


/******************************************      Macros        ******************************************/


/*
 ********************************************************************************************************
 *                                               DATA TYPES
 ********************************************************************************************************
 */


/*
 ********************************************************************************************************
 *                                                VARIABLES
 ********************************************************************************************************
 */
/************************************************* Global **********************************************/

/************************************************* Local ***********************************************/
/** The file descriptor of the SPI device. */
static int fd;

/** Whether the driver and IC have been successfully initialized. */
static bool initialized = false;

/** Whether the */


/*
 ********************************************************************************************************
 *                                                 CONSTANTS
 ********************************************************************************************************
 */


/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DECLARATIONS
 ********************************************************************************************************
 */
static int32_t atm90e26_single_reg_read(uint8_t reg, uint16_t * const p_regval);


/*
 ********************************************************************************************************
 *                                         PUBLIC FUNCTIONS DEFINITIONS
 ********************************************************************************************************
 */

/**
 * @brief Initialize the ATM90E26 driver and IC.
 *
 * @param[in] spidev The SPI device to which the ATM90E26 is connected.
 *
 * @param[in] spifreqHz The SPI frequency to use, in Hz.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_init(char const * const spidev, uint32_t const spifreqHz)
{
   int32_t retval = 0;

   fd = open(spidev, O_RDWR);
   if (0 > fd)
   {
      retval = errno;
   }

   if (0 == retval)
   {
      retval = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spifreqHz);
   }

   // The ATM90E26 uses SPI mode 0.
   if (0 == retval)
   {
      uint32_t const spi_mode = SPI_MODE_0;
      retval = ioctl(fd, SPI_IOC_WR_MODE, &spi_mode);
   }

   // Verify the correct metering IC is there. TODO: The register is not documented in the data sheet. If
   // it's really not there, take this code out, along with the associated #defines.
#if 0
   if (0 == retval)
   {
      uint16_t chipid;
      retval = atm90e26_reg_read(REG_CHIPID, &chipid);
      if ((0 == retval) && (CHIPID_90E26 != chipid))
      {
         //retval = -1;
      }
   }
#endif

   initialized = (0 == retval);

   return retval;
}

/**
 * @brief Deinitialize the ATM90E26 driver and IC.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_deinit(void)
{
   int32_t retval = close(fd);
   fd = -1;
   initialized = false;

   return retval;
}

/**
 * @brief Start the metering and measurement operations.
 *
 * This function takes the desired L gain as an argument. This should be specified to match the voltage
 * divider. All other configuration settings are hard-coded in the driver.
 *
 * @param[in] lgain The desired L gain.
 *
 * @param[in] sag_level_volts The AC line voltage level at which to warn.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_start(Lgain_t const lgain, float sag_level_volts)
{
   int32_t retval = -1;

   if (initialized)
   {
      retval = 0;
      uint16_t lgain_value;
      switch (lgain)
      {
         case lgain_1:
            lgain_value = LGAIN_1;
            break;

         case lgain_4:
            lgain_value = LGAIN_4;
            break;

         case lgain_8:
            lgain_value = LGAIN_8;
            break;

         case lgain_16:
            lgain_value = LGAIN_16;
            break;

         case lgain_24:
            lgain_value = LGAIN_24;
            break;

         default:
            retval = -EINVAL;
      }

      uint16_t regval = 0;

      if (0 == retval)
      {
         // Start with a soft reset.
         retval = atm90e26_reg_write(REG_SOFTRESET, SOFTRESET_KEY);
      }

      // Set the LGAIN bits in the MMODE register.
      if (0 == retval)
      {
         retval = atm90e26_reg_read(REG_MMODE, &regval);
      }

      if (0 == retval)
      {
         regval &= ~LGAIN_MASK;
         regval |= lgain_value << LGAIN_SHIFT;
         retval = atm90e26_reg_write(REG_MMODE, regval);
      }

      // Start the metering operation with no checksum test of the configuration registers.
      if (0 == retval)
      {
         retval = atm90e26_reg_write(REG_CALSTART, START_NO_CHECK);
      }

      // Start the measuring operation with no checksum test of the configuration registers.
      if (0 == retval)
      {
         retval = atm90e26_reg_write(REG_ADJSTART, START_NO_CHECK);
      }

      if (0 == retval)
      {
         retval = atm90e26_reg_write(REG_IGAINL, IGAINL_DEF);
      }

      if (0 == retval)
      {
         retval = atm90e26_reg_write(REG_CS2, CS2_DEF);
      }

      if (0 == retval)
      {
         // Set the voltage sag threshold to the given value.
         // The sag threshold is integer V * 100.
         regval = (uint16_t)(sag_level_volts * 100.0F);
         retval = atm90e26_reg_write(REG_SAGTH, regval);
      }

      // Enable voltage sag detection on the WarnOut pin.
      if (0 == retval)
      {
         retval = atm90e26_reg_read(REG_FUNCEN, &regval);
      }

      if (0 == retval)
      {
         regval |= (SAG_EN | SAG_WO);
         retval = atm90e26_reg_write(REG_FUNCEN, regval);
      }
   }

   return retval;
}

/**
 * @brief Return the measured line current.
 *
 * @param[out] p_irms A pointer to where to return the measured current (RMS amperes).
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_irms_get(float * const p_irms)
{
   uint16_t ui_irms;
   int32_t retval = atm90e26_irms_raw_get(&ui_irms);

   if (0 == retval)
   {
      // The Irms register contains mA RMS. Divide by 1000 to convert to amperes.
#ifdef NEED_I_BOOST
      *p_irms = (float)ui_irms / 100.0F;
#else
      *p_irms = (float)ui_irms / 1000.0F;
#endif
   }

   return retval;
}

/**
 * @brief Return the measured line voltage.
 *
 * @param[out] p_vrms A pointer to where to return the measured voltage (RMS volts).
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_vrms_get(float * const p_vrms)
{
   uint16_t ui_vrms;
   int32_t retval = atm90e26_vrms_raw_get(&ui_vrms);

   if (0 == retval)
   {
      // The Urms register contains 1/100 V RMS. Divide by 100 to convert to volts.
      *p_vrms = (float)ui_vrms / 100.0F;
   }

   return retval;
}

/**
 * @brief Return the measured active power.
 *
 * @param[out] p_power A pointer to where to return the measured power (W).
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_power_get(float * const p_power)
{
   int32_t retval = -1;

   if (initialized)
   {
      uint16_t ui_power;
      retval = atm90e26_reg_read(REG_PMEAN, &ui_power);
      if (0 == retval)
      {
         // The Pmean register contains signed watts.
         int16_t i_power = (int16_t)ui_power;
         *p_power = (float)i_power;
      }
   }

   return retval;
}

/**
 * @brief Return the measured line current raw register value.
 *
 * @param[out] p_irms A pointer to where to return the Irms register value.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_irms_raw_get(uint16_t * const p_irms)
{
   int32_t retval = -1;

   if (initialized)
   {
      retval = atm90e26_reg_read(REG_IRMS, p_irms);
   }

   return retval;
}

/**
 * @brief Return the measured line voltage raw register value.
 *
 * @param[out] p_vrms A pointer to where to return the Urms register value.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_vrms_raw_get(uint16_t * const p_vrms)
{
   int32_t retval = -1;

   if (initialized)
   {
      retval = atm90e26_reg_read(REG_URMS, p_vrms);
   }

   return retval;
}

/**
 * @brief Read the given ATM90E26 register.
 *
 * This function implements the recommendation in app note 46102, section 4.3, to use the LastData register
 * to verify correct communication while reading a register.
 *
 * @param[in] reg The address of the register to read.
 *
 * @param[out] p_regval A pointer to where to store the register value.
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_reg_read(uint8_t reg, uint16_t * const p_regval)
{
   uint16_t target_reg_val;
   uint16_t lastdata_val;

   // Read the target register.
   int32_t retval = atm90e26_single_reg_read(reg, &target_reg_val);

   if (0 == retval)
   {
      // Read the LastData register.
      retval = atm90e26_single_reg_read(REG_LASTDATA, &lastdata_val);
   }

   bool values_are_equal = false;
   if (0 == retval)
   {
      // Compare the target register and LastData register values, use this flag later on.
      values_are_equal = (target_reg_val == lastdata_val);
   }

   if ((0 == retval) && values_are_equal)
   {
      // No driver errors, and both read values are the same.
      *p_regval = target_reg_val;
   }
   else if ((0 == retval) && !values_are_equal)
   {
      // No driver errors, but different read values. Read the LastData register again and return that value.
      retval = atm90e26_single_reg_read(REG_LASTDATA, &lastdata_val);

      if (0 == retval)
      {
         *p_regval = lastdata_val;
      }
   }
   else
   {
      // Non-zero retval; nothing else is needed here.
   }

   return retval;
}

/**
 * @brief Write a value to the given ATM90E26 register.
 *
 * @param[in] reg The address of the register to write.
 *
 * @param[in] regval The value to write to the register
 *
 * @return 0 if no error, otherwise a standard error code.
 */
int32_t atm90e26_reg_write(uint8_t reg, uint16_t const regval)
{
   uint8_t buf[4];

   (void)memset((void *)buf, 0, sizeof(buf));

   buf[0] = reg & ~READ_BIT;  // Instruct the ATM90E26 that we are writing the register.
   buf[1] = (regval >> 8) & 0xFFU;
   buf[2] = regval & 0xFFU;

   int32_t retval = write(fd, buf, 3);
   if (3 == retval)
   {
      retval = 0;
   }
   else if (0 <= retval)
   {
      retval = -1;
   }
   else
   {
      // Nothing else is needed here.
   }

   return retval;
}

/*
 ********************************************************************************************************
 *                                       FILE LOCAL FUNCTIONS DEFINITIONS
 ********************************************************************************************************
 */

/**
 * @brief Read a single register from the ATM90E26.
 *
 * @param[in] reg The register address to read.
 *
 * @param[out] p_regval A pointer to where to store the register value.
 *
 * @return Zero if no errors occurred, or a negative error code.
 */
static int32_t atm90e26_single_reg_read(uint8_t reg, uint16_t * const p_regval)
{
   struct spi_ioc_transfer xfer[2];
   uint8_t buf[4];

   (void)memset((void *)xfer, 0, sizeof(xfer));
   (void)memset((void *)buf, 0, sizeof(buf));

   buf[0] = reg | READ_BIT;         // Instruct the ATM90E26 that we are reading the register.
   xfer[0].tx_buf = (__u64)(uint32_t)buf;
   xfer[0].len = 1U;

   xfer[1].rx_buf = (__u64)(uint32_t)buf;
   xfer[1].len = 2U;               // The register is 16 bits (2 bytes) long.

   int32_t retval = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
   if (0 < retval)
   {
      *p_regval = (uint16_t)((buf[0] << 8) | buf[1]);
      retval = 0;
   }
   else if (0 == retval)
   {
      retval = -1;
   }
   else
   {
      // Nothing else is needed here.
   }

   return retval;
}


/*
 ********************************************************************************************************
 *                                           ISR FUNCTIONS DEFINITIONS
 ********************************************************************************************************
 */


/******************************************      End of file        ************************************/

