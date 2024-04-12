/******************************************************************************/
/*                                                                            */
/* FILE:        frontier_uhc.h                                                */
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

#define SERIAL_NUMBER_FILE          "/etc/serialNumber.txt"
#define MODEL_NUMBER_FILE           "/etc/modelNumber.txt"
#define SETPOINT_LOW_LIMIT_FILE     "/etc/setpointLowLimit.txt"
#define SETPOINT_HIGH_LIMIT_FILE    "/etc/setpointHighLimit.txt"
#define SOFT_SHUTDOWN_FILE          "/etc/softShutdown"
#define TOUCH_SOFT_SHUTDOWN_FILE    "touch /etc/softShutdown"
#define RM_SOFT_SHUTDOWN_FILE       "rm /etc/softShutdown"
#define COPY_SYSLOG_NG_FILE         "cp /usr/bin/syslog-ng /etc/logrotate.d/syslog-ng"
#define COPY_LOGROTATE_FILE         "cp /usr/bin/logrotate.conf /etc/logrotate.conf"
#define FIRMWARE_VERSION_FILE       "/etc/frontier-uhc.version"
#define FRONTIER_UHC_PACKAGE_FILENAME_ROOT "frontier-uhc"

// 30 day logging SD card device and mount point
#define SD_CARD_MOUNT_POINT         "/mnt/SD"
#define SD_CARD_DEVICE              "/dev/mmcblk0p1"
// mount -t vfat /dev/mmcblk0p1 /mnt/SD
#define SD_CARD_MOUNT_COMMAND       "mount -t vfat %s %s"
// mkdir /mnt/SD
#define SD_CARD_MKDIR_MOUNT_POINT   "mkdir %s"
#define CHECK_SD_CARD_EXISTS        "ls -l %s > %s"
// blkid | grep mmcblk0p1 > /tmp/blah.txt
#define BLKID_TXT_FILE              "/tmp/blkid.txt"
#define CHECK_SD_CARD_VIA_BLKID     "blkid | grep %s > %s"

#define ETHERNET_UP_DOWN_FILE      "/tmp/ethernetUpDown.txt"
#define ETHERNET_UP_DOWN_COMMAND   "ip link show eth0 | grep \"state UP\" > /tmp/ethernetUpDown.txt"

#define SHUTDOWN_NOW_COMMAND        "shutdown now"
#define REBOOT_COMMAND              "shutdown -r now"

#define FORCE_LOGROTATE_COMMAND     "/usr/sbin/logrotate /etc/logrotate.conf &"

#define  SERIAL_NUMBER_SIZE         64
#define MODEL_NUMBER_SIZE           64
#define SETPOINT_LIMIT_SIZE         8
#define DEFAULT_SERIAL_NUMBER       "0123456789"
#define DEFAULT_MODEL_NUMBER        "HennyPenny Frontier UHC Model 600"
#define DEFAULT_SETPOINT_LOW_LIMIT  150
#define DEFAULT_SETPOINT_HIGH_LIMIT 215

#define FRONTIER_UHC_FIRMWARE_VERSION  "0.9.021"
#define CONTROLLER_MANIFEST_FILENAME	"/tmp/controller.manifest"
#define LINE_BUFFER_LENGTH             1024
#define SHA_STRING_LENGTH              256
#define FIRMWARE_VERSION_STRING_LENGTH 256
#define THIS_FILE_FILENAME_LENGTH      256
#define SHA_OUTPUT_FILE                "/tmp/sha.out"
#define SHA256_COMMAND                 "sha256sum /tmp/%s > %s"
#define MANIFEST_FILE_ITEMS_PER_LINE   3


#define FUNCTION_SUCCESS            0
#define FUNCTION_FAILURE            1

#define COMMAND_LINE_BUFFER_SIZE    1024
#define DEFAULT_MESSAGE_BUFFER_SIZE 1024
#define DEFAULT_CSS_MESSAGE_BUFFER_SIZE 2048
#define DEFAULT_RTD_MESSAGE_BUFFER_SIZE 2048
#define FIRMWARE_UPDATE_BUFFER_SIZE 8192
#define MAX_FILE_PATH               1024
#define IP_STRING_SIZE              64
#define ADC_READ_BUFFER_SIZE        16
#define BIT_READ_BUFFER_SIZE        4
#define ERROR_CODE_LENGTH           64
#define FIRMWARE_UPDATE_RESULT_BUFFER_SIZE  32767

