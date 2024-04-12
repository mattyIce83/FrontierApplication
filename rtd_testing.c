/******************************************************************************/
/*                                                                            */
/* FILE:        rtd_testing.c                                                 */
/*                                                                            */
/* DESCRIPTION: Routines to implement tests for the controller board RTDs     */
/*              of the HennyPenny Frontier UHC for McDonalds                  */
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "rtd_testing.h"
#include "PGA117.h"

//#define DEBUG_OUT

// global variables
RTD_MUX_MAPPING rtdMappings[NUM_RTDs];
char temperatureFileName[MAX_FILE_PATH];

char *labels[NUM_RTDs] = {	" Heater 1  Slot 1 Top   ",
							" Heater 2  Slot 1 Bottom",
							" Heater 3  Slot 2 Top   ",
							" Heater 4  Slot 2 Bottom",
							" Heater 5  Slot 3 Top   ",
							" Heater 6  Slot 3 Bottom",
							" Heater 7  Slot 4 Top   ",
							" Heater 8  Slot 4 Bottom",
							" Heater 9  Slot 5 Top   ",
							"Heater 10 Slot 5 Bottom",
							"Heater 11 Slot 6 Top   ",
							"Heater 12 Slot 6 Bottom",
							"Heatsink               ",
                     "Ambient                " };

int fd_analogInput0 = -1;
int fd_analogInput1 = -1;

int fd_spi_bus_0 = -1;

// current draw from the wall outlet for the heaters
float irms = 0.0;

// One of these lookup tables MUST be included or the code won't build
// since this is for testing the Frankenstein original Frymaster cabinet with
// Frymaster heaters and RTDs buth with the USAFW designed controller, use
// the Frymaster lookup table.
#define LOOKUPTABLEHP	1
#ifdef LOOKUPTABLEHP
#include "lookupTableHennyPenny.cpp"
#else
#include "lookupTableFrymaster.cpp"
#endif

#define NUM_TEMP_LOOKUP_ENTRIES (sizeof(tempLookupTable) / sizeof(TEMP_LOOKUP_TABLE))


// ****************** RTD code ******************
int init_PGA117_SPI()
{
	int retval = 0;
	uint32_t spifreqHz = ANALOG_MUX_SPI_SPEED;

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


/*******************************************************************************************/
/*                                                                                         */
/* int initADCData()                                                                       */
/*                                                                                         */
/* Initialize the file descriptors for accessing the analog inputs for the RTD data.       */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initADCData()
{
	// open the files corresponding to the to ADC channels
	// and keep them open the entire time the program is running
	int ret = 0;
 	char fileName[MAX_FILE_PATH];

	memset(fileName, 0, sizeof(fileName));
	sprintf(fileName, ADC_FILE_PATH, AIN0_CHANNEL);
	fd_analogInput0 = open(fileName, O_RDONLY);

	memset(fileName, 0, sizeof(fileName));
	sprintf(fileName, ADC_FILE_PATH, AIN1_CHANNEL);
	fd_analogInput1 = open(fileName, O_RDONLY);

	fd_spi_bus_0 = open(ANALOG_MUX_SPI_DEVICE, O_RDWR);

	if ((fd_spi_bus_0 < 0) || (fd_analogInput0 < 0) || (fd_analogInput1 < 0))
	{
		ret = 1;
	}

	return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initRTDMappings()                                                                   */
/*                                                                                         */
/* Initialize the mappings for accessing the RTD data.                                     */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initRTDMappings()
{
	// this mapping is necessary because it made things easier in
	// the circuit board layout. The RTDs for the heaters are spread around the
	// two muxes, not in a simple linear order.
	// RTD number  analog input number Mux number Mux channel   Gain
	//      1               0               0          5          1
	//      2               0               0          4          1
	//      3               0               0          3          1
	//      4               0               0          2          1
	//      5               0               0          1          1
	//      6               0               0          7          1
	//      7               1               1          5          1
	//      8               1               1          4          1
	//      9               1               1          3          1
	//      10              1               1          2          1
	//      11              1               1          1          1
	//      12              1               1          7          1
	//      13              1               1          8          1

	memset(rtdMappings, 0, sizeof(rtdMappings));

	int i = 0;
	rtdMappings[i].rtd_number				= RTD_NUMBER_1;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH5;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD1);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_2;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH4;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD2);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_3;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH3;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD3);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_4;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH2;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD4);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_5;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH1;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD5);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_6;
	rtdMappings[i].analog_input_channel		= AIN0_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ZERO;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH7;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput0;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD6);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_7;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH5;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD7);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_8;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH4;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD8);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_9;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH3;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD9);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_10;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH2;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD10);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_11;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH1;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD11);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_12;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH7;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= 0;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD12);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_13;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH8;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= DEFAULT_HEATSINK_TEMP_COUNT;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD_HEATSINK);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	i++;
	rtdMappings[i].rtd_number				= RTD_NUMBER_14;
	rtdMappings[i].analog_input_channel		= AIN1_CHANNEL;
	rtdMappings[i].mux_number				= MUX_NUMBER_ONE;
	rtdMappings[i].mux_channel				= PGA117_CHANNEL_CH6;
	rtdMappings[i].gain						= PGA117_GAIN_1;
	rtdMappings[i].value					= DEFAULT_HEATSINK_TEMP_COUNT;
	rtdMappings[i].fd						= fd_analogInput1;
	strcpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD_HEATSINK);
	memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

	return 0;
}


