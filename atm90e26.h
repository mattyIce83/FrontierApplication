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
 * @brief       ATM90E26 power monitor driver header file.
 * @file        atm90e26.h
 * @author      Roger Chaplin
 *
 ********************************************************************************************************
 */
// TODO: Take out section headers of sections that are not used when finished developing this module.


/*
 ********************************************************************************************************
 *                                                   MODULE
 ********************************************************************************************************
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/*
 ********************************************************************************************************
 *                                             INCLUDE FILES
 ********************************************************************************************************
 */
/*********************************************** System ************************************************/
#include <stdint.h>

/**********************************************   User   ***********************************************/


/*
 ********************************************************************************************************
 *                                                 EXTERNAL
 ********************************************************************************************************
 */
/*********************************************** Functions *********************************************/

/**********************************************  Variables *********************************************/


/*
 ********************************************************************************************************
 *                                                 DEFINES
 ********************************************************************************************************
 */
/******************************************* Symbolic Constants ****************************************/
#define ATM90E26_SPI_FREQ_DEFAULT_HZ   15625U

#define REG_SOFTRESET   0x00U
#define REG_SYSSTATUS   0x01U
#define REG_FUNCEN      0x02U
#define REG_SAGTH       0x03U
#define REG_SMALLPMOD   0x04U
#define REG_CHIPID      0x05U    // Undocumented
#define REG_LASTDATA    0x06U
#define REG_LSB         0x08U
#define REG_TYPE        0x16U    // Undocumented
#define REG_CALSTART    0x20U
#define REG_PLCONSTH    0x21U
#define REG_PLCONSTL    0x22U
#define REG_LGAIN       0x23U
#define REG_LPHI        0x24U
#define REG_NGAIN       0x25U
#define REG_NPHI        0x26U
#define REG_PSTARTTH    0x27U
#define REG_PNOLTH      0x28U
#define REG_QSTARTTH    0x29U
#define REG_QNOLTH      0x2AU
#define REG_MMODE       0x2BU
#define REG_CS1         0x2CU
#define REG_ADJSTART    0x30U
#define REG_UGAIN       0x31U
#define REG_IGAINL      0x32U
#define REG_IGAINN      0x33U
#define REG_UOFFSET     0x34U
#define REG_IOFFSETL    0x35U
#define REG_IOFFSETN    0x36U
#define REG_POFFSETL    0x37U
#define REG_QOFFSETL    0x38U
#define REG_POFFSETN    0x39U
#define REG_QOFFSETN    0x3AU
#define REG_CS2         0x3BU
#define REG_APENERGY    0x40U
#define REG_ANENERGY    0x41U
#define REG_ATENERGY    0x42U
#define REG_RPENERGY    0x43U
#define REG_RNENERGY    0x44U
#define REG_RTENERGY    0x45U
#define REG_ENSTATUS    0x46U
#define REG_IRMS        0x48U
#define REG_URMS        0x49U
#define REG_PMEAN       0x4AU
#define REG_QMEAN       0x4BU
#define REG_FREQ        0x4CU
#define REG_POWERF      0x4DU
#define REG_PANGLE      0x4EU
#define REG_SMEAN       0x4FU
#define REG_IRMS2       0x68U
#define REG_PMEAN2      0x6AU
#define REG_QMEAN2      0x6BU
#define REG_POWERF2     0x6DU
#define REG_PANGLE2     0x6EU
#define REG_SMEAN2      0x6FU

/*******************************************      Macros        ****************************************/


/*
 ********************************************************************************************************
 *                                                CONSTANTS
 ********************************************************************************************************
 */
/** Allowed values for L gain. */
typedef enum Lgain
{
   lgain_1,
   lgain_4,
   lgain_8,
   lgain_16,
   lgain_24
} Lgain_t;

/*
 ********************************************************************************************************
 *                                                DATA TYPES
 ********************************************************************************************************
 */


/*
 ********************************************************************************************************
 *                                                  TABLES
 ********************************************************************************************************
 */


/*
 ********************************************************************************************************
 *                                                 VARIABLES
 ********************************************************************************************************
 */


/*
 ********************************************************************************************************
 *                                             FUNCTION PROTOTYPES
 ********************************************************************************************************
 */
int32_t atm90e26_init(char const * const spidev, uint32_t const spifreq);
int32_t atm90e26_deinit(void);
int32_t atm90e26_reg_read(uint8_t reg, uint16_t * const p_regval);
int32_t atm90e26_reg_write(uint8_t reg, uint16_t const regval);
int32_t atm90e26_start(Lgain_t const lgain, float sag_level_volts);
int32_t atm90e26_irms_get(float * const p_irms);
int32_t atm90e26_vrms_get(float * const p_vrms);
int32_t atm90e26_power_get(float * const p_power);
int32_t atm90e26_irms_raw_get(uint16_t * const p_irms);
int32_t atm90e26_vrms_raw_get(uint16_t * const p_vrms);


/*
 ********************************************************************************************************
 *                                                 MODULE END
 ********************************************************************************************************
 */
#ifdef __cplusplus
}
#endif

/******************************************      End of file        ************************************/