#define STARTUP_TEMP_DELTA_FOR_COMPLETE      5     /* we report at temp when we reach (setpoint - STARTUP_TEMP_DELTA_FOR_COMPLETE) */
#define SETPOINT_RANGE_PLUS_MINUS            5       /* legal temp range is +/- 5F from setpoint */
#define MAX_STARTUP_REACH_SETPOINT_TIME      3000    /* startup time 35 minutes RELAXED TO 50 MINUTES FOR ALPHA */
#define ONE_MINUTE_IN_SECONDS                60
#define TWO_MINUTES_IN_SECONDS               120
#define THREE_MINUTES_IN_SECONDS             180
#define ONE_HOUR_IN_SECONDS                  3600
#define THREE_SECONDS                        3
#define GUI_NO_COMMUNICATION_TIME_LIMIT      THREE_MINUTES_IN_SECONDS
#define ETHERNET_NO_COMMUNICATION_TIME_LIMIT THREE_MINUTES_IN_SECONDS
#define PROC_UPTIME_FILE                     "/proc/uptime"
#define PROC_UPTIME_BUFFER_SIZE              64

#define ONE_SECOND_IN_MICROSECONDS        1000000    /* One million microseconds = one second */
#define ONE_MILLISECOND_IN_MICROSECONDS   1000       /* One thousand microseconds = one millisecond */
#define TEN_MILLISECONDS_IN_MICROSECONDS  10000      /* Ten thousand microseconds = one millisecond */
#define MINIMUM_THREAD_SLEEP_TIME         TEN_MILLISECONDS_IN_MICROSECONDS

// defined but not used GPIOs
#define UNUSED_GPIO_INIT_SCRIPT              "/usr/bin/setupUnusedGPIOs.sh"

// ******************* Heater specific definitions *******************
#define NUM_HEATERS                 12
#define HEATER_ON                   1
#define HEATER_OFF                  0
#define HEATER_ON_STRING            "1"
#define HEATER_OFF_STRING           "0"
#define HEATER_POSITION_BOTTOM      1
#define HEATER_POSITION_TOP         2
#define MAX_HEATERS_ON              8
#define VAC220_ON_STRING            "1"
#define VAC220_OFF_STRING           "0"
#define ADC_RAW_COUNTS_OPEN            0x0000
#define ADC_RAW_COUNTS_SHORTED         0x0FFF
#define MAX_CONSECUTIVE_SECONDS_ERROR  3        /* error condition must happen this many times in a row */

#define MINIMUM_SETPOINT_TEMPERATURE   100
#define MAXIMUM_SETPOINT_TEMPERATURE   340

#define OVERTEMP_DELTA_LIMIT_DEGREES   11
#define OVERTEMP_DELTA_LIMIT_SECONDS   900      /* 15 minutes in seconds = 900 */
#define UNDERTEMP_DELTA_LIMIT_DEGREES  11
#define UNDERTEMP_DELTA_LIMIT_SECONDS  900      /* 15 minutes in seconds = 900 */


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
#define HEATER1_VALUE      "/sys/class/gpio/gpio47/value"      /* P8_15 GPIO_47 */
#define HEATER2_VALUE      "/sys/class/gpio/gpio27/value"      /* P8_17 GPIO_27 */
#define HEATER3_VALUE      "/sys/class/gpio/gpio61/value"      /* P8_26 GPIO_61 */
#define HEATER4_VALUE      "/sys/class/gpio/gpio88/value"      /* P8_28 GPIO_88 */
#define HEATER5_VALUE      "/sys/class/gpio/gpio89/value"      /* P8_30 GPIO_89 */
#define HEATER6_VALUE      "/sys/class/gpio/gpio81/value"      /* P8_34 GPIO_81 */
#define HEATER7_VALUE      "/sys/class/gpio/gpio80/value"      /* P8_36 GPIO_80 */
#define HEATER8_VALUE      "/sys/class/gpio/gpio79/value"      /* P8_38 GPIO_79 */
#define HEATER9_VALUE      "/sys/class/gpio/gpio77/value"      /* P8_40 GPIO_77 */
#define HEATER10_VALUE     "/sys/class/gpio/gpio75/value"      /* P8_42 GPIO_75 */
#define HEATER11_VALUE     "/sys/class/gpio/gpio70/value"      /* P8_45 GPIO_70 */
#define HEATER12_VALUE     "/sys/class/gpio/gpio71/value"      /* P8_46 GPIO_71 */

