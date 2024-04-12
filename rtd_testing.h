/******************************************************************************/
/*                                                                            */
/* FILE:        rtd_testing.h                                                 */
/*                                                                            */
/* DESCRIPTION: Definitions needed to implement the firmware for the          */
/*              controller board of the HennyPenny Frontier UHC for McDonalds */
/*                                                                            */
/* AUTHOR(S):   USA Firmware, LLC                                             */
/*                                                                            */
/* DATE:        May 5, 2023                                                   */
/*                                                                            */
/* This is an unpublished work subject to Trade Secret and Copyright          */
/* protection by HennyPenny and USA Firmware, LLC                             */
/*                                                                            */
/* USA Firmware, LLC                                                          */
/* 10060 Brecksville Road Brecksville, OH 44141                               */
/*                                                                            */
/* EDIT HISTORY:                                                              */
/* May 5, 2023 - Initial release                                              */
/*                                                                            */
/******************************************************************************/

#pragma once

#define SERIAL_NUMBER_FILE			"/etc/serialNumber.txt"
#define MODEL_NUMBER_FILE			"/etc/modelNumber.txt"
#define SETPOINT_LOW_LIMIT_FILE		"/etc/setpointLowLimit.txt"
#define SETPOINT_HIGH_LIMIT_FILE	"/etc/setpointHighLimit.txt"

#define	SERIAL_NUMBER_SIZE			64
#define MODEL_NUMBER_SIZE			64
#define SETPOINT_LIMIT_SIZE			8
#define DEFAULT_SERIAL_NUMBER		"0123456789"
#define DEFAULT_MODEL_NUMBER		"HennyPenny Frontier UHC Model 100"
#define DEFAULT_SETPOINT_LOW_LIMIT	65
#define DEFAULT_SETPOINT_HIGH_LIMIT	340

#define FRONTIER_UHC_FIRMWARE_VERSION	"001.001.002"

#define TEMPERATURE_LOOKUP_TABLE_FILE		"/etc/hennyPennyTempData.txt"

// allow for 1 temperature calibration file per RTD
#define TEMPERATURE_LOOKUP_FILENAME_FILE	"/etc/fileName.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD1	"/etc/tempCalibrationTableFilename_RTD1.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD2	"/etc/tempCalibrationTableFilename_RTD2.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD3	"/etc/tempCalibrationTableFilename_RTD3.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD4	"/etc/tempCalibrationTableFilename_RTD4.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD5	"/etc/tempCalibrationTableFilename_RTD5.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD6	"/etc/tempCalibrationTableFilename_RTD6.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD7	"/etc/tempCalibrationTableFilename_RTD7.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD8	"/etc/tempCalibrationTableFilename_RTD8.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD9	"/etc/tempCalibrationTableFilename_RTD9.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD10	"/etc/tempCalibrationTableFilename_RTD10.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD11	"/etc/tempCalibrationTableFilename_RTD11.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD12	"/etc/tempCalibrationTableFilename_RTD12.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD_HEATSINK	"/etc/tempCalibrationTableFilename_RTD_HEATSINK.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD_BOARD		"/etc/tempCalibrationTableFilename_RTD14.txt"

#define FIRST_TEMPERATURE_ENTRY				32
#define LAST_TEMPERATURE_ENTRY				350
#define TEMP_TABLE_NUM_ENTRIES				((350 - 32) + 1)	/* first row to last row, inclusive */
#define CARRIAGE_RETURN_CHARACTER			0x0D
#define LINEFEED_CHARACTER					0x0A

#define FUNCTION_SUCCESS			0
#define FUNCTION_FAILURE			1

#define COMMAND_LINE_BUFFER_SIZE	1024
#define DEAULT_MESSAGE_BUFFER_SIZE	1024
#define FIRMWARE_UPDATE_BUFFER_SIZE	8192
#define MAX_FILE_PATH				1024
#define IP_STRING_SIZE				64
#define ADC_READ_BUFFER_SIZE		16
#define BIT_READ_BUFFER_SIZE		4