/*******************************************************************************************/
/*                                                                                         */
/* int lookupTempFromRawCounts(int rawCounts, rtdIndex)                                    */
/*                                                                                         */
/* Calculates the temperature in farenheit from the supplied ADC raw counts for the        */
/* specified RTD. This allows individual RTDs to have their own calibration data.          */
/*                                                                                         */
/* Returns: int temp (-1 if out of range)                                                  */
/*                                                                                         */
/*******************************************************************************************/
int lookupTempFromRawCounts(int rawCounts, int rtdIndex)
{
	int temp = -1;
	int i = 0;
	bool found = false;

	if ((rawCounts < rtdMappings[rtdIndex].temp_lookup_table[0].adc_raw_counts) || (rawCounts > rtdMappings[rtdIndex].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-1].adc_raw_counts))
	{
		// out of range
		temp = -1;
	}
	else if (rawCounts == rtdMappings[rtdIndex].temp_lookup_table[0].adc_raw_counts)
	{
		temp = rtdMappings[rtdIndex].temp_lookup_table[0].degreesF;
	}
	else if (rawCounts == rtdMappings[rtdIndex].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-1].adc_raw_counts)
	{
		temp = rtdMappings[rtdIndex].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-1].degreesF;
	}
	else
	{
		for (i = 0; i < NUM_TEMP_LOOKUP_ENTRIES && ! found; i++)
		{
			if ((rawCounts >= rtdMappings[rtdIndex].temp_lookup_table[i].adc_raw_counts) && (rawCounts < rtdMappings[rtdIndex].temp_lookup_table[i+1].adc_raw_counts))
			{
				// we've found the correct value
				found = true;
				temp = rtdMappings[rtdIndex].temp_lookup_table[i].degreesF;
			}
		}
	}
	return temp;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getTemperatureLookupFilename()                                                      */
/*                                                                                         */
/* Read the TEMPERATURE_LOOKUP_FILENAME_FILE, if it exists. Then parse its contents for    */
/* the name of the lookup table file and set it.                                           */
/* if the file doesn't exist, just use the default table.                                  */
/*                                                                                         */
/* Returns: int ret  0 = success, 1 = failure                                              */
/*                                                                                         */
/*******************************************************************************************/
int getTemperatureLookupFilename(int rtdIndex, char *filename)
{
	int ret = FUNCTION_FAILURE;
	char tempFileName[MAX_FILE_PATH];
	memset(tempFileName, 0, sizeof(tempFileName));
	memset(temperatureFileName, 0, sizeof(temperatureFileName));
	strcpy(tempFileName, rtdMappings[rtdIndex].temp_data_filename);
	// see if the file that specifies the name of the temperature lookup file exists
	if (access(rtdMappings[rtdIndex].temp_data_filename, R_OK) == FUNCTION_SUCCESS)
	{
		FILE *fp = NULL;
		fp = fopen(rtdMappings[rtdIndex].temp_data_filename, "r");
		if (fp != NULL)
		{
			// the file does exist
			// read the file name from within that file
			char buf[MAX_FILE_PATH];
			memset(buf, 0, sizeof(buf));
			int bytesRead = fread(buf, 1, sizeof(buf) - 1, fp);
			fclose(fp);
			if (bytesRead > 0)
			{
				// strip off any CRs or LFs
				for (int i = 0; i < strlen(buf); i++)
				{
					if ((buf[i] == CARRIAGE_RETURN_CHARACTER) || (buf[i] == LINEFEED_CHARACTER))
					{
						buf[i] = 0;
					}
				}
				// now see if the target temperature lookup table file exists
				strcpy(tempFileName, buf);
				if (access(tempFileName, R_OK) == FUNCTION_SUCCESS)
				{
					// target file exists
					// use it as the filename
					strcpy(filename, tempFileName);
					ret = FUNCTION_SUCCESS;
				}
				else
				{
					// specified file doesn't exist; use default name
					strcpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE);
				}
			}
			else
			{
				// no filename contained in the file; use default name
				strcpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE);
			}
		}
		else
		{
			// no filename file found; use default name
			strcpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE);
		}
	}
	else
	{
		// no TEMPERATURE_LOOKUP_FILENAME_FILE exists; use default name
		strcpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE);
	}
	return ret;
}