#define HEATER_220VAC_RELAY   "/sys/class/gpio/gpio22/value"      /* P8_19 GPIO_22 */

#define SLOT1_TOP_HEATER         HEATER1_VALUE
#define SLOT1_BOTTOM_HEATER      HEATER2_VALUE
#define SLOT2_TOP_HEATER         HEATER3_VALUE
#define SLOT2_BOTTOM_HEATER      HEATER4_VALUE
#define SLOT3_TOP_HEATER         HEATER5_VALUE
#define SLOT3_BOTTOM_HEATER      HEATER6_VALUE
#define SLOT4_TOP_HEATER         HEATER7_VALUE
#define SLOT4_BOTTOM_HEATER      HEATER8_VALUE
#define SLOT5_TOP_HEATER         HEATER9_VALUE
#define SLOT5_BOTTOM_HEATER      HEATER10_VALUE
#define SLOT6_TOP_HEATER         HEATER11_VALUE
#define SLOT6_BOTTOM_HEATER      HEATER12_VALUE

#define TOTAL_SLOTS 6
#define SLOT1   1
#define SLOT2   2
#define SLOT3   3
#define SLOT4   4
#define SLOT5   5
#define SLOT6   6

#define HEATER_DISABLED          0
#define HEATER_ENABLED           1

#define MAX_NEGATIVE_DELTA_TEMP  -350

#define HEATER_GPIO_INIT_SCRIPT  "/usr/bin/setupHeaterGPIOs.sh"

#define MAX_HEATER_DATA_FILES             5
#define HENNYPENNY_HEATER_DATA_FILES      "/var/log/HennyPenny/heaterData*.csv"
#define HENNYPENNY_LOG_DIRECTORY          "/var/log/HennyPenny/"
#define HEATER_DATA_TARGET_FILE           "/var/log/HennyPenny/heaterData%d.csv"
#define HEATER_DATA_CSV_FILE              "/tmp/heaterData.csv"
#define HEATER_DATA_TRIGGER_FILE          "/etc/writeHeaterCSVFile"
#define GET_NEWEST_LOG_FILE               "ls -Art /var/log/HennyPenny/heaterData* | tail -n 1 > %s"
#define GET_OLDEST_LOG_FILE               "ls -At /var/log/HennyPenny/heaterData* | tail -n 1 > %s"
#define GET_FILE_COUNT                    "ls -l %s | egrep -c '^-' > %s"
#define MOVE_HEATER_DATA_TO_LOG_DIRECTORY "mv %s %s"
#define FILE_COUNT_FILE                   "/tmp/csvFileCount.txt"
#define OLDEST_FILENAME_FILE              "/tmp/oldestFile.txt"
#define NEWEST_FILENAME_FILE              "/tmp/newestFile.txt"
#define LOG_DIRECTORY_PERMISSIONS         0755
#define TOUCH_FILE                        "touch %s"

#define SD_CARD_LOG_TOP          SD_CARD_MOUNT_POINT  "/log"
#define SD_CARD_LOG_DIRECTORY    SD_CARD_LOG_TOP      "/HennyPenny/"
#define EESLOGFILENAMETEMPLATE   "%s/%4d%02d%02dControl.csv"
#define EESLOGINTERVAL_SEC       3
#define LOGEVENT_TIMEOUT_MS      100
#define LOGERROR_LOCATION_SIZE   65
#define LOGERROR_DESCR_SIZE      129
#define TIME_STR_SIZE            26
#define UTC_OFFSET_SIZE          8
#define RECENT_ERRORS_FILENAME   "recent_errors.log"
#define RECENT_ERRORS_FILE       SD_CARD_LOG_DIRECTORY RECENT_ERRORS_FILENAME
#define RECENT_ERRORS_TEMP_FILE  "/tmp/" RECENT_ERRORS_FILENAME
#define SD_CARD_FSCK_CMD         "fsck -t vfat " SD_CARD_MOUNT_POINT
#define SD_CARD_REMOUNT_CMD      "mount -o remount,rw " SD_CARD_MOUNT_POINT

#define SET_TIMEZONE_COMMAND              "timedatectl set-timezone %s"

#define DEFAULT_SETPOINT                  165
#define DEFAULT_CURRENT_TEMP              72
#define DEFAULT_HEATSINK_TEMP_COUNT       3457
#define CLEANING_MODE_SETPOINT            120
#define DEFAULT_ECO_MODE_SETPOINT         100