// ******************* Heater specific definitions *******************
#define NUM_HEATERS					12
#define HEATER_ON					1
#define HEATER_OFF					0
#define HEATER_ON_STRING			"1"
#define HEATER_OFF_STRING			"0"
#define HEATER_POSITION_BOTTOM		1
#define HEATER_POSITION_TOP			2
#define MAX_HEATERS_ON				8

#define ADC_RAW_COUNTS_OPEN				0x0000
#define ADC_RAW_COUNTS_SHORTED			0xFFFF

#define MINIMUM_SETPOINT_TEMPERATURE	100
#define MAXIMUM_SETPOINT_TEMPERATURE	340

#define OVERTEMP_DELTA_LIMIT_DEGREES	20
#define OVERTEMP_DELTA_LIMIT_SECONDS	30
#define UNDERTEMP_DELTA_LIMIT_DEGREES	20
#define UNDERTEMP_DELTA_LIMIT_SECONDS	30


// indices into the heaterInfo array
#define SLOT1_TOP_HEATER_INDEX      0
#define SLOT1_BOTTOM_HEATER_INDEX   1
#define SLOT2_TOP_HEATER_INDEX      2
#define SLOT2_BOTTOM_HEATER_INDEX   3
#define SLOT3_TOP_HEATER_INDEX      4
#define SLOT3_BOTTOM_HEATER_INDEX   5
#define SLOT4_TOP_HEATER_INDEX      6
#define SLOT4_BOTTOM_HEATER_INDEX   7
#define SLOT5_TOP_HEATER_INDEX      8
#define SLOT5_BOTTOM_HEATER_INDEX   9
#define SLOT6_TOP_HEATER_INDEX      10
#define SLOT6_BOTTOM_HEATER_INDEX   11


// Heaters are defined in the schematic:
// top most heater is heater 1
// bottom heater in the top most slot is heater 2
// and so on until
// top heater in the bottom most slot is heater 11
// bottom heater of bottom slot is heater 12
#define HEATER1_VALUE	"/sys/class/gpio/gpio37/value"      /* P8_22 GPIO_37 */
#define HEATER2_VALUE	"/sys/class/gpio/gpio33/value"		/* P8_24 GPIO_33 */
#define HEATER3_VALUE	"/sys/class/gpio/gpio61/value"		/* P8_26 GPIO_61 */
#define HEATER4_VALUE	"/sys/class/gpio/gpio88/value"		/* P8_28 GPIO_88 */
#define HEATER5_VALUE	"/sys/class/gpio/gpio89/value"		/* P8_30 GPIO_89 */
#define HEATER6_VALUE	"/sys/class/gpio/gpio81/value"		/* P8_34 GPIO_81 */
#define HEATER7_VALUE	"/sys/class/gpio/gpio80/value"		/* P8_36 GPIO_80 */
#define HEATER8_VALUE	"/sys/class/gpio/gpio79/value"		/* P8_38 GPIO_79 */
#define HEATER9_VALUE	"/sys/class/gpio/gpio77/value"		/* P8_40 GPIO_77 */
#define HEATER10_VALUE	"/sys/class/gpio/gpio75/value"		/* P8_42 GPIO_75 */
#define HEATER11_VALUE	"/sys/class/gpio/gpio73/value"		/* P8_44 GPIO_73 */
#define HEATER12_VALUE	"/sys/class/gpio/gpio71/value"		/* P8_46 GPIO_71 */