/*******************************************************************************************/
/*                                                                                         */
/* int readTemperatureLookupFiles()                                                        */
/*                                                                                         */
/* Read the temperature Lookup file for the given RTD. It has a fixed format, but the data */
/* inside can change                                                                       */
/* if the file doesn't exist, just use the default table.                                  */
/*                                                                                         */
/* Returns: int ret  0 = success, 1 = failure                                              */
/*                                                                                         */
/*******************************************************************************************/
int readTemperatureLookupFiles()
{
	int ret = FUNCTION_SUCCESS;
	FILE *fp = NULL;
	TEMP_LOOKUP_TABLE tlt[NUM_TEMP_LOOKUP_ENTRIES];
	uint16_t numRows = 0;
	int i = 0;
	int j = 0;
	char calibratedTempLookupTableFilename[MAX_FILE_PATH];

	for (i = 0; i < NUM_RTDs; i++)
	{
		memset(tlt, 0, sizeof(tlt));
		memset(calibratedTempLookupTableFilename, 0, sizeof(calibratedTempLookupTableFilename));
		getTemperatureLookupFilename(i, calibratedTempLookupTableFilename);
		fp = fopen(calibratedTempLookupTableFilename, "r");
		if (fp != NULL)
		{
			(void)fscanf(fp, "%hu\n", &numRows);
			if (NUM_TEMP_LOOKUP_ENTRIES != numRows)
			{
				printf("Row count of %hu doesn't match expected vaue of %u\n", numRows, NUM_TEMP_LOOKUP_ENTRIES);
				fclose(fp);
				ret = FUNCTION_FAILURE;
				return ret;
			}

			int count = 0;
			for (j = 0; j < numRows; j++)
			{
				count += fscanf(fp, "%hu %hu\n", &tlt[j].degreesF, &tlt[j].adc_raw_counts);
//				printf("%d  %d\n", tlt[j].degreesF, tlt[j].adc_raw_counts);
			}
			fclose(fp);
			fp = NULL;

			if (((NUM_TEMP_LOOKUP_ENTRIES * 2) == count) && (FIRST_TEMPERATURE_ENTRY == tlt[0].degreesF) && (LAST_TEMPERATURE_ENTRY == tlt[NUM_TEMP_LOOKUP_ENTRIES - 1].degreesF))
			{
//				printf("Temperature data file %s OK\n", calibratedTempLookupTableFilename);
				memcpy(rtdMappings[i].temp_lookup_table, tlt, sizeof(rtdMappings[i].temp_lookup_table));
			}
			else
			{
				printf("Temperature file %s isn't a valid temp file!\n", calibratedTempLookupTableFilename);
			}
		}
		else
		{
			printf("Error reading Temperature data file %s!\n", calibratedTempLookupTableFilename);
		}
	}
	return ret;
}


int issueSPICommands(int rtd_index)
{
	int ret = 0;
	int retVal = 0;
	uint16_t mux1Command = 0;
	uint16_t mux2Command = 0;
	char buf[4];

	// Select channel 0 (GND) with unity gain as the output for the Mux we're not trying to read
	if (rtd_index < NUM_RTDS_ON_AIN0)
	{
		/* reading Mux 1 */
		/* Format the commands */
      mux1Command = (uint16_t)(PGA117_CMD_WRITE |
         (rtdMappings[rtd_index].mux_channel << PGA117_CHANNEL_SHIFT) |
         (rtdMappings[rtd_index].gain << PGA117_GAIN_SHIFT));

      mux2Command = (uint16_t)(PGA117_DCCMD_WRITE |
         (PGA117_CHANNEL_CH0 << PGA117_CHANNEL_SHIFT) |
         (PGA117_GAIN_1 << PGA117_GAIN_SHIFT));
	}
	else
	{
		/* reading Mux 2 */
		/* Format the commands */
      mux1Command = (uint16_t)(PGA117_CMD_WRITE |
         (PGA117_CHANNEL_CH0 << PGA117_CHANNEL_SHIFT) |
         (PGA117_GAIN_1 << PGA117_GAIN_SHIFT));

      mux2Command = (uint16_t)(PGA117_DCCMD_WRITE |
         (rtdMappings[rtd_index].mux_channel << PGA117_CHANNEL_SHIFT) |
         (rtdMappings[rtd_index].gain << PGA117_GAIN_SHIFT));
	}


	buf[0] = (mux2Command >> 8) & 0xFF;
	buf[1] = mux2Command & 0xFF;
	buf[2] = (mux1Command >> 8) & 0xFF;
	buf[3] = mux1Command & 0xFF;
	// write the commands
	ret = write(fd_spi_bus_0, buf, sizeof(buf));
	if (ret != 4)
	{
		retVal = 1;
	}
	usleep(10);
	return retVal;
}