#define TEMPERATURE_LOOKUP_TABLE_FILE     "/etc/hennyPennyTempData.txt"

// allow for 1 temperature calibration file per RTD
#define TEMPERATURE_LOOKUP_FILENAME_FILE            "/etc/fileName.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD1            "/etc/tempCalibrationTableFilename_RTD1.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD2            "/etc/tempCalibrationTableFilename_RTD2.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD3            "/etc/tempCalibrationTableFilename_RTD3.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD4            "/etc/tempCalibrationTableFilename_RTD4.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD5            "/etc/tempCalibrationTableFilename_RTD5.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD6            "/etc/tempCalibrationTableFilename_RTD6.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD7            "/etc/tempCalibrationTableFilename_RTD7.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD8            "/etc/tempCalibrationTableFilename_RTD8.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD9            "/etc/tempCalibrationTableFilename_RTD9.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD10           "/etc/tempCalibrationTableFilename_RTD10.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD11           "/etc/tempCalibrationTableFilename_RTD11.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD12           "/etc/tempCalibrationTableFilename_RTD12.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD_HEATSINK    "/etc/tempCalibrationTableFilename_RTD_HEATSINK.txt"
#define TEMPERATURE_LOOKUP_FILENAME_RTD_BOARD       "/etc/tempCalibrationTableFilename_RTD_BOARD.txt"

#define FIRST_TEMPERATURE_ENTRY           32
#define LAST_TEMPERATURE_ENTRY            350
#define TEMP_TABLE_NUM_ENTRIES            ((350 - 32) + 1)  /* first row to last row, inclusive */
#define CARRIAGE_RETURN_CHARACTER         0x0D
#define LINEFEED_CHARACTER                0x0A

typedef struct
{
   int fd;                    // file descriptor to the "value" file - after init, always kept open for read
   char GPIO_PATH[64];           // string representing the file path for SYSFS access
   uint8_t state;             // HEATER_ON or HEATER_OFF
   uint8_t location;          // HEATER_LOCATION_LOWER or HEATER_LOCATION_UPPER
   uint16_t temperature_setpoint;   // degrees Farenheit
   uint16_t cleaning_mode_setpoint; // max 125F
   uint16_t eco_mode_setpoint;      // setpoint for ECO mode
   uint16_t saved_setpoint;      // placeholder for current setpoint when they select cleaning mode
   int16_t current_temperature;  // degrees Farenheit
   google::protobuf::Timestamp start_time;
   google::protobuf::Timestamp end_time;
   bool is_enabled;
   bool is_on;
   bool was_on;
   bool is_overtemp;          // above setpoint OVERTEMP_DELTA_LIMIT_DEGREES for OVERTEMP_DELTA_LIMIT_SECONDS
   bool is_undertemp;            // below setpoint UNDERTEMP_DELTA_LIMIT_DEGREES for UNDERTEMP_DELTA_LIMIT_SECONDS
   bool is_overtemp_CSS;         // once we reach the time limit, set this for the CurrentSystemState message
   bool is_undertemp_CSS;        // once we reach the time limit, set this for the CurrentSystemState message
   bool overtemp_oneshot;
   bool undertemp_oneshot;
   bool eco_mode_on;
   bool setpoint_changed;
   uint32_t seconds_overtemp;
   uint32_t seconds_undertemp;
   uint32_t seconds_on_time;     // number of seconds the heater is on per hour
   int32_t delta_temp;
} HEATER_GPIO_SYSFS_INFO;

#define NUM_TEMP_LOOKUP_TABLE_ENTRIES
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
#define NUM_FANS           2
#define FAN_ON             1
#define FAN_OFF            0
#define FAN_ON_STRING      "1"
#define FAN_OFF_STRING     "0"

// indices into the fanInfo array
#define FAN1_INDEX         0
#define FAN2_INDEX         1

// fan Tach re-read time
#define FAN_TACH_RE_READ_TIME_uS 5000  /* 5mS = 5000 uS */
#define FAN_TACH_RETRY_LIMIT     5     /* try to read it 5 times or until we read a 1 */