#define SLOT1_TOP_HEATER			HEATER1_VALUE
#define SLOT1_BOTTOM_HEATER			HEATER2_VALUE
#define SLOT2_TOP_HEATER			HEATER3_VALUE
#define SLOT2_BOTTOM_HEATER			HEATER4_VALUE
#define SLOT3_TOP_HEATER			HEATER5_VALUE
#define SLOT3_BOTTOM_HEATER			HEATER6_VALUE
#define SLOT4_TOP_HEATER			HEATER7_VALUE
#define SLOT4_BOTTOM_HEATER			HEATER8_VALUE
#define SLOT5_TOP_HEATER			HEATER9_VALUE
#define SLOT5_BOTTOM_HEATER			HEATER10_VALUE
#define SLOT6_TOP_HEATER			HEATER11_VALUE
#define SLOT6_BOTTOM_HEATER			HEATER12_VALUE

#define TOTAL_SLOTS 6
#define SLOT1   1
#define SLOT2   2
#define SLOT3   3
#define SLOT4   4
#define SLOT5   5
#define SLOT6   6

#define HEATER_DISABLED				0
#define HEATER_ENABLED				1

#define MAX_NEGATIVE_DELTA_TEMP		-350

#define HEATER_GPIO_INIT_SCRIPT		"/usr/bin/setupHeaterGPIOs.sh"

#define DEFAULT_SETPOINT			175
#define DEFAULT_CURRENT_TEMP		72
#define DEFAULT_HEATSINK_TEMP_COUNT	3457
#define HEATSINK_RTD_INDEX			12

#define TEMPERATURE_LOOKUP_TABLE_FILE		"/etc/hennyPennyTempData.txt"
#define TEMPERATURE_LOOKUP_FILENAME_FILE	"/etc/fileName.txt"
#define FIRST_TEMPERATURE_ENTRY				32
#define LAST_TEMPERATURE_ENTRY				350
#define TEMP_TABLE_NUM_ENTRIES				((350 - 32) + 1)	/* first row to last row, inclusive */
#define CARRIAGE_RETURN_CHARACTER			0x0D
#define LINEFEED_CHARACTER					0x0A


typedef struct
{
	uint16_t degreesF;
	uint16_t adc_raw_counts;
} TEMP_LOOKUP_TABLE;

typedef struct
{
	int32_t delta_temp;
	uint8_t heater_index;
} TEMP_DELTAS;




// ******************* Fan related definitions *******************
#define NUM_FANS			2
#define FAN_ON				1
#define FAN_OFF				0
#define FAN_ON_STRING		"1"
#define FAN_OFF_STRING		"0"

// indices into the fanInfo array
#define FAN1_INDEX			0
#define FAN2_INDEX			1

#define FAN1_ON_VALUE		"/sys/class/gpio/gpio72/value"      /* P8_43 GPIO2_8 */
#define FAN2_ON_VALUE		"/sys/class/gpio/gpio74/value"      /* P8_41 GPIO2_10 */
#define FAN1_TACH_VALUE		"/sys/class/gpio/gpio76/value"		/* P8_37 GPIO2_12 */
#define FAN2_TACH_VALUE		"/sys/class/gpio/gpio78/value"		/* P8_39 GPIO2_14 */

#define FAN_GPIO_INIT_SCRIPT		"/usr/bin/setupFanGPIOs.sh"

typedef struct
{
	int fd_on;						// file descriptor to the "value" file - after init, always kept open for read
	int fd_tach;					// file descriptor to the "value" file - after init, always kept open for read
	char ON_GPIO_PATH[64];			// string representing the file path for SYSFS access
	char TACH_GPIO_PATH[64];		// string representing the file path for SYSFS access
	uint8_t fan_on;				    // 1 = on, 0 = off
	int tach_value;		    		// non-zero means it's actually rotating
} FAN_GPIO_SYSFS_INFO;




// Board revision definitions
#define BOARD_ID0_VALUE		"/sys/class/gpio/gpio9/value"		/* P8_33 GPIO0_9 */
#define BOARD_ID1_VALUE		"/sys/class/gpio/gpio8/value"		/* P8_35 GPIO0_8 */
#define BOARD_ID2_VALUE		"/sys/class/gpio/gpio10/value"		/* P8_31 GPIO0_10 */