/*******************************************************************************************/
/*                                                                                         */
/* int readADCChannel(int rtd_index)                                                       */
/*                                                                                         */
/* Reads the ADC value for the specified RTD.                                              */
/*                                                                                         */
/* Returns: int rawCount                                                                   */
/*                                                                                         */
/*******************************************************************************************/
int readADCChannel(int rtd_index)
{
	int rawCount = -1;
	char buf[ADC_READ_BUFFER_SIZE];

 	if (rtdMappings[rtd_index].fd != -1)
	{
		// issuing the SPI commands routes the RTD data through the PGA to either AIN0 or AIN1 ADC
		// which is actually the value we need (raw ADC count)
		(void)issueSPICommands(rtd_index);
		usleep(10000);
		// each time we read the analog value, we need to rewind the file descriptor back to the beginning of the file
		// or we'll never read another value
		lseek(rtdMappings[rtd_index].fd, 0, SEEK_SET);
		memset(buf, 0, sizeof(buf));
		// get the raw ADC count from either AIN0 or AIN1
 		int readLen = read(rtdMappings[rtd_index].fd, buf, sizeof(buf));
 		if (readLen > 1)
		{
			// more than EOL; make sure it's NULL terminated
 			buf[readLen-1] = '\0';
			rawCount = atoi(buf);

 		}
 	}

 	return rawCount;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initializeHardware()                                                                */
/*                                                                                         */
/* Initialize all of the controller hardware.                                              */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initializeHardware()
{
	int ret = 0;
	system("cd /sys/class/gpio/gpio5");
	system("echo out > direction");
	system("echo 0 > value");
	ret = init_PGA117_SPI();
	return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initializeDataStructures()                                                          */
/*                                                                                         */
/* Initialize all of the controller data structures.                                       */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int initializeDataStructures()
{
   int ret = 0;
	ret |= initADCData();
	ret |= initRTDMappings();
	ret |= readTemperatureLookupFiles();
	return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int main(int argc, char *argv[])                                                        */
/*                                                                                         */
/* Call the initialization functions and the test code.                                    */
/*                                                                                         */
/*******************************************************************************************/
int main(int argc, char *argv[])
{

	int16_t current_temperature = 0;

#ifdef DEBUG_OUT
	int ret = 0;
#endif

#ifdef DEBUG_OUT
	ret = initializeDataStructures();
#else
   (void)initializeDataStructures();
#endif

#ifdef DEBUG_OUT
	if (0 == ret)
	{
		printf("Board data structure initialization successful\n");
	}
	else
	{
		printf("Board data structure initialization failed ret = %d\n", ret);
	}
#endif

	// ASSUME THAT THE GPIOs ARE ALREADY SET UP, OR THIS WILL ALTER THE CURRENT STATE!
#ifdef DEBUG_OUT
	ret = initializeHardware();
	if (0 == ret)
	{
		printf("Board hardware initialization successful\n");
	}
	else
	{
		printf("Board hardware initialization failed ret = %d\n", ret);
	}
#endif

    printf(" NUMBER        LOCATION              VOLTAGE   ADC COUNTS  TEMPERATURE  SHORTED/OPEN\n");
    printf("======================================================================================\n");
	for (int i = 0; i < NUM_RTDs; i++)
	{
		rtdMappings[i].value = readADCChannel(i);
		current_temperature = lookupTempFromRawCounts(rtdMappings[i].value, i);
#ifdef DEBUG_OUT
      printf("Value read: %d  current temp: %d  340F value:%d \n ", rtdMappings[i].value, current_temperature, rtdMappings[i].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts);
#endif
		char str[16];
		memset(str, 0, sizeof(str));
		// if the counts are above 340 degree counts, call that open
		if (rtdMappings[i].value > rtdMappings[i].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
		{
			strcpy(str, "Open");
		}
		if (rtdMappings[i].value < rtdMappings[i].temp_lookup_table[0].adc_raw_counts)
		{
			strcpy(str, "Shorted");
		}
		printf("RTD [%d] %s     %0.5fV     %4d      %5d F        %s\n", i+1, labels[i], ((float)(rtdMappings[i].value) * 1.8) / 4096.0,  rtdMappings[i].value, current_temperature, str);
	}
    printf("\n\n");

	exit(0);
}