#define FAN1_ON_VALUE      "/sys/class/gpio/gpio72/value"      /* P8_43 GPIO2_8  */
#define FAN2_ON_VALUE      "/sys/class/gpio/gpio74/value"      /* P8_41 GPIO2_10 */
#define FAN1_TACH_VALUE    "/sys/class/gpio/gpio66/value"      /* P8_7  GPIO2_2  */
#define FAN2_TACH_VALUE    "/sys/class/gpio/gpio69/value"      /* P8_9  GPIO2_5  */

#define FAN_GPIO_INIT_SCRIPT     "/usr/bin/setupFanGPIOs.sh"

typedef struct
{
   int fd_on;                 // file descriptor to the "value" file - after init, always kept open for read
   int fd_tach;               // file descriptor to the "value" file - after init, always kept open for read
   char ON_GPIO_PATH[64];        // string representing the file path for SYSFS access
   char TACH_GPIO_PATH[64];      // string representing the file path for SYSFS access
   uint8_t fan_on;                // 1 = on, 0 = off
   int tach_value;               // non-zero means it's actually rotating
} FAN_GPIO_SYSFS_INFO;


// Power monitor IC definitions
#define WARN_OUT_PULSE_VALUE  "/sys/class/gpio/gpio44/value"   /* P8_12 GPIO1_12 */
#define LINE_SAG_VOLTS        20.0F    // AC line voltage level that triggers soft power down.
#define STOP_KERNEL_LOG_CMD            "systemctl stop syslog-ng"

#define POWERMON_GPIO_INIT_SCRIPT      "/usr/bin/setupPowerMonGPIOs.sh"


// Board revision definitions
#define BOARD_ID0_VALUE    "/sys/class/gpio/gpio9/value"    /* P8_33 GPIO0_9 */
#define BOARD_ID1_VALUE    "/sys/class/gpio/gpio8/value"    /* P8_35 GPIO0_8 */
#define BOARD_ID2_VALUE    "/sys/class/gpio/gpio10/value"      /* P8_31 GPIO0_10 */

#define BOARD_ID_GPIO_INIT_SCRIPT      "/usr/bin/setupIDGPIOs.sh"


// ******************* RTD related definitions *******************
typedef struct
{
   uint16_t rtd_number;       // According to the schematic, RTDs 1 - 12 are the slot heater RTDs, 13 is the heatsink
   uint16_t analog_input_channel;   // either 0 or 1, corresponding to AIN0 or AIN1
   uint16_t mux_number;       // either 0 or 1
   uint16_t mux_channel;         // somewhere between 1 and 9
   uint16_t gain;             // the PGA117 uses scope gain 3 bits
   uint16_t value;                // value read from the input, raw counts
   bool is_shorted;
   bool is_open;
   bool shorted_oneshot;
   bool open_oneshot;
   uint16_t seconds_shorted;  // number of consecutive seconds RTD reads back shorted
   uint16_t seconds_open;     // number of consecutive seconds RTD reads back open
   int fd;                    // file descriptor to read the analog input value from
   char temp_data_filename[MAX_FILE_PATH];   // filename containing the filename of the temperature data file
   TEMP_LOOKUP_TABLE temp_lookup_table[TEMP_TABLE_NUM_ENTRIES];   // each RTD has its own lookup table for "calibration" purposes.
} RTD_MUX_MAPPING;



// the two PGA117s are tied to AIN0 and AIN1, so we only have to read from the two analog inputs
// however, since they are muxes, we read AIN0 for multiple RTDs, switching the mux in between channels
// and the same thing for AIN1
#define NUM_ADC_CHANNELS   2
#define NUM_RTDS_ON_AIN0   6
#define NUM_RTDS_ON_AIN1   8
#define ADC_FILE_PATH      "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw"
#define MAX_READ_RTD_RETRY_COUNT    4

// There are 13 RTDs.  12 for the 12 heaters, and one for the Triac heatsink
// The first 12 values are for heaters 1 - 12, and the last one is for the heatsink
// Starting with rev A02 hardware, there are 14 RTDs. 1 - 12 are the heaters, 13 is the heatsink,
// and 14 is the new ambient temp sensor (aka PCB or board temp sensor)
#define NUM_RTDs        14
#define AMBIENT_TEMP_RTD_INDEX 13

#define AIN0_CHANNEL    0
#define AIN1_CHANNEL    1

#define MUX_NUMBER_ZERO    0
#define MUX_NUMBER_ONE     1

#define SPI_TRANSACTION_BYTE_COUNT     4 /* 4 bytes = 32 bits of SPI data */