#define BOARD_ID_GPIO_INIT_SCRIPT		"/usr/bin/setupIDGPIOs.sh"


// ******************* RTD related definitions *******************
typedef struct
{
	uint16_t rtd_number;			// According to the schematic, RTDs 1 - 12 are the slot heater RTDs, 13 is the heatsink
	uint16_t analog_input_channel; 	// either 0 or 1, corresponding to AIN0 or AIN1
	uint16_t mux_number;			// either 0 or 1
	uint16_t mux_channel;			// somewhere between 1 and 9
	uint16_t gain;					// the PGA117 uses scope gain 3 bits
	uint16_t value;				    // value read from the input, raw counts
	int fd;							// file descriptor to read the analog input value from
	char temp_data_filename[MAX_FILE_PATH];	// filename containing the filename of the temperature data file
	TEMP_LOOKUP_TABLE temp_lookup_table[TEMP_TABLE_NUM_ENTRIES];	// each RTD has its own lookup table for "calibration" purposes.
} RTD_MUX_MAPPING;



// the two PGA117s are tied to AIN0 and AIN1, so we only have to read from the two analog inputs
// however, since they are muxes, we read AIN0 for multiple RTDs, switching the mux in between channels
// and the same thing for AIN1
#define NUM_ADC_CHANNELS	2
#define NUM_RTDS_ON_AIN0	6
#define NUM_RTDS_ON_AIN1	7
//#define ADC_FILE_PATH		"/sys/devices/platform/ocp/44e0d000.tscadc/TI-am335x-adc.0.auto/iio:device0/in_voltage%d_raw"
#define ADC_FILE_PATH		"/sys/bus/iio/devices/iio:device0/in_voltage%d_raw"

// There are 13 RTDs.  12 for the 12 heaters, and one for the Triac heatsink
// The first 12 values are for heaters 1 - 12, and the last one is for the heatsink
#define NUM_RTDs			14
#define HEATSINK_RTD_INDEX	12
#define AMBIENT_TEMP_RTD_INDEX 13

#define AIN0_CHANNEL		0
#define AIN1_CHANNEL		1

#define MUX_NUMBER_ZERO		0
#define MUX_NUMBER_ONE		1

// these definitions are in case the mapping in the wiring harness doesn't match the expected order
#define RTD_NUMBER_1		1
#define RTD_NUMBER_2		2
#define RTD_NUMBER_3		3
#define RTD_NUMBER_4		4
#define RTD_NUMBER_5		5
#define RTD_NUMBER_6		6
#define RTD_NUMBER_7		7
#define RTD_NUMBER_8		8
#define RTD_NUMBER_9		9
#define RTD_NUMBER_10		10
#define RTD_NUMBER_11		11
#define RTD_NUMBER_12		12
#define RTD_NUMBER_13		13 /* heatsink */
#define RTD_NUMBER_14      14 /* ambient temp startnig with HW A02 rev boards */

// Firmware update
#define DPKG_INSTALL_OVERWRITE_COMMAND	"dpkg -i --force-overwrite %s/%s"
#define DEB_FILE_EXTENSION				".deb"
#define TEMP_DIRECTORY					"/tmp"
#define SSH_KEYSCAN						"ssh-keyscan -H %s >> ~/.ssh/known_hosts"
#define SCP_UPDATE_FILES				"sshpass -p \"%s\" scp -r %s@%s:%s /tmp/."
#define SCP_PART1						"sshpass -p \"%s\" scp -r "
#define SCP_PART2						"%s"
#define SCP_AT							"@"

// Power Meter SPI device
#define POWER_METER_SPI_DEVICE			"/dev/spidev1.0"
#define POWER_METER_DEFAULT_SPI_SPEED	15625U

// Analog Mux SPI devices PGA117 daisychained
#define ANALOG_MUX_SPI_DEVICE			"/dev/spidev0.0"
#define ANALOG_MUX_SPI_SPEED			1000000