// these definitions are in case the mapping in the wiring harness doesn't match the expected order
#define RTD_NUMBER_1    1
#define RTD_NUMBER_2    2
#define RTD_NUMBER_3    3
#define RTD_NUMBER_4    4
#define RTD_NUMBER_5    5
#define RTD_NUMBER_6    6
#define RTD_NUMBER_7    7
#define RTD_NUMBER_8    8
#define RTD_NUMBER_9    9
#define RTD_NUMBER_10   10
#define RTD_NUMBER_11   11
#define RTD_NUMBER_12   12
#define RTD_NUMBER_13   13   /* heatsink */
#define RTD_NUMBER_14   14   /* ambient temp starting with HW A02 rev boards */

#define HEATSINK_RTD_INDEX                   12
#define HEATSINK_MAX_TEMP                    176      // Triac maximum operational junction temp 176F

#define AMBIENT_MAX_TEMP                    158    // Ambient temp sensor max temp according to Joel Marcum 10/4/2023
#define AMBIENT_OVERTEMP_TIME_LIMIT_SECONDS 900    // generate alarm when ambient is at or above MAX TEMP for TIME LIMIT SECONDS 15 minutes according to Joel Marcum 10/04/2023

// HENNY PENNY ERROR CODES
#define REPORT_ERROR_COUNT                   3     // report each error 3 times before clearing
#define HEATSINK_OVER_TEMP_ERROR_CODE        "E-4B"
#define AMBIENT_OVER_TEMP_ERROR_CODE         "E-4A"
#define SHELF_OVER_TEMP_ERROR_CODE           "E-5"
#define SHELF_UNDER_TEMP_ERROR_CODE          "E-216"
#define TEMP_PROBE_ERROR_CODE                "E-6"
#define TEMP_PROBE_OPEN_ERROR_CODE           "E-6A"
#define TEMP_PROBE_CLOSED_ERROR_CODE         "E-6B"
#define BOTH_GUIS_COMM_LOSS_ERROR_CODE       "E-220"
#define ETHERNET_DOWN_ERROR_CODE             "E-220A"
#define SINGLE_GUI_COMM_LOSS_ERROR           "E-60A"
#define STARTUP_TIME_EXCEEDED_ERROR_CODE     "E-215"
#define FAN_FAILURE_ERROR_CODE               "E-210"
#define POWER_FAIL_DETECTED_ERROR_CODE       "E-200"
#define SD_CARD_MISSING_OR_CORRUPT           "E-200"
#define HARDWARE_FAILURE_ERROR_CODE          "E-225"

// Firmware update
#define DPKG_INSTALL_OVERWRITE_COMMAND    "dpkg -i --force-overwrite %s/%s"
#define DEB_FILE_EXTENSION                ".deb"
#define TEMP_DIRECTORY                    "/tmp"
#define SSH_KEYSCAN                       "ssh-keyscan -v -H %s >> /root/.ssh/known_hosts"
#define RM_KNOWN_HOSTS                    "rm /root/.ssh/known_hosts"
#define SCP_UPDATE_FILES                  "sshpass -p \"%s\" scp %s@%s:%s /tmp/."
#define SCP_PART1                         "sshpass -p \"%s\" scp -r "
#define SCP_PART2                         "%s"
#define SCP_AT                            "@"

// Power Meter SPI device
#define POWER_METER_SPI_DEVICE            "/dev/spidev1.0"
#define POWER_METER_DEFAULT_SPI_SPEED     15625U
#define POWER_METER_MAX_READS             5

// Analog Mux SPI devices PGA117 daisychained
#define ANALOG_MUX_SPI_DEVICE             "/dev/spidev0.0"
#define ANALOG_MUX_SPI_SPEED              1000000

#define SPI_GPIO_INIT_SCRIPT     "/usr/bin/setupSPIGPIOs.sh"

// Syslog stuff
#define SYSLOG_IDENT_STRING      "FRONTIER-UHC"

// debug printf control file
// issue the command "touch /tmp/debug" to turn debug messages on
// issue "rm /tmp/debug" to turn them off
#define DEBUG_PRINTF_FILE           "/tmp/debug"
#define DEBUG_HEATERS_PRINTF_FILE   "/tmp/debugHeaters"
#define DEBUG_CSS_MESSAGE_FILE      "/tmp/debugCSS"
#define DEBUG_CSV_FILE              "/tmp/debugCSV"
