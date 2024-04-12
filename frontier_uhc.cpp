/******************************************************************************/
/*                                                                            */
/* FILE:        frontier_uhc.cpp                                              */
/*                                                                            */
/* DESCRIPTION: Routines to implement the firmware for the controller board   */
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
#include <pthread.h>
#include <time.h>
#include <poll.h>
#include <ctime>
#include <chrono>
#include <google/protobuf/util/time_util.h>
#include <zmq.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <iostream>
#include <errno.h>
#include <execinfo.h>

#include "frontier_uhc.h"
#include "uhc.pb.h"
#include "uhc_proto.h"
#include "atm90e26.h"
#include "PGA117.h"
#include "safe_queue.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

// global variables
bool initialStartup = true;
bool startupComplete = false;
bool selfTestsOK = false;
void *context = NULL;
void *statusPublisher = NULL;
void *subscriber = NULL;
void *subscriber2 = NULL;
void *timeSyncSubscriber = NULL;
void *timeSyncSubscriber2 = NULL;
void *commandListener = NULL;
void *commandListener2 = NULL;
void *commandResponsePublisher = NULL;
void *commandResponsePublisher2 = NULL;
void *heartBeatListener = NULL;
void *heartBeatListener2 = NULL;
void *firmwareUpdateResponsePublisher = NULL;
void *firmwareUpdateListener = NULL;
void *rtdPublisher = NULL;

char controllerIPAddress[IP_STRING_SIZE];
char guiIPAddress1[IP_STRING_SIZE];
char guiIPAddress2[IP_STRING_SIZE];
char serialNumber[SERIAL_NUMBER_SIZE + 1];
char modelNumber[MODEL_NUMBER_SIZE + 1];
char errorCodeString[ERROR_CODE_LENGTH + 1];
char controllerManifestFilename[MAX_FILE_PATH + 1];
char frontierUHCPackageFilename[MAX_FILE_PATH + 1];


// thread IDs
pthread_t statusPublisherThread_ID;
pthread_t loggerThread_ID;
pthread_t subscriberThread_ID;
pthread_t heartBeatListenerThread_ID;
pthread_t heartBeatListenerThread2_ID;
pthread_t readADCThread_ID;
pthread_t timeSyncSubscriberThread_ID;
pthread_t timeSyncSubscriberThread2_ID;
pthread_t firmwareUpdateListenerThread_ID;
pthread_t commandHandlerThread_ID;
pthread_t commandHandlerThread2_ID;
pthread_t heaterControlThread_ID;
pthread_t softPowerdownThread_ID;
pthread_t firmwareUpdatePackageThread_ID;
pthread_t rtdPublisherThread_ID;

uint16_t controllerBoardRevision = 0;
FAN_GPIO_SYSFS_INFO fanInfo[NUM_FANS];
RTD_MUX_MAPPING rtdMappings[NUM_RTDs];
HEATER_GPIO_SYSFS_INFO heaterInfo[NUM_HEATERS];
int tempDeltas[NUM_HEATERS];

uint16_t setpointLowLimit = DEFAULT_SETPOINT_LOW_LIMIT;
uint16_t setpointHighLimit = DEFAULT_SETPOINT_HIGH_LIMIT;
char setpointLowLimitText[SETPOINT_LIMIT_SIZE];
char setpointHighLimitText[SETPOINT_LIMIT_SIZE];

int fd_analogInput0 = -1;
int fd_analogInput1 = -1;
int fd_spi_bus_0 = -1;

bool shutdownRequested = false;
bool debugPrintf = false;        // turn on debug messages to the console on the fly if /tmp/debug exists
bool debugHeatersPrintf = false;
bool heatsinkOvertemp = false;
bool heatsinkOvertempOneShot = false;
bool ambientOvertemp = false;
bool ambientOvertempOneShot = false;
bool softShutdownCheckOneshot = false;
bool inCleaningMode = false;
bool logDirectoryOk = false;
bool startupMessageReceived = false;
bool timeSyncReceived = false;
bool timeZoneConfigured = false;
bool sdCardExists = false;
bool debugCSS = false;
bool debugCSV = false;
bool ethernetUp = true;
bool previousEthernetUp = true;
bool ethernetErrorOneShot = true;
bool gui1MissingOneShot = true;
bool gui2MissingOneShot = true;
bool bothGuisMissingOneShot = true;
bool manifestFileContainsFrontierUHC = false;
bool processFrontierInstaller = false;
bool powerMonitorBad = false;
bool powerMonitorBadOneShot = true;
bool sigTermReceived = false;
bool regularHeaterAlgorithmOneShot = true;
bool fan1FailureOneShot = true;
bool fan2FailureOneShot = true;
bool fan1CurrentLimitOneShot = true;
bool fan2CurrentLimitOneShot = true;
bool alreadyClosing = false;
bool maxStartupTimeExceededOneShot = true;
bool fsckAttempted = false;
bool nsoMode = false;
bool demoMode = false;
bool loggingIsEventDriven = false;

uint32_t errorReportCount = 0;
uint32_t heatsinkTemp = 0;
uint32_t ambientTemp = 0;
uint32_t heatsinkOvertempSeconds = 0;
uint32_t ambientOvertempSeconds = 0;
uint32_t startupTimeSeconds = 0;
uint32_t irms_retries = 0;
uint32_t vrms_retries = 0;
uint32_t loggingLinesWritten = 0;
uint32_t ethernetDownTimeSeconds = 0;
uint32_t manifestFileLineCount = 0;
uint32_t numThreadsRunning = 0;
uint32_t loggingPeriodSeconds = DEFAULT_LOGGING_PERIOD_SECONDS;

enum SD_CARD_TYPE sdCardType = UNKNOWN;

uint32_t lastTimeGUI1Heard = 0;
uint32_t lastTimeGUI2Heard = 0;
long systemUptime = 0;

FILE *csvFile = NULL;
bool debugHeatersWriteCSVFile = false;
struct timeval csvStartTime;

// current draw from the wall outlet for the heaters
float irms = 0.0;
float voltage = 0.0;

uhc::SystemStatus systemStatus = SYSTEM_STATUS_NORMAL;
uhc::AlarmCode alarmCode = ALARM_CODE_NONE;
uhc::SystemCommands lastCommandReceived = SYSTEM_COMMAND_UNKNOWN;

char firmwareUpdateResultBuffer[FIRMWARE_UPDATE_RESULT_BUFFER_SIZE];

// Function prototypes
int enableDisableHeater(int heaterIndex, bool enabled);
int turnHeaterOnOff(int heaterIndex, uhc::HeaterState on);

const char *labels[NUM_RTDs] = { "Heater 0  Slot 1 Top   ",
                     "Heater 1  Slot 1 Bottom",
                     "Heater 2  Slot 2 Top   ",
                     "Heater 3  Slot 2 Bottom",
                     "Heater 4  Slot 3 Top   ",
                     "Heater 5  Slot 3 Bottom",
                     "Heater 6  Slot 4 Top   ",
                     "Heater 7  Slot 4 Bottom",
                     "Heater 8  Slot 5 Top   ",
                     "Heater 9  Slot 5 Bottom",
                     "Heater 10 Slot 6 Top   ",
                     "Heater 11 Slot 6 Bottom",
                     "Heatsink               ",
                     "Ambient / Board        " };

// Logging
struct date_t
{
   int year;
   int month;
   int day;
};

enum log_type_t
{
   UNDEFINED_T,
   ERROR_T,
   COMMAND_EVENT_T,
   INTERNAL_EVENT_T,
   STOP_T
};

enum internal_event_t
{
   STARTUP_COMPLETE_T,
   TIMESYNC_RECEIVED_T
};

struct error_event_t
{
   log_type_t type;
   struct timeval timestamp;
   union ee_data_t
   {
      struct error_data_t
      {
         char const *error_code;
         char location[LOGERROR_LOCATION_SIZE];
         char description[LOGERROR_DESCR_SIZE];
      } error_data;

      struct command_event_data_t
      {
         uhc::SystemCommands command;
         char sender_ip_address[IP_STRING_SIZE];
         int command_data_len;
         int command_data[3];
      } command_event_data;

      struct internal_event_data_t
      {
         internal_event_t event;
      } internal_event_data;
   } data;
};

#define MAX_LOG_QUEUE_ELEMENTS   16
static SafeQueue<error_event_t, MAX_LOG_QUEUE_ELEMENTS> log_queue;

static const char *systemStatusStr();
static const char *slotStatusStr(int slot_number);
static const char *heaterStatusStr(int heater_number);
static const char *commandStr(uhc::SystemCommands command);
static const char *internalStr(internal_event_t event);
static void nowLocalGet(struct tm *nowtm);
static date_t currentDateGet();
static inline bool datesEqual(const date_t &date1, const date_t &date2);
static void logfileNameCreate(char *name, size_t name_len, date_t date);
static FILE *logfileOpen(const char *name);
static void logHeaderWrite(const char *name);
static void logErrorEventWrite(const char *name, const error_event_t &ee);
static void logStatusWrite(const char *name);
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address);
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1);
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1, int data2);
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1, int data2, int data3);
static int logInternalEvent(internal_event_t event);
static int logError(char const *error_code, char const *location, char const *description);
static void logAppendRecentErrors(struct timeval timestamp, const char *errorstr);
static void logStop();

// One of these lookup tables MUST be included or the code won't build
#define LOOKUPTABLEHP   1
#ifdef LOOKUPTABLEHP
#include "lookupTableHennyPenny.cpp"
#else
#include "lookupTableFrymaster.cpp"
#endif

#define NUM_TEMP_LOOKUP_ENTRIES (sizeof(tempLookupTable) / sizeof(TEMP_LOOKUP_TABLE))

static __attribute__((noreturn)) void closeProgram()
{
   alreadyClosing = true;
   if (statusPublisher != NULL)
   {
      (void)zmq_close (statusPublisher);
      statusPublisher = NULL;
   }
   if (subscriber != NULL)
   {
      (void)zmq_close (subscriber);
      subscriber = NULL;
   }
   if (timeSyncSubscriber != NULL)
   {
      (void)zmq_close (timeSyncSubscriber);
      timeSyncSubscriber = NULL;
   }
   if (timeSyncSubscriber2 != NULL)
   {
      (void)zmq_close (timeSyncSubscriber2);
      timeSyncSubscriber2 = NULL;
   }
   if (heartBeatListener != NULL)
   {
      (void)zmq_close (heartBeatListener);
      heartBeatListener = NULL;
   }
   if (heartBeatListener2 != NULL)
   {
      (void)zmq_close (heartBeatListener2);
      heartBeatListener2 = NULL;
   }
   if (commandListener != NULL)
   {
      (void)zmq_close (commandListener);
      commandListener = NULL;
   }
   if (commandListener2 != NULL)
   {
      (void)zmq_close (commandListener2);
      commandListener2 = NULL;
   }
   if (commandResponsePublisher != NULL)
   {
      (void)zmq_close (commandResponsePublisher);
      commandResponsePublisher = NULL;
   }
   if (commandResponsePublisher2 != NULL)
   {
      (void)zmq_close (commandResponsePublisher2);
      commandResponsePublisher2 = NULL;
   }
   if (firmwareUpdateListener != NULL)
   {
      (void)zmq_close (firmwareUpdateListener);
      firmwareUpdateListener = NULL;
   }
   if (firmwareUpdateResponsePublisher != NULL)
   {
      (void)zmq_close (firmwareUpdateResponsePublisher);
      firmwareUpdateResponsePublisher = NULL;
   }
   if (rtdPublisher != NULL)
   {
      (void)zmq_close (rtdPublisher);
      rtdPublisher = NULL;
   }
   if (context != NULL)
   {
      (void)zmq_ctx_destroy (context);
      context = NULL;
   }
   syslog(LOG_NOTICE, "Exiting program");
   closelog();
   exit(0);
}


/*******************************************************************************************/
/*                                                                                         */
/* static void dumpBacktrace(void)                                                         */
/*                                                                                         */
/* Print out the backtrace to see why we crashed                                           */
/*                                                                                         */
/*******************************************************************************************/
static void dumpBacktrace(void)
{
   int j, nptrs;
#define SIZE 100
   void *buffer[100];
   char **strings;

   nptrs = backtrace(buffer, SIZE);
   printf("backtrace() returned %d addresses\n", nptrs);

   /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

   strings = backtrace_symbols(buffer, nptrs);
   if (strings == NULL)
   {
      perror("backtrace_symbols");
      exit(EXIT_FAILURE);
   }

   for (j = 0; j < nptrs; j++)
   {
      printf("%s\n", strings[j]);
   }

   free(strings);
}


/*******************************************************************************************/
/*                                                                                         */
/* static void handle_signals(int signum)                                                  */
/*                                                                                         */
/* SIGINT & SIGTERM handler: set doQuit to true for graceful termination                   */
/*                                                                                         */
/*******************************************************************************************/
static void handle_signals(int signum)
{
   (void)printf("signum = %d\n", signum);

   switch (signum)
   {
      case SIGUSR1:
      case SIGSEGV:
      case SIGABRT:
      case SIGINT:
         switch (signum)
         {
            case SIGSEGV:
               (void)printf("SIGSEGV\n");
               if (!alreadyClosing)
               {
                  (void)dumpBacktrace();
                  closeProgram();
               }
               break;

            case SIGABRT:
               (void)printf("SIGABRT\n");
               if (!alreadyClosing)
               {
                  (void)dumpBacktrace();
                  closeProgram();
               }
               break;

            case SIGINT:
               (void)printf("SIGINT\n");
               if (!alreadyClosing)
               {
                  (void)dumpBacktrace();
                  closeProgram();
               }
               break;

            default:
               break;
         }
         break;

      case SIGTERM:
	     sigTermReceived = true;
	     break;

      default:
         break;
   }
}   // handle_signals()

// ****************** Utilitity functions ******************
/*******************************************************************************************/
/*                                                                                         */
/* char *removeCRLF(char *string)                                                          */
/*                                                                                         */
/* Removes CR/LF from the end of a string                                                  */
/*                                                                                         */
/* Returns: modified string                                                                */
/*                                                                                         */
/*******************************************************************************************/
char *removeCRLF(char *string, size_t string_max_len)
{
   char buf[MAX_FILE_PATH];
   (void)memset(buf, 0, sizeof(buf));

   size_t string_len = strnlen(string, string_max_len);
   int j = 0;
   for (int i = 0; i < (int)string_len; i++)
   {
      if ((string[i] != CARRIAGE_RETURN_CHARACTER) && (string[i] != LINEFEED_CHARACTER))
      {
         buf[j] = string[i];
         j++;
      }
   }

   (void)strncpy(string, buf, string_max_len);

   return string;
}


/*******************************************************************************************/
/*                                                                                         */
/* bool logDirectoryExists()                                                               */
/*                                                                                         */
/* Checks to see if the given log file directory exists. Creates it if it doesn't          */
/*                                                                                         */
/* Returns: bool logDirectoryOk                                                            */
/*                                                                                         */
/*******************************************************************************************/
bool logDirectoryExists(char const *dirname)
{
   bool logDirectoryOk = false;
   DIR* dir = opendir(dirname);
   if (0 != dir)
   {
      /* Directory exists. */
      (void)printf("Log Directory Exists\n");
      logDirectoryOk = true;
      (void)closedir(dir);
   }
   else
   {
      (void)printf("Log directory doesn't exist, have to create it: %s\n", dirname);
      (void)mkdir(dirname, LOG_DIRECTORY_PERMISSIONS);
      dir = opendir(dirname);
      if (0 != dir)
      {
         /* Directory exists. */
         (void)printf("Log directory created OK\n");
         logDirectoryOk = true;
         (void)closedir(dir);
      }
      else
      {
         (void)printf("create log directory failed\n");
      }
   }

   return logDirectoryOk;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getFileCount()                                                                      */
/*                                                                                         */
/* Gets the current count of heater data log files                                         */
/*                                                                                         */
/* Returns: int fc                                                                         */
/*                                                                                         */
/*******************************************************************************************/
int getFileCount()
{
   int fc = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)snprintf(commandLine, sizeof(commandLine) - 1, GET_FILE_COUNT, HENNYPENNY_HEATER_DATA_FILES, FILE_COUNT_FILE);
   (void)unlink(FILE_COUNT_FILE);
   sync();
   int ret = system(commandLine);
   if (0 == ret)
   {
      FILE *fp = fopen(FILE_COUNT_FILE, "r");
      if (fp != NULL)
      {
         char buf[32];
         (void)fread(buf, 1, sizeof(buf) - 1, fp);
         fc = atoi(buf);
      }
      else
      {
         (void)printf("%s doesn't exist\n", FILE_COUNT_FILE);
      }

   }

   return fc;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getNewestFile(char *newestFile)                                                     */
/*                                                                                         */
/* Gets the filename of the newest log file                                                */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int getNewestFile(char *newestFile, size_t newestFile_len)
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)snprintf(commandLine, sizeof(commandLine) - 1, GET_NEWEST_LOG_FILE, NEWEST_FILENAME_FILE);
   (void)unlink(NEWEST_FILENAME_FILE);
   sync();
   ret = system(commandLine);
   if (0 == ret)
   {
      FILE *fp = fopen(NEWEST_FILENAME_FILE, "r");
      if (fp != NULL)
      {
         char buf[MAX_FILE_PATH];
         (void)memset(buf, 0, sizeof(buf));
         (void)fread(buf, 1, sizeof(buf) - 1, fp);
         (void)removeCRLF(buf, sizeof(buf));
         (void)strncpy(newestFile, buf, newestFile_len);
      }
      else
      {
         (void)printf("%s doesn't exist\n", NEWEST_FILENAME_FILE);
         ret = 1;
      }
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getNextFilename(char *filename)                                                     */
/*                                                                                         */
/* Gets the next filename to use for heater logging                                        */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int getOldestFile(char *file, size_t file_len)
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)snprintf(commandLine, sizeof(commandLine) - 1, GET_OLDEST_LOG_FILE, OLDEST_FILENAME_FILE);
   (void)unlink(OLDEST_FILENAME_FILE);
   sync();
   ret = system(commandLine);
   if (0 == ret)
   {
      FILE *fp = fopen(OLDEST_FILENAME_FILE, "r");
      if (fp != NULL)
      {
         char buf[MAX_FILE_PATH];
         (void)memset(buf, 0, sizeof(buf));
         (void)fread(buf, 1, sizeof(buf) - 1, fp);
         (void)removeCRLF(buf, sizeof(buf));
         (void)strncpy(file, buf, file_len);
      }
      else
      {
         (void)printf("%s doesn't exist\n", OLDEST_FILENAME_FILE);
         ret = 1;
      }
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getNextFilename(char *filename)                                                     */
/*                                                                                         */
/* Gets the next filename to use for heater logging                                        */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int getNextFilename(char *filename, size_t filename_len)
{
   int ret = 0;
   char nextFilename[MAX_FILE_PATH];
   char lastFilename[MAX_FILE_PATH];

   int fileCount = getFileCount();
   if (0 == fileCount)
   {
      (void)snprintf(nextFilename, sizeof(nextFilename) - 1, HEATER_DATA_TARGET_FILE, 1);
      (void)strncpy(filename, nextFilename, filename_len);
   }
   else
   {
      if (MAX_HEATER_DATA_FILES == fileCount)
      {
         (void)getOldestFile(lastFilename, sizeof(lastFilename));
         (void)strncpy(filename, lastFilename, filename_len);
         (void)unlink(lastFilename);
      }
      else
      {
         (void)getNewestFile(lastFilename, sizeof(lastFilename));
         int idNumber = atoi(&lastFilename[strlen(lastFilename) - 5]);

         switch (idNumber)
         {
            case 1:
            case 2:
            case 3:
            case 4:
            default:
               (void)snprintf(nextFilename, sizeof(nextFilename) - 1, HEATER_DATA_TARGET_FILE, idNumber + 1);
               break;

            case 5:
               (void)snprintf(nextFilename, sizeof(nextFilename) - 1, HEATER_DATA_TARGET_FILE, 1);
               break;
         }

         (void)strncpy(filename, nextFilename, filename_len);
      }
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void getSytemUptime()                                                                   */
/*                                                                                         */
/* Gets the system uptime from the Linux kernel                                            */
/*                                                                                         */
/* Returns: systemUptime (long)                                                            */
/*                                                                                         */
/*******************************************************************************************/
long getSytemUptime()
{
   FILE * uptimefile = fopen(PROC_UPTIME_FILE, "r");
   if(NULL == uptimefile)
   {
      perror("supt");
	  return 0;
   }

   char uptime_chr[PROC_UPTIME_BUFFER_SIZE];
   (void)fgets(uptime_chr, PROC_UPTIME_BUFFER_SIZE, uptimefile);
   (void)fclose(uptimefile);

   systemUptime = strtol(uptime_chr, NULL, 10);

   if (debugPrintf)
   {
      (void)printf("System up for %ld seconds, %ld hours\n", systemUptime, systemUptime / 3600);
   }

   return systemUptime;
}


/*******************************************************************************************/
/*                                                                                         */
/* void printArray(int arr[], int size)                                                    */
/*                                                                                         */
/* Prints the specified number of elements of an array of integers. Used for debug.        */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void printArray(int arr[], int size)
{
   for (int i = 0; i < size; i++)
   {
      (void)printf("%d ", arr[i]);
   }

   (void)printf("\n");
}


/*******************************************************************************************/
/*                                                                                         */
/* void swap(int *xp, int *yp)                                                             */
/*                                                                                         */
/* Swaps two values using pointers. Used by the selectionSort.                             */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void swap(int *xp, int *yp)
{
   int temp = *xp;
   *xp = *yp;
   *yp = temp;
}


/*******************************************************************************************/
/*                                                                                         */
/* void selectionSort(int arr[], int n)                                                    */
/*                                                                                         */
/* Sort the specified array from low to high values. Used by the heater control algorithm. */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void selectionSort(int arr[], int n)
{
   // One by one move boundary of unsorted subarray
   for (int i = 0; i < n - 1; i++)
   {
      // Find the minimum element in unsorted array
      int min_idx = i;
      for (int j = i + 1; j < n; j++)
      {
         if (arr[j] < arr[min_idx])
         {
            min_idx = j;
         }
      }

      // Swap the found minimum element with the first element
      if(min_idx != i)
      {
         swap(&arr[min_idx], &arr[i]);
      }
   }

   if (debugHeatersPrintf)
   {
      (void)printArray(arr, n);
   }
}


// ****************** Board revision code ******************
/*******************************************************************************************/
/*                                                                                         */
/* int initBoardRevIDGPIOs()                                                               */
/*                                                                                         */
/* Initialize the GPIO bits that comprise the controller board hardware revision           */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initBoardRevIDGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, BOARD_ID_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void getBoardRev()                                                                      */
/*                                                                                         */
/* Read the controller board hardware revision bits and populate the integer value         */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void getBoardRev()
{
   // open the three files to read the GPIOs from
   int id0_fd = open(BOARD_ID0_VALUE, O_RDONLY);
   int id1_fd = open(BOARD_ID1_VALUE, O_RDONLY);
   int id2_fd = open(BOARD_ID2_VALUE, O_RDONLY);

   // if all three file descriptors are valid, read the bits
   if ((id0_fd > 0) && (id1_fd > 0) && (id2_fd > 0))
   {
      // initialize the buffers
      char buf0[BIT_READ_BUFFER_SIZE];
      char buf1[BIT_READ_BUFFER_SIZE];
      char buf2[BIT_READ_BUFFER_SIZE];
      (void)memset(buf0, 0, sizeof(buf0));
      (void)memset(buf1, 0, sizeof(buf1));
      (void)memset(buf2, 0, sizeof(buf2));

      int bit0 = 0;
      int bit1 = 0;
      int bit2 = 0;

      // read the three bits from the GPIOs
      int bytesRead = read(id0_fd, buf0, sizeof(buf0));
      if (bytesRead > 1)
      {
         buf0[bytesRead-1] = '\0';
         bit0 = atoi(buf0);
      }

      bytesRead = read(id1_fd, buf1, sizeof(buf1));
      if (bytesRead > 1)
      {
         buf1[bytesRead-1] = '\0';
         bit1 = atoi(buf1);
      }

      bytesRead = read(id2_fd, buf2, sizeof(buf2));
      if (bytesRead > 1)
      {
         buf2[bytesRead-1] = '\0';
         bit2 = atoi(buf2);
      }

      // close the files
      (void)close(id0_fd);
      (void)close(id1_fd);
      (void)close(id2_fd);

      // build the controller board revision from the 3 bits
      controllerBoardRevision = (bit2 << 2) | (bit1 << 1) | bit0;
   }
}

// ****************** Fan code ******************
/*******************************************************************************************/
/*                                                                                         */
/* int initFanGPIOs()                                                                      */
/*                                                                                         */
/* Initialize the GPIO bits that control the controller board cooling fans                 */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int initFanGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, FAN_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initFanDataStructures()                                                             */
/*                                                                                         */
/* Initialize the data structures for the controller board cooling fans                    */
/*                                                                                         */
/* Returns: int ret - 0=success                                                            */
/*                                                                                         */
/*******************************************************************************************/
int initFanDataStructures()
{
   (void)memset(fanInfo, 0, sizeof(fanInfo));

   int i = 0;
   fanInfo[i].tach_value = 0;
   fanInfo[i].fan_on = FAN_STATE_OFF;
   (void)strncpy(fanInfo[i].ON_GPIO_PATH, FAN1_ON_VALUE, sizeof(fanInfo[i].ON_GPIO_PATH));
   fanInfo[i].fd_on = open(fanInfo[i].ON_GPIO_PATH, O_WRONLY);
   (void)strncpy(fanInfo[i].TACH_GPIO_PATH, FAN1_TACH_VALUE, sizeof(fanInfo[i].TACH_GPIO_PATH));
   fanInfo[i].fd_tach = open(fanInfo[i].TACH_GPIO_PATH, O_RDONLY);
   (void)strncpy(fanInfo[i].ILIM_GPIO_PATH, FAN1_ILIM_VALUE, sizeof(fanInfo[i].ILIM_GPIO_PATH));
   fanInfo[i].fd_ilim = open(fanInfo[i].ILIM_GPIO_PATH, O_RDONLY);
   fanInfo[i].current_limit_tripped = false;
   fanInfo[i].current_limit_auto_correct_count = 0;
   fanInfo[i].current_limit_delay_count = 0;
   fanInfo[i].consecutive_tach_failures = 0;

   i++;
   fanInfo[i].tach_value = 0;
   fanInfo[i].fan_on = FAN_STATE_OFF;
   (void)strncpy(fanInfo[i].ON_GPIO_PATH, FAN2_ON_VALUE, sizeof(fanInfo[i].ON_GPIO_PATH));
   fanInfo[i].fd_on = open(fanInfo[i].ON_GPIO_PATH, O_WRONLY);
   (void)strncpy(fanInfo[i].TACH_GPIO_PATH, FAN2_TACH_VALUE, sizeof(fanInfo[i].TACH_GPIO_PATH));
   fanInfo[i].fd_tach = open(fanInfo[i].TACH_GPIO_PATH, O_RDONLY);
   (void)strncpy(fanInfo[i].ILIM_GPIO_PATH, FAN2_ILIM_VALUE, sizeof(fanInfo[i].ILIM_GPIO_PATH));
   fanInfo[i].fd_ilim = open(fanInfo[i].ILIM_GPIO_PATH, O_RDONLY);
   fanInfo[i].current_limit_tripped = false;
   fanInfo[i].current_limit_auto_correct_count = 0;
   fanInfo[i].current_limit_delay_count = 0;
   fanInfo[i].consecutive_tach_failures = 0;

   return 0;
}


/*******************************************************************************************/
/*                                                                                         */
/* bool getFanCurrentLimit(int fanIndex)                                                   */
/*                                                                                         */
/* Get the current limit bit for the requested fan.                                        */
/*                                                                                         */
/* Returns: bool current_limit                                                             */
/*                                                                                         */
/*******************************************************************************************/
bool getFanCurrentLimit(int fanIndex)
{
   bool current_limit = false;
   char buf[BIT_READ_BUFFER_SIZE];

   // if the index is in range, get the tach_value
   if ((fanIndex >= FAN1_INDEX) && (fanIndex <= FAN2_INDEX))
   {
      // read the ILIM GPIO input for this fan
      (void)lseek(fanInfo[fanIndex].fd_ilim, 0, SEEK_SET);
      (void)memset(buf, 0, sizeof(buf));
      int readLen = read(fanInfo[fanIndex].fd_ilim, buf, sizeof(buf));
      if (readLen > 1)
      {
         // more than EOL; make sure it's NULL terminated
         buf[readLen-1] = '\0';
         // a low signal indicates the limit (over voltage, or over current) tripped
         int intVal = atoi(buf);
         if (0 == intVal)
         {
            current_limit = true;
         }
         else
         {
            current_limit = false;
         }

         fanInfo[fanIndex].current_limit_tripped = current_limit;
      }
   }

   return(current_limit);
}


/*******************************************************************************************/
/*                                                                                         */
/* int setFanOnOff(int fanIndex, uint8_t on)                                               */
/*                                                                                         */
/* Turns the requested fan on/off                                                          */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int setFanOnOff(int fanIndex, uhc::FanState on)
{
   int ret = 0;
   if ((fanIndex >= FAN1_INDEX) && (fanIndex <= FAN2_INDEX))
   {
      // if the index is in range, set fan_on to the specified value
      int fd_on = fanInfo[fanIndex].fd_on;
      int bytesWritten = 0;
      fanInfo[fanIndex].fan_on = on;
      if (FAN_STATE_ON == on)
      {
         fanInfo[fanIndex].current_limit_tripped = false;
         if (FAN1_INDEX == fanIndex)
         {
            if (!fan1CurrentLimitOneShot || !fan1FailureOneShot)
            {
               syslog(LOG_NOTICE, "Fan 1 normal operation restored");
               (void)logError("", "Fan 1 normal operation restored", "Fan 1 normal operation restored");
            }
         }
         else
         {
            if (!fan2CurrentLimitOneShot || !fan2FailureOneShot)
            {
               syslog(LOG_NOTICE, "Fan 2 normal operation restored");
               (void)logError("", "Fan 2 normal operation restored", "Fan 2 normal operation restored");
            }
            fan2CurrentLimitOneShot = true;
            fan2FailureOneShot = true;
         }
         bytesWritten = write(fd_on, FAN_ON_STRING, 1);
         if (debugPrintf)
         {
            (void)printf("Fan %d ON\n", fanIndex);
         }
      }
      else
      {
         bytesWritten = write(fd_on, FAN_OFF_STRING, 1);
         if (debugPrintf)
         {
            (void)printf("Fan %d OFF\n", fanIndex);
         }
      }

      if (-1 == bytesWritten)
      {
         if (debugPrintf)
         {
            (void)printf("Error writing to %s\n", fanInfo[fanIndex].ON_GPIO_PATH);
         }
         ret = 1;
      }
   }
   else
   {
      // bad index
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getFanTach(int fanIndex)                                                            */
/*                                                                                         */
/* Get the tach value for the requested fan.                                               */
/*                                                                                         */
/* Returns: int tach_value                                                                 */
/*                                                                                         */
/*******************************************************************************************/
int getFanTach(int fanIndex)
{
   int tach_value = 0;
   char buf[BIT_READ_BUFFER_SIZE];

   // if the index is in range, get the tach_value
   if ((fanIndex >= FAN1_INDEX) && (fanIndex <= FAN2_INDEX))
   {
      // since all we have is a digital GPIO connected to the fan's tach output
      // read it until we get a 1
      for (int i = 0; (i < FAN_TACH_RETRY_LIMIT) && (0 == tach_value); i++)
      {
         (void)lseek(fanInfo[fanIndex].fd_tach, 0, SEEK_SET);
         (void)memset(buf, 0, sizeof(buf));
         int readLen = read(fanInfo[fanIndex].fd_tach, buf, sizeof(buf));
         if (readLen > 1)
         {
            // more than EOL; make sure it's NULL terminated
            buf[readLen-1] = '\0';
            tach_value = atoi(buf);
            fanInfo[fanIndex].tach_value = tach_value;
            if (0 == tach_value)
            {
               // wait 5ms and try again
               (void)usleep(FAN_TACH_RE_READ_TIME_uS);
            }
         }
      }
   }

   return(tach_value);
}


/*******************************************************************************************/
/*                                                                                         */
/* int turnFansOnOff(uhc::FanState on)                                                     */
/*                                                                                         */
/* Turn both fans on or off                                                                */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int turnFansOnOff(uhc::FanState on)
 {
   int ret = 0;
   ret =  setFanOnOff(FAN1_INDEX, on);
   ret |= setFanOnOff(FAN2_INDEX, on);

   return ret;
 }


// ************* Power Monitor Code *************
/**
 * @brief Initialize the Power Monitor GPIOs.
 *
 * This is to initialize the GPIOs connected to the Power Monitor IC's discrete outputs.
 * The GPIOs for the IC's SPI interface are initialized in initSPIGPIOs().
 */
int initPowerMonGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, POWERMON_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


// ****************** RTD code ******************
/*******************************************************************************************/
/*                                                                                         */
/* int initSPIGPIOs()                                                                      */
/*                                                                                         */
/* Initialize the GPIO bits that comprise the controller board hardware revision           */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initSPIGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, SPI_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initPGA117SPI()                                                                     */
/*                                                                                         */
/* Initialize the GPIOs for managing the RTDs.                                             */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initPGA117SPI()
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

   (void)memset(fileName, 0, sizeof(fileName));
   (void)snprintf(fileName, sizeof(fileName), ADC_FILE_PATH, AIN0_CHANNEL);
   fd_analogInput0 = open(fileName, O_RDONLY);

   (void)memset(fileName, 0, sizeof(fileName));
   (void)snprintf(fileName, sizeof(fileName), ADC_FILE_PATH, AIN1_CHANNEL);
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
   //      14              1               1          6          1

   (void)memset(rtdMappings, 0, sizeof(rtdMappings));

   int i = 0;
   rtdMappings[i].rtd_number           = RTD_NUMBER_1;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH5;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD1, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 1
   rtdMappings[i].rtd_number           = RTD_NUMBER_2;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH4;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD2, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 2
   rtdMappings[i].rtd_number           = RTD_NUMBER_3;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH3;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD3, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 3
   rtdMappings[i].rtd_number           = RTD_NUMBER_4;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH2;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD4, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 4
   rtdMappings[i].rtd_number           = RTD_NUMBER_5;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH1;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD5, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 5
   rtdMappings[i].rtd_number           = RTD_NUMBER_6;
   rtdMappings[i].analog_input_channel = AIN0_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ZERO;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH7;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput0;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD6, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 6
   rtdMappings[i].rtd_number           = RTD_NUMBER_7;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH5;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD7, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 7
   rtdMappings[i].rtd_number           = RTD_NUMBER_8;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH4;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD8, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 8
   rtdMappings[i].rtd_number           = RTD_NUMBER_9;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH3;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD9, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 9
   rtdMappings[i].rtd_number           = RTD_NUMBER_10;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH2;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD10, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 10
   rtdMappings[i].rtd_number           = RTD_NUMBER_11;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH1;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD11, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 11
   rtdMappings[i].rtd_number           = RTD_NUMBER_12;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH7;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = 0;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD12, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 12
   rtdMappings[i].rtd_number           = RTD_NUMBER_13;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH8;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = DEFAULT_HEATSINK_TEMP_COUNT;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD_HEATSINK, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

   i++;  // 13
   rtdMappings[i].rtd_number           = RTD_NUMBER_14;
   rtdMappings[i].analog_input_channel = AIN1_CHANNEL;
   rtdMappings[i].mux_number           = MUX_NUMBER_ONE;
   rtdMappings[i].mux_channel          = PGA117_CHANNEL_CH6;
   rtdMappings[i].gain                 = PGA117_GAIN_1;
   rtdMappings[i].value                = DEFAULT_HEATSINK_TEMP_COUNT;
   rtdMappings[i].is_shorted           = false;
   rtdMappings[i].is_open              = false;
   rtdMappings[i].shorted_oneshot      = false;
   rtdMappings[i].open_oneshot         = false;
   rtdMappings[i].seconds_shorted      = 0;
   rtdMappings[i].seconds_open         = 0;
   rtdMappings[i].fd                   = fd_analogInput1;
   (void)strncpy(rtdMappings[i].temp_data_filename, TEMPERATURE_LOOKUP_FILENAME_RTD_BOARD, sizeof(rtdMappings[i].temp_data_filename));
   (void)memcpy(rtdMappings[i].temp_lookup_table, tempLookupTable, sizeof(rtdMappings[i].temp_lookup_table));

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
      bool found = false;
      for (int i = 0; (i < (int)NUM_TEMP_LOOKUP_ENTRIES) && !found; i++)
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
int getTemperatureLookupFilename(int rtdIndex, char *filename, size_t filename_len)
{
   int ret = FUNCTION_FAILURE;
   char tempFileName[MAX_FILE_PATH];
   (void)memset(tempFileName, 0, sizeof(tempFileName));
   (void)strncpy(tempFileName, rtdMappings[rtdIndex].temp_data_filename, sizeof(tempFileName));

   // see if the file that specifies the name of the temperature lookup file exists
   if (access(rtdMappings[rtdIndex].temp_data_filename, R_OK) == FUNCTION_SUCCESS)
   {
      FILE *fp = fopen(rtdMappings[rtdIndex].temp_data_filename, "r");
      if (fp != NULL)
      {
         // the file does exist
         // read the file name from within that file
         char buf[MAX_FILE_PATH];
         (void)memset(buf, 0, sizeof(buf));
         int bytesRead = fread(buf, 1, sizeof(buf) - 1, fp);
         (void)fclose(fp);

         if (bytesRead > 0)
         {
            // strip off any CRs or LFs
            for (int i = 0; i < (int)strnlen(buf, sizeof(buf)); i++)
            {
               if ((buf[i] == CARRIAGE_RETURN_CHARACTER) || (buf[i] == LINEFEED_CHARACTER))
               {
                  buf[i] = 0;
               }
            }

            // now see if the target temperature lookup table file exists
            (void)strncpy(tempFileName, buf, sizeof(tempFileName));
            if (access(tempFileName, R_OK) == FUNCTION_SUCCESS)
            {
               // target file exists
               // use it as the filename
               (void)strncpy(filename, tempFileName, filename_len);
               ret = FUNCTION_SUCCESS;
            }
            else
            {
               // specified file doesn't exist; use default name
               (void)strncpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE, filename_len);
            }
         }
         else
         {
            // no filename contained in the file; use default name
            (void)strncpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE, filename_len);
         }
      }
      else
      {
         // no filename file found; use default name
         (void)strncpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE, filename_len);
      }
   }
   else
   {
      // no TEMPERATURE_LOOKUP_FILENAME_FILE exists; use default name
      (void)strncpy(filename, TEMPERATURE_LOOKUP_TABLE_FILE, filename_len);
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
   TEMP_LOOKUP_TABLE tlt[NUM_TEMP_LOOKUP_ENTRIES];
   char calibratedTempLookupTableFilename[MAX_FILE_PATH];
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)unlink(TEMPERATURE_LOOKUP_TABLE_SYMLINK_FILE);
   (void)memset(commandLine, 0, sizeof(commandLine));
   if (controllerBoardRevision > 0)
   {
      (void)snprintf(commandLine, sizeof(commandLine), TEMPERATURE_LOOKUP_TABLE_SYMLINK_COMMAND, A02_HARDWARE_REV_TEMP_LOOKUP_TABLE, TEMPERATURE_LOOKUP_TABLE_SYMLINK_FILE);
	  (void)system(commandLine);
   }
   else
   {
      (void)snprintf(commandLine, sizeof(commandLine), TEMPERATURE_LOOKUP_TABLE_SYMLINK_COMMAND, A01_HARDWARE_REV_TEMP_LOOKUP_TABLE, TEMPERATURE_LOOKUP_TABLE_SYMLINK_FILE);
	  (void)system(commandLine);
   }
   for (int i = 0; i < NUM_RTDs; i++)
   {
      (void)memset(tlt, 0, sizeof(tlt));
      (void)memset(calibratedTempLookupTableFilename, 0, sizeof(calibratedTempLookupTableFilename));
      (void)getTemperatureLookupFilename(i, calibratedTempLookupTableFilename, sizeof(calibratedTempLookupTableFilename));
      FILE *fp = fopen(calibratedTempLookupTableFilename, "r");
      if (fp != NULL)
      {
         uint16_t numRows = 0;
         (void)fscanf(fp, "%hu\n", &numRows);
         if (NUM_TEMP_LOOKUP_ENTRIES != numRows)
         {
            (void)printf("Row count of %hu doesn't match expected vaue of %u\n", numRows, NUM_TEMP_LOOKUP_ENTRIES);
            (void)fclose(fp);
            ret = FUNCTION_FAILURE;
            return ret;
         }

         int count = 0;
         for (int j = 0; j < numRows; j++)
         {
            count += fscanf(fp, "%hu %hu\n", &tlt[j].degreesF, &tlt[j].adc_raw_counts);
         }

         (void)fclose(fp);
         fp = NULL;

         if (((NUM_TEMP_LOOKUP_ENTRIES * 2) == count) && (FIRST_TEMPERATURE_ENTRY == tlt[0].degreesF) && (LAST_TEMPERATURE_ENTRY == tlt[NUM_TEMP_LOOKUP_ENTRIES - 1].degreesF))
         {
            (void)memcpy(rtdMappings[i].temp_lookup_table, tlt, sizeof(rtdMappings[i].temp_lookup_table));
         }
         else
         {
            (void)printf("Temperature file %s isn't a valid temp file!\n", calibratedTempLookupTableFilename);
         }
      }
      else
      {
         (void)printf("Error reading Temperature data file %s!\n", calibratedTempLookupTableFilename);
      }
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int issueSPICommands(int rtd_index)                                                     */
/*                                                                                         */
/* Builds and issues the SPI commands for the daisy chained PGA117 analog muxes.           */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int issueSPICommands(int rtd_index)
{
   int retVal = 0;
   uint16_t mux1Command = 0;
   uint16_t mux2Command = 0;

   // Select channel 0 (GND) with unity gain as the output for the Mux we're not trying to read
   if (rtd_index < NUM_RTDS_ON_AIN0)
   {
      /* reading Mux 0 */
      /* Format the commands */
      mux1Command = PGA117_CMD_WRITE |
         ((uint16_t)rtdMappings[rtd_index].mux_channel << PGA117_CHANNEL_SHIFT) |
         ((uint16_t)rtdMappings[rtd_index].gain << PGA117_GAIN_SHIFT);

      mux2Command = PGA117_DCCMD_WRITE |
         ((uint16_t)PGA117_CHANNEL_CH0 << PGA117_CHANNEL_SHIFT) |
         ((uint16_t)PGA117_GAIN_1 << PGA117_GAIN_SHIFT);
   }
   else
   {
      /* reading Mux 1 */
      /* Format the commands */
      mux1Command = PGA117_CMD_WRITE |
         ((uint16_t)PGA117_CHANNEL_CH0 << PGA117_CHANNEL_SHIFT) |
         ((uint16_t)PGA117_GAIN_1 << PGA117_GAIN_SHIFT);

      mux2Command = PGA117_DCCMD_WRITE |
         ((uint16_t)rtdMappings[rtd_index].mux_channel << PGA117_CHANNEL_SHIFT) |
         ((uint16_t)rtdMappings[rtd_index].gain << PGA117_GAIN_SHIFT);
   }

   // when daisy chained, the second devices' command gets sent first
   char buf[SPI_TRANSACTION_BYTE_COUNT];
   buf[0] = (mux2Command >> 8) & 0xFF;
   buf[1] = mux2Command & 0xFF;
   buf[2] = (mux1Command >> 8) & 0xFF;
   buf[3] = mux1Command & 0xFF;
   // write the commands
   int ret = write(fd_spi_bus_0, buf, SPI_TRANSACTION_BYTE_COUNT);
   if (ret != SPI_TRANSACTION_BYTE_COUNT)
   {
      retVal = 1;
   }

   // allow 10 uS for the output of the Mux (with unity gain) to stabliize.
   // the datasheet states 2.55 uS for 0.01% settling time
   // so 10uS is plenty of time to settle.
   (void)usleep(10);

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
   char topString[] = {"TOP"};
   char bottomString[] = {"BOTTOM"};

   if (rtdMappings[rtd_index].fd != -1)
   {
      int retryCount = 0;

      // issuing the SPI commands routes the RTD data through the PGA to either AIN0 or AIN1 ADC
      // which is actually the value we need (raw ADC count)
      (void)issueSPICommands(rtd_index);
      // each time we read a value, we need to rewind the file descriptor back to the beginning of the file
      // or we'll never read another value
      (void)usleep(10000);
      (void)lseek(rtdMappings[rtd_index].fd, 0, SEEK_SET);
      char buf[ADC_READ_BUFFER_SIZE];
      (void)memset(buf, 0, sizeof(buf));
      int readLen = read(rtdMappings[rtd_index].fd, buf, sizeof(buf));
      if (readLen > 1)
      {
         // more than EOL; make sure it's NULL terminated
         buf[readLen-1] = '\0';
         rawCount = atoi(buf);
         // check for out of range
         // if so, read up to 4 more times
         if ((retryCount < MAX_READ_RTD_RETRY_COUNT) && ((rawCount > rtdMappings[rtd_index].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
                                                     ||  (rawCount < rtdMappings[rtd_index].temp_lookup_table[0].adc_raw_counts)))
         {
            retryCount++;
            (void)issueSPICommands(rtd_index);
            (void)usleep(10000);
            (void)lseek(rtdMappings[rtd_index].fd, 0, SEEK_SET);
            (void)memset(buf, 0, sizeof(buf));
            readLen = read(rtdMappings[rtd_index].fd, buf, sizeof(buf));
            if (readLen > 1)
            {
               // more than EOL; make sure it's NULL terminated
               buf[readLen-1] = '\0';
               rawCount = atoi(buf);
            }
         }

         char *locationString = NULL;
         int lowerHeaterThisShelf = 0;
         int upperHeaterThisShelf = 0;
         int shelfNumber = 0;
         if (rtd_index < NUM_HEATERS)
         {
            if (HEATER_POSITION_TOP == heaterInfo[rtd_index].location)
            {
               locationString = topString;
            }
            else
            {
               locationString = bottomString;
            }
            // reading one of the heater RTDs
            // if the counts are above 340 degree counts, call that open
            if (rawCount > rtdMappings[rtd_index].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
            {
               rtdMappings[rtd_index].seconds_open++;
               if (rtdMappings[rtd_index].seconds_open > MAX_CONSECUTIVE_SECONDS_ERROR)
               {
                  rtdMappings[rtd_index].is_open = true;
                  heaterInfo[rtd_index].is_enabled = false;
                  // according to FRON-246, if an RTD is open the SHELF is disabled, not just the single heater
                  // Above statment was said to be incorrect by Dan Wells, if we read an open OR short on 1 
                  // heater the corresponding heater opposite should remain on and be read for another open or short. 12/15/23 MC
                  /*
                  if (HEATER_LOCATION_LOWER == heaterInfo[rtd_index].location)
                  {
                     // this is a bottom/lower heater, also disable the corresponding top/upper heater
                     upperHeaterThisShelf = rtd_index - 1;
                     lowerHeaterThisShelf = rtd_index;
                     heaterInfo[rtd_index-1].is_enabled = false;
                  }
                  else
                  {
                     // this is an upper/top heater, also disable the corresponding bottom/lower heater
                     lowerHeaterThisShelf = rtd_index + 1;
                     upperHeaterThisShelf = rtd_index;
                     heaterInfo[rtd_index+1].is_enabled = false;
                  }
                  */
                  (void)turnHeaterOnOff(lowerHeaterThisShelf, HEATER_STATE_OFF);
                  (void)turnHeaterOnOff(upperHeaterThisShelf, HEATER_STATE_OFF);

                  switch (rtd_index)
                  {
                     case SLOT1_TOP_HEATER_INDEX:
                     case SLOT1_BOTTOM_HEATER_INDEX:
                     default:
                       shelfNumber = SLOT1;
                        break;

                     case SLOT2_TOP_HEATER_INDEX:
                     case SLOT2_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT2;
                        break;

                     case SLOT3_TOP_HEATER_INDEX:
                     case SLOT3_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT3;
                        break;

                     case SLOT4_TOP_HEATER_INDEX:
                     case SLOT4_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT4;
                        break;

                     case SLOT5_TOP_HEATER_INDEX:
                     case SLOT5_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT5;
                        break;

                     case SLOT6_TOP_HEATER_INDEX:
                     case SLOT6_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT6;
                        break;
                  }
                  if (!rtdMappings[rtd_index].open_oneshot)
                  {
                     rtdMappings[rtd_index].open_oneshot = true;
                     syslog(LOG_ERR, "%s Temp probe open failure for RTD %d raw count %d location %s shelf %d", TEMP_PROBE_OPEN_ERROR_CODE, rtd_index, rawCount, locationString, shelfNumber);
                     (void)strncpy(errorCodeString, TEMP_PROBE_OPEN_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     char location_str[LOGERROR_LOCATION_SIZE];
                     (void)snprintf(location_str, sizeof(location_str), "Shelf %d %s", shelfNumber, locationString);
                     (void)logError(TEMP_PROBE_OPEN_ERROR_CODE, location_str, "Temp probe open failure");
                  }
               }
            }
            else
            {
               rtdMappings[rtd_index].is_open = false;
               rtdMappings[rtd_index].open_oneshot = false;
               rtdMappings[rtd_index].seconds_open = 0;
            }

            // if the counts are less than the first entry in the temp table, call that "shorted"
            if (rawCount < rtdMappings[rtd_index].temp_lookup_table[0].adc_raw_counts)
            {
               rtdMappings[rtd_index].seconds_shorted++;
               if (rtdMappings[rtd_index].seconds_shorted > MAX_CONSECUTIVE_SECONDS_ERROR)
               {
                  rtdMappings[rtd_index].is_shorted = true;
                  heaterInfo[rtd_index].is_enabled = false;
                  // according to FRON-246, if an RTD is open the SHELF is disabled, not just the single heater
                  // Above statment was said to be incorrect by Dan Wells, if we read an open OR short on 1 
                  // heater the corresponding heater opposite should remain on and be read for another open or short. 12/15/23 MC
                  /*
                  if (HEATER_LOCATION_LOWER == heaterInfo[rtd_index].location)
                  {
                     // this is a bottom/lower heater, also disable the corresponding top/upper heater
                     upperHeaterThisShelf = rtd_index - 1;
                     lowerHeaterThisShelf = rtd_index;
                     heaterInfo[rtd_index-1].is_enabled = false;
                  }
                  else
                  {
                     // this is an upper/top heater, also disable the corresponding bottom/lower heater
                     lowerHeaterThisShelf = rtd_index + 1;
                     upperHeaterThisShelf = rtd_index;
                     heaterInfo[rtd_index+1].is_enabled = false;
                  }
                  */
                  (void)turnHeaterOnOff(lowerHeaterThisShelf, HEATER_STATE_OFF);
                  (void)turnHeaterOnOff(upperHeaterThisShelf, HEATER_STATE_OFF);

                  switch (rtd_index)
                  {
                     case SLOT1_TOP_HEATER_INDEX:
                     case SLOT1_BOTTOM_HEATER_INDEX:
                     default:
                        shelfNumber = SLOT1;
                        break;

                     case SLOT2_TOP_HEATER_INDEX:
                     case SLOT2_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT2;
                        break;

                     case SLOT3_TOP_HEATER_INDEX:
                     case SLOT3_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT3;
                        break;

                     case SLOT4_TOP_HEATER_INDEX:
                     case SLOT4_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT4;
                        break;

                     case SLOT5_TOP_HEATER_INDEX:
                     case SLOT5_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT5;
                        break;

                     case SLOT6_TOP_HEATER_INDEX:
                     case SLOT6_BOTTOM_HEATER_INDEX:
                        shelfNumber = SLOT6;
                        break;
                  }
                  if (!rtdMappings[rtd_index].shorted_oneshot)
                  {
                     rtdMappings[rtd_index].shorted_oneshot = true;
                     syslog(LOG_ERR, "%s Temp probe shorted failure for RTD %d raw count %d location %s shelf %d", TEMP_PROBE_CLOSED_ERROR_CODE, rtd_index, rawCount, locationString, shelfNumber);
                     (void)strncpy(errorCodeString, TEMP_PROBE_CLOSED_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     char location_str[LOGERROR_LOCATION_SIZE];
                     (void)snprintf(location_str, sizeof(location_str), "Shelf %d %s", shelfNumber, locationString);
                     (void)logError(TEMP_PROBE_CLOSED_ERROR_CODE, location_str, "Temp probe shorted failure");
                  }
               }
            }
            else
            {
               rtdMappings[rtd_index].seconds_shorted = 0;
               rtdMappings[rtd_index].is_shorted = false;
               rtdMappings[rtd_index].shorted_oneshot = false;
            }
         }
         else if (HEATSINK_RTD_INDEX == rtd_index)
         {
            // reading the heatsink RTD
            // if the counts are above 340 degree counts, call that open
            if (rawCount > rtdMappings[rtd_index].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
            {
               rtdMappings[rtd_index].seconds_open++;
               if (rtdMappings[rtd_index].seconds_open > MAX_CONSECUTIVE_SECONDS_ERROR)
               {
                  rtdMappings[rtd_index].is_open = true;
                  // turn all heaters off
                  for (int j = 0; j < NUM_HEATERS; j++)
                  {
                     (void)enableDisableHeater(j, HEATER_DISABLED);
                     (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
                  }

                  if (!rtdMappings[rtd_index].open_oneshot)
                  {
                     rtdMappings[rtd_index].open_oneshot = true;
                     syslog(LOG_ERR, "%s Temp probe open failure for HEATSINK raw count %d", TEMP_PROBE_OPEN_ERROR_CODE, rawCount);
                     (void)strncpy(errorCodeString, TEMP_PROBE_OPEN_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     (void)logError(TEMP_PROBE_OPEN_ERROR_CODE, "Heat sink", "Temp probe open failure");
                     syslog(LOG_ERR, "Heaters turned off and disabled");
                  }
               }
            }
            else
            {
               rtdMappings[rtd_index].seconds_open = 0;
               rtdMappings[rtd_index].is_open = false;
               rtdMappings[rtd_index].open_oneshot = false;
            }

            // if the counts are less than the first entry in the temp table, call that "shorted"
            if (rawCount < rtdMappings[rtd_index].temp_lookup_table[0].adc_raw_counts)
            {
               rtdMappings[rtd_index].seconds_shorted++;
               if (rtdMappings[rtd_index].seconds_shorted > MAX_CONSECUTIVE_SECONDS_ERROR)
               {
                  rtdMappings[rtd_index].is_shorted = true;
                  // turn all heaters off
                  for (int j = 0; j < NUM_HEATERS; j++)
                  {
                     (void)enableDisableHeater(j, HEATER_DISABLED);
                     (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
                  }
                  if (!rtdMappings[rtd_index].shorted_oneshot)
                  {
                     rtdMappings[rtd_index].shorted_oneshot = true;
                     syslog(LOG_ERR, "%s Temp probe shorted failure for HEATSINK raw count %d", TEMP_PROBE_CLOSED_ERROR_CODE, rawCount);
                     (void)strncpy(errorCodeString, TEMP_PROBE_CLOSED_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     (void)logError(TEMP_PROBE_CLOSED_ERROR_CODE, "Heat sink", "Temp probe shorted failure");
                     syslog(LOG_ERR, "Heaters turned off and disabled");
                  }
               }
            }
            else
            {
               rtdMappings[rtd_index].seconds_shorted = 0;
               rtdMappings[rtd_index].is_shorted = false;
               rtdMappings[rtd_index].shorted_oneshot = false;
            }
         }
         else
         {
            // reading the PCB (board, aka AMBIENT) temp sensor
            // only if the board is rev A02 or greater
            if (controllerBoardRevision > 0)
            {
               // if the counts are above 340 degree counts, call that open
               if (rawCount > rtdMappings[rtd_index].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
               {
                  rtdMappings[rtd_index].seconds_open++;
                  rtdMappings[rtd_index].is_open = true;
                  if (rtdMappings[rtd_index].seconds_open > MAX_CONSECUTIVE_SECONDS_ERROR)
                  {
#if (0)
                     // turn all heaters off
                     for (int j = 0; j < NUM_HEATERS; j++)
                     {
                        (void)enableDisableHeater(j, HEATER_DISABLED);
                        (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
                     }
#endif
                     if (!rtdMappings[rtd_index].open_oneshot)
                     {
                        rtdMappings[rtd_index].open_oneshot = true;
                        syslog(LOG_ERR, "%s Temp probe open failure for AMBIENT raw count %d", TEMP_PROBE_OPEN_ERROR_CODE, rawCount);
                        (void)strncpy(errorCodeString, TEMP_PROBE_OPEN_ERROR_CODE, sizeof(errorCodeString) - 1);
                        systemStatus = SYSTEM_STATUS_ERROR;
                        (void)logError(TEMP_PROBE_OPEN_ERROR_CODE, "Heat sink", "Temp probe open failure");
//                        syslog(LOG_ERR, "Heaters turned off and disabled");
                     }
                  }
               }
               else
               {
                  rtdMappings[rtd_index].seconds_open = 0;
                  rtdMappings[rtd_index].is_open = false;
                  rtdMappings[rtd_index].open_oneshot = false;
               }

               // if the counts are less than the first entry in the temp table, call that "shorted"
               if (rawCount < rtdMappings[rtd_index].temp_lookup_table[0].adc_raw_counts)
               {
                  rtdMappings[rtd_index].seconds_shorted++;
                  if (rtdMappings[rtd_index].seconds_shorted > MAX_CONSECUTIVE_SECONDS_ERROR)
                  {
                     rtdMappings[rtd_index].is_shorted = true;
#if (0)
                     // turn all heaters off
                     for (int j = 0; j < NUM_HEATERS; j++)
                     {
                        (void)enableDisableHeater(j, HEATER_DISABLED);
                        (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
                     }
#endif
                     if (!rtdMappings[rtd_index].shorted_oneshot)
                     {
                        syslog(LOG_ERR, "%s Temp probe shorted failure for AMBIENT raw count %d", TEMP_PROBE_CLOSED_ERROR_CODE, rawCount);
                        (void)strncpy(errorCodeString, TEMP_PROBE_CLOSED_ERROR_CODE, sizeof(errorCodeString) - 1);
                        systemStatus = SYSTEM_STATUS_ERROR;
                        (void)logError(TEMP_PROBE_CLOSED_ERROR_CODE, "Heat sink", "Temp probe shorted failure");
//                        syslog(LOG_ERR, "Heaters turned off and disabled");
                     }
                  }
               }
               else
               {
                  rtdMappings[rtd_index].seconds_shorted = 0;
                  rtdMappings[rtd_index].is_shorted = false;
                  rtdMappings[rtd_index].shorted_oneshot = false;
               }
            }
         }
//          valueFloat = (atof(buf) * 1.8) / 4096; // 12 bit ADC with 1.8V reference

      }
   }

   return rawCount;
}


/*******************************************************************************************/
/*                                                                                         */
/* void *readADCThread(void *)                                                             */
/*                                                                                         */
/* Run this thread once per second to get the current RTD values through the ADC channels. */
/* Also read the current and voltage from the power monitor IC.                            */
/* Runs forever.                                                                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *readADCThread(void *)
{
    struct timeval start;
    struct timeval end;
    uint32_t executionTime;
    uint32_t balance;
    int i;
    int j;

    numThreadsRunning++;
    while(!sigTermReceived)
    {
      (void)gettimeofday(&start, NULL);

      // need to handle setting of the PGA117 analog mux here
      // and which channel of the ADC to read
      for (i = 0; i < NUM_RTDs; i++)
      {
         rtdMappings[i].value = readADCChannel(i);
         if (i < HEATSINK_RTD_INDEX)
         {
            heaterInfo[i].current_temperature = lookupTempFromRawCounts(rtdMappings[i].value, i);
         }
         else if (HEATSINK_RTD_INDEX == i)
         {
            // read the HEATSINK
            heatsinkTemp = lookupTempFromRawCounts(rtdMappings[i].value, i);
            if (heatsinkTemp >= HEATSINK_MAX_TEMP)
            {
               heatsinkOvertempSeconds++;
               if ((heatsinkOvertempSeconds >= HEATSINK_OVERTEMP_TIME_LIMIT_SECONDS) && !heatsinkOvertempOneShot)
               {
                  // for self-preservation of the electronics, we need to
                  // shut down the heaters immediately
                  // and report and log the error
                  heatsinkOvertemp = true;
                  heatsinkOvertempOneShot = true;
                  alarmCode = ALARM_CODE_HEATSINK_OVER_TEMP;

                  // turn all heaters off
                  for (int j = 0; j < NUM_HEATERS; j++)
                  {
                     (void)enableDisableHeater(j, HEATER_DISABLED);
                     (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
                  }

                  syslog(LOG_ERR, "%s ALARM_CODE_HEATSINK_OVER_TEMP temp = %u F", HEATSINK_OVER_TEMP_ERROR_CODE, heatsinkTemp);
                  char descr_str[LOGERROR_DESCR_SIZE];
                  (void)snprintf(descr_str, sizeof(descr_str), "Heatsink over temperature %d F", heatsinkTemp);
                  (void)logError(HEATSINK_OVER_TEMP_ERROR_CODE, "Heat sink", descr_str);
                  syslog(LOG_ERR, "Heaters turned off and disabled");
                  (void)strncpy(errorCodeString, HEATSINK_OVER_TEMP_ERROR_CODE, sizeof(errorCodeString) - 1);
                  systemStatus = SYSTEM_STATUS_ERROR;
               }
            }
            else
            {
               heatsinkOvertemp = false;
               heatsinkOvertempOneShot = false;
               heatsinkOvertempSeconds = 0;
            }
         }
         else
         {
            if (controllerBoardRevision > 0)
            {
               // read the BOARD (PCB, aka AMBIENT) temp sensor
               ambientTemp = lookupTempFromRawCounts(rtdMappings[AMBIENT_TEMP_RTD_INDEX].value, AMBIENT_TEMP_RTD_INDEX);
               if (ambientTemp >= AMBIENT_MAX_TEMP)
               {
                  ambientOvertempSeconds++;
                  if ((ambientOvertempSeconds >= AMBIENT_OVERTEMP_TIME_LIMIT_SECONDS) && !ambientOvertempOneShot)
                  {
                     // for self-preservation of the electronics, we need to
                     // report and log the error
                     ambientOvertemp = true;
                     ambientOvertempOneShot = true;
                     alarmCode = ALARM_CODE_AMBIENT_OVER_TEMP;

                     syslog(LOG_ERR, "%s ALARM_CODE_AMBIENT_OVER_TEMP temp = %u F", AMBIENT_OVER_TEMP_ERROR_CODE, ambientTemp);
                     char descr_str[LOGERROR_DESCR_SIZE];
                     (void)snprintf(descr_str, sizeof(descr_str), "Ambient over temperature %d F", ambientTemp);
                     (void)logError(AMBIENT_OVER_TEMP_ERROR_CODE, "Ambient temp", descr_str);
                     (void)strncpy(errorCodeString, AMBIENT_OVER_TEMP_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                  }
               }
               else
               {
                  ambientOvertemp = false;
                  ambientOvertempOneShot = false;
                  ambientOvertempSeconds = 0;
               }
            }
            else
            {
               // for pre-A02 hardware, substutute the HEATSINK for the AMBIENT temp sensor
               ambientTemp = lookupTempFromRawCounts(rtdMappings[HEATSINK_RTD_INDEX].value, HEATSINK_RTD_INDEX);
            }
         }
      }

      // read the current comsumption and voltage from the power monitor
      // sometimes it takes multiple reads to get a non-zero value
      irms = 0.0;
      for (j = 0; (j < POWER_METER_MAX_READS) && (0.0 == irms); j++)
      {
         (void)atm90e26_irms_get(&irms);
         if (j > 0)
         {
            irms_retries++;
         }
      }
      if (POWER_METER_MAX_READS == j)
      {
         powerMonitorBad = true;
      }

      voltage = 0.0;
      for (j = 0; (j < POWER_METER_MAX_READS) && (0.0 == voltage); j++)
      {
         (void)atm90e26_vrms_get(&voltage);
         if (j > 0)
         {
            vrms_retries++;
         }
      }
      if (POWER_METER_MAX_READS == j)
      {
         powerMonitorBad = true;
      }

      if (debugHeatersPrintf)
      {
         (void)printf("Current draw = %0.2fA  voltage = %0.2fV\n", irms, voltage);
         for (int i = 0; i < NUM_RTDs; i++)
         {
            (void)printf("ADC channel [%d]  counts = %d  temp = %dF\n", i, rtdMappings[i].value, lookupTempFromRawCounts(rtdMappings[i].value, i));
         }

         (void)printf("\n");
      }

      (void)gettimeofday(&end, NULL);

      executionTime = (((end.tv_sec - start.tv_sec) * ONE_SECOND_IN_MICROSECONDS) + (end.tv_usec - start.tv_usec));
      if (executionTime < ONE_SECOND_IN_MICROSECONDS)
      {
         balance = ONE_SECOND_IN_MICROSECONDS - executionTime;
//      printf("task execution took %d microseconds balance = %ld\n", executionTime, balance);
         usleep(balance);
      }
      else
      {
         // force a minimum thread sleep time
//         printf("Execution time took more than 1 second - %d.%d\n", (executionTime / ONE_SECOND_IN_MICROSECONDS), (executionTime % ONE_SECOND_IN_MICROSECONDS));
         usleep(MINIMUM_THREAD_SLEEP_TIME);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


// unused GPIO code
/*******************************************************************************************/
/*                                                                                         */
/* int initUnusedGPIOs()                                                                   */
/*                                                                                         */
/* Initialize the unused GPIOs. Set them all to inputs per the schematic.                  */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initUnusedGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, UNUSED_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


// heater code
/*******************************************************************************************/
/*                                                                                         */
/* int initHeaterGPIOs()                                                                   */
/*                                                                                         */
/* Initialize the GPIOs for managing the heaters.                                          */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initHeaterGPIOs()
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(commandLine, HEATER_GPIO_INIT_SCRIPT, sizeof(commandLine));
   int ret = system(commandLine);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int enableDisableHeater(int heaterIndex, bool enabled)                                  */
/*                                                                                         */
/* Turns the requested heater on/off                                                       */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int enableDisableHeater(int heaterIndex, bool enabled)
{
   int ret = 0;

   if ((heaterIndex >= SLOT1_TOP_HEATER_INDEX) && (heaterIndex <= SLOT6_BOTTOM_HEATER_INDEX))
   {
      // if the index is in range, set is_enabled to the specified value
      heaterInfo[heaterIndex].is_enabled = enabled;
      if (enabled)
      {
         // if we're enabling a heater, reset error states
         // they will be checked the next time the heater algorithm runs
         rtdMappings[heaterIndex].is_shorted = false;
         rtdMappings[heaterIndex].is_open    = false;
         heaterInfo[heaterIndex].is_undertemp = false;
         heaterInfo[heaterIndex].is_overtemp = false;
         heaterInfo[heaterIndex].is_undertemp_CSS = false;
         heaterInfo[heaterIndex].is_overtemp_CSS = false;
         heaterInfo[heaterIndex].overtemp_oneshot = false;
         heaterInfo[heaterIndex].undertemp_oneshot = false;
         rtdMappings[heaterIndex].shorted_oneshot = false;
         rtdMappings[heaterIndex].open_oneshot = false;
         heaterInfo[heaterIndex].eco_mode_on = false;
         heaterInfo[heaterIndex].seconds_undertemp = 0;
         heaterInfo[heaterIndex].seconds_overtemp = 0;
         rtdMappings[heaterIndex].seconds_shorted = 0;
         rtdMappings[heaterIndex].seconds_open = 0;
      }
   }
   else
   {
      // bad index
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int turnHeaterOnOff(int heaterIndex, uhc::HeaterState on)                               */
/*                                                                                         */
/* Turns the requested heater on/off                                                       */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int turnHeaterOnOff(int heaterIndex, uhc::HeaterState on)
{
   int ret = 0;

   if ((heaterIndex >= SLOT1_TOP_HEATER_INDEX) && (heaterIndex <= SLOT6_BOTTOM_HEATER_INDEX))
   {
      // if the index is in range, set heater_on to the specified value
      int fd_on = heaterInfo[heaterIndex].fd;
      int bytesWritten = 0;
      heaterInfo[heaterIndex].state = on;

      if (HEATER_STATE_ON == on)
      {
         bytesWritten = write(fd_on, HEATER_ON_STRING, 1);
         if (debugHeatersPrintf)
         {
            (void)printf("Heater %d ON\n", heaterIndex);
         }
      }
      else
      {
         bytesWritten = write(fd_on, HEATER_OFF_STRING, 1);
         if (debugHeatersPrintf)
         {
            (void)printf("Heater %d OFF\n", heaterIndex);
         }
      }

      if (-1 == bytesWritten)
      {
         if (debugPrintf)
         {
            (void)printf("Error writing to %s\n", heaterInfo[heaterIndex].GPIO_PATH);
         }

         ret = 1;
      }
   }
   else
   {
      // bad index
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int setHeaterTimesIndex(int heaterIndex,                                                */
/*                         google::protobuf::Timestamp startTime,                          */
/*                         google::protobuf::Timestamp endTime)                            */
/*                                                                                         */
/* Set the heater start and end times to the specified values.                             */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int setHeaterTimesIndex(int heaterIndex, google::protobuf::Timestamp startTime, google::protobuf::Timestamp endTime)
{
   int ret = 0;

   if ((heaterIndex >= SLOT1_TOP_HEATER_INDEX) && (heaterIndex <= SLOT6_BOTTOM_HEATER_INDEX))
   {
      // if the index is in range, set start and end times to the specified value
      heaterInfo[heaterIndex].start_time = startTime;
      heaterInfo[heaterIndex].end_time = endTime;
   }
   else
   {
      // bad index
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int setHeaterTimesSlot(int slot,                                                        */
/*                         google::protobuf::Timestamp startTime,                          */
/*                         google::protobuf::Timestamp endTime)                            */
/*                                                                                         */
/* Set the heater start and end times to the specified values for a given slot.            */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int setHeaterTimesSlot(int slot, google::protobuf::Timestamp startTime, google::protobuf::Timestamp endTime)
{
   int ret = 0;

   if ((slot >= SLOT1) && (slot <= SLOT6))
   {
      // SLOTS are 1 based; the index needs to be zero based.
      slot -= 1;

      // if the index is in range, set start and end times to the specified value
      // set the upper heater times
      heaterInfo[(slot*2)].start_time = startTime;
      heaterInfo[(slot*2)].end_time = endTime;

      // set the upper heater times
      heaterInfo[(slot*2)+1].start_time = startTime;
      heaterInfo[(slot*2)+1].end_time = endTime;
   }
   else
   {
      // bad index
      ret = 1;
   }

   return ret;
}
/*******************************************************************************************/
/*                                                                                         */
/* int initHeaterDataStructures()                                                          */
/*                                                                                         */
/* Initialize the data structures for managing the heaters.                                */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initHeaterDataStructures()
{
   (void)memset(heaterInfo, 0, sizeof(heaterInfo));

   ::google::protobuf::Timestamp* timestamp = new ::google::protobuf::Timestamp();
   struct timeval now;
   (void)gettimeofday(&now, NULL);
   timestamp->set_seconds(now.tv_sec);
   timestamp->set_nanos(now.tv_usec * 1000);

   int i = 0;
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT1_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 1
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT1_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 2
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT2_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 3
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT2_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 4
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT3_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 5
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT3_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 6
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT4_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 7
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT4_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 8
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT5_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 9
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT5_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 10
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT6_TOP_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   i++;  // 11
   heaterInfo[i].state = HEATER_OFF;
   heaterInfo[i].location = (i % 2) ? HEATER_LOCATION_LOWER : HEATER_LOCATION_UPPER;
   heaterInfo[i].temperature_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].saved_setpoint = DEFAULT_SETPOINT;
   heaterInfo[i].cleaning_mode_setpoint = CLEANING_MODE_SETPOINT;
   heaterInfo[i].eco_mode_setpoint = DEFAULT_ECO_MODE_SETPOINT;
   heaterInfo[i].current_temperature = lookupTempFromRawCounts(readADCChannel(i), i);
   (void)strncpy(heaterInfo[i].GPIO_PATH, SLOT6_BOTTOM_HEATER, sizeof(heaterInfo[i].GPIO_PATH));
   heaterInfo[i].fd = open(heaterInfo[i].GPIO_PATH, O_WRONLY);
   heaterInfo[i].is_enabled = false;
   heaterInfo[i].is_on = false;
   heaterInfo[i].was_on = false;
   heaterInfo[i].is_undertemp = false;
   heaterInfo[i].is_overtemp = false;
   heaterInfo[i].is_undertemp_CSS = false;
   heaterInfo[i].is_overtemp_CSS = false;
   heaterInfo[i].overtemp_oneshot = false;
   heaterInfo[i].undertemp_oneshot = false;
   heaterInfo[i].eco_mode_on = false;
   heaterInfo[i].setpoint_changed = false;
   heaterInfo[i].eco_mode_is_on = false;
   heaterInfo[i].seconds_undertemp = 0;
   heaterInfo[i].seconds_overtemp = 0;
   heaterInfo[i].seconds_on_time = 0;
   heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
   heaterInfo[i].start_time.set_seconds(timestamp->seconds());
   heaterInfo[i].start_time.set_nanos(0);
   heaterInfo[i].end_time.set_seconds(timestamp->seconds());
   heaterInfo[i].end_time.set_nanos(0);

   return 0;
}


/*******************************************************************************************/
/*                                                                                         */
/* int setSetpointHeater(int channelIndex, uint32_t setpoint)                              */
/*                                                                                         */
/* Set the temperature setpoint for a specific heater.                                     */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
int setSetpointHeater(int channelIndex, uint32_t setpoint)
{
   int ret = 0;

   if ((channelIndex >= SLOT1_TOP_HEATER_INDEX) && (channelIndex <= SLOT6_BOTTOM_HEATER_INDEX))
   {
      // if the index is in range, set the setpoint to the specified value
      heaterInfo[channelIndex].temperature_setpoint = (uint16_t)setpoint;
      heaterInfo[channelIndex].setpoint_changed = true;
      ret = 0;
   }
   else
   {
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void setSetpointSlot(int slotNumber, uint32_ setpoint)                                  */
/*                                                                                         */
/* Set the temperature setpoint for a slot. Since there are two heaters/RTDs per slot,     */
/* set both setpoints to the same value.                                                   */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void setSetpointSlot(int slotNumber, uint32_t setpoint)
{
    // when you set a setpoint for a slot, both the upper and lower heaters
    // get the same setpoint
    switch (slotNumber)
    {
        case SLOT1:
            heaterInfo[SLOT1_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT1_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT1_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        case SLOT2:
            heaterInfo[SLOT2_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT2_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT2_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        case SLOT3:
            heaterInfo[SLOT3_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT3_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT3_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        case SLOT4:
            heaterInfo[SLOT4_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT4_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT4_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        case SLOT5:
            heaterInfo[SLOT5_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT5_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT5_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        case SLOT6:
            heaterInfo[SLOT6_TOP_HEATER_INDEX].temperature_setpoint     = (uint16_t)setpoint;
            heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].temperature_setpoint  = (uint16_t)setpoint;
            heaterInfo[SLOT6_TOP_HEATER_INDEX].saved_setpoint           = (uint16_t)setpoint;
            heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].saved_setpoint        = (uint16_t)setpoint;
            heaterInfo[SLOT6_TOP_HEATER_INDEX].setpoint_changed = true;
            heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].setpoint_changed = true;
            break;

        default:
            break;
    }
}


/*******************************************************************************************/
/*                                                                                         */
/* void lookupHeaterIndices(int slotNumber,                                                */
/*                          int *channelIndexTopHeater,                                    */
/*                          int *channelIndexBottomHeater)                                 */
/*                                                                                         */
/* Look up the heater indices for a give slot number.                                      */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void lookupHeaterIndices(int slotNumber, int *channelIndexTopHeater, int *channelIndexBottomHeater)
{
   switch (slotNumber)
   {
      case SLOT1:
         *channelIndexTopHeater = SLOT1_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT1_BOTTOM_HEATER_INDEX;
         break;

      case SLOT2:
         *channelIndexTopHeater = SLOT2_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT2_BOTTOM_HEATER_INDEX;
         break;

      case SLOT3:
         *channelIndexTopHeater = SLOT3_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT3_BOTTOM_HEATER_INDEX;
         break;

      case SLOT4:
         *channelIndexTopHeater = SLOT4_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT4_BOTTOM_HEATER_INDEX;
         break;

      case SLOT5:
         *channelIndexTopHeater = SLOT5_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT5_BOTTOM_HEATER_INDEX;
         break;

      case SLOT6:
      default:
         *channelIndexTopHeater = SLOT6_TOP_HEATER_INDEX;
         *channelIndexBottomHeater = SLOT6_BOTTOM_HEATER_INDEX;
         break;
   }
}


/*******************************************************************************************/
/*                                                                                         */
/* void heatersAtStartup(int maxHeatersOn)                                                 */
/*                                                                                         */
/* This is the heater startup algorithm. Attempt the fastest possible startup by keeping   */
/* the most heaters on that we can. This will turn on the bottom heaters of all 6 slots    */
/* and the top heaters of slot 1 and 6, and if possible, up to 2 more upper heaters based  */
/* on the line voltage.                                                                    */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void heatersAtStartup(int maxHeatersOn)
{
   int i = 0;
   int numHeatersOn = 0;

   // first turn everything off so we don't blow the power budget
   for (i = 0; i < NUM_HEATERS; i++)
   {
      (void)turnHeaterOnOff(i, HEATER_STATE_OFF);
      heaterInfo[i].is_on = false;
      heaterInfo[i].was_on = false;
   }

   // check all lower heaters first
   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT1_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT1_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT2_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT2_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT3_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT3_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT4_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT4_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT5_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT5_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].is_enabled && (heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].current_temperature < heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT6_BOTTOM_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].was_on = heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].is_on;
      heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT6_BOTTOM_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   // now, check the upper heaters; turn on as many as we can
   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT6_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT6_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT6_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT6_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT6_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT6_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT6_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT6_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT1_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT1_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT1_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT1_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT1_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT1_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT1_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT1_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT5_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT5_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT5_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT5_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT5_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT5_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT5_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT5_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT2_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT2_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT2_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT2_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT2_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT2_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT2_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT2_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT3_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT3_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT3_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT3_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT3_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT3_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT3_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT3_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   if ((numHeatersOn < maxHeatersOn) && heaterInfo[SLOT4_TOP_HEATER_INDEX].is_enabled && (heaterInfo[SLOT4_TOP_HEATER_INDEX].current_temperature < heaterInfo[SLOT4_TOP_HEATER_INDEX].temperature_setpoint))
   {
      (void)turnHeaterOnOff(SLOT4_TOP_HEATER_INDEX, HEATER_STATE_ON);
      heaterInfo[SLOT4_TOP_HEATER_INDEX].was_on = heaterInfo[SLOT4_TOP_HEATER_INDEX].is_on;
      heaterInfo[SLOT4_TOP_HEATER_INDEX].is_on = true;
      heaterInfo[SLOT4_TOP_HEATER_INDEX].seconds_on_time++;
      numHeatersOn++;
   }

   int numberOfHeatersAtTemp = 0;
   int numberOfHeatersEnabled = 0;
   for (i = 0; i < NUM_HEATERS; i++)
   {
      if (heaterInfo[i].is_enabled)
      {
         numberOfHeatersEnabled++;
      }
      if (heaterInfo[i].is_enabled && ((HEATER_LOCATION_UPPER == heaterInfo[i].location) && (heaterInfo[i].current_temperature >= (heaterInfo[i].temperature_setpoint - 10))))
      {
         numberOfHeatersAtTemp++;
      }
      if (heaterInfo[i].is_enabled && ((HEATER_LOCATION_LOWER == heaterInfo[i].location) && (heaterInfo[i].current_temperature >= heaterInfo[i].temperature_setpoint)))
      {
         numberOfHeatersAtTemp++;
      }
   }
 printf("numberOfHeatersEnabled = %d, numberOfHeatersAtTemp = %d\n", numberOfHeatersEnabled, numberOfHeatersAtTemp);
   if ((numberOfHeatersEnabled == numberOfHeatersAtTemp) || (startupTimeSeconds >= MAX_STARTUP_REACH_SETPOINT_TIME))
   {
printf("initialStartup is COMPLETE or startup time exceeded. Should resume regular heater algorithm\n");
      initialStartup = false;
   }
}


/*******************************************************************************************/
/*                                                                                         */
/* void runHeaterAlgorithm()                                                               */
/*                                                                                         */
/* Once per second, run the heater control algorithm. Figure out the eight heaters that    */
/* are the farthest from their setpoints, and turn on their heaters. Ones that were on     */
/* but won't be, turn them off before turning on any heaters. This eliminates the chance   */
/* of having more than eight heaters on at any given time. Turn off a heater if it is      */
/* above the setpoint.                                                                     */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void runHeaterAlgorithm()
{
   // based on the data provided by HennyPenny, their custom Watlow
   // heaters are rated for 400W at 240V, thus drawing a max of 1.66A
   // With 1.2A consumed by the 12V power supply, we
   // have 13.8A to work with to power the heaters and stay within
   // the 15A from the wall outlet limit.
   // At 1.66A each, we can safely run 8 heaters, since
   // 8 * 1.66A = 13.28A
   // 13.28A + 1.2A < 15A

   // check if any heaters are at or above their setpoints
   // by calculating their current delta_temp
   // if the delta is <= 0, turn those heaters off
   // if the delta is > 0 but <= 5, that heater may need to turned on.
   // go through all of the heaters and find the top 8 deltas
   // then turn their heaters on.
   // but first, turn off any heaters that are transitioning to off so that
   // we don't violate the power budget

   float volts = 0.0;
   (void)atm90e26_vrms_get(&volts);

   // default to 8 and 4 if we can't get the line voltage
   int maxHeatersOn = 8;
   int startingIndex = 4;

   if (volts > 0.0)
   {
      if (volts <= 201.0)
      {
         maxHeatersOn = 10;
         startingIndex = 2;
      }
      else if ((volts > 201.0) && (volts <= 221.0))
      {
         maxHeatersOn = 9;
         startingIndex = 3;
      }
      else
      {
         maxHeatersOn = 8;
         startingIndex = 4;
      }
   }

   if (initialStartup)
   {
      regularHeaterAlgorithmOneShot = true;

      if (!startupMessageReceived)
      {
         // do not start the fastest heating possible algorithm
         // until the HMI sends the STARTUP command.
         return;
      }
      // Turn on all 6 lower heaters and at least two uppers 100% duty cycle until startup is complete,
      // then resume the normal algorithm
      startupTimeSeconds++;
#if (0)
      for (int i = 0; i < NUM_HEATERS; i++)
      {
         if (!heaterInfo[i].is_enabled)
         {
            // if the user has turned off ANY heater
            // abort the startup algorithm and let the regular algorithm take over.
            initialStartup = false;
            return;
         }
      }
#endif
      // run the heater startup algorithm
      heatersAtStartup(maxHeatersOn);
   }
   else
   {
      if (regularHeaterAlgorithmOneShot)
      {
         printf("Resuming regular heater algorithm\n");
         regularHeaterAlgorithmOneShot = false;
      }
      // normal operating mode heater algorithm
      (void)memset(tempDeltas, 0, sizeof(tempDeltas));
      for (int i = 0; i < NUM_HEATERS; i++)
      {
         if (!heaterInfo[i].is_enabled)
         {
            // if a heater is not enabled, make sure it's off
            // and set the delta to MAX_NEGATIVE_DELTA_TEMP so that it's shuffled to the end of the line
            // a negative delta temp means the temp is above the setpoint
            heaterInfo[i].delta_temp = MAX_NEGATIVE_DELTA_TEMP;
            tempDeltas[i] = heaterInfo[i].delta_temp;
            heaterInfo[i].was_on = false;
            heaterInfo[i].is_on = false;
            (void)turnHeaterOnOff(i, HEATER_STATE_OFF);
         }
         else
         {
            // readADCThread is the single point of contact for reading the RTDs
            // so it updates the current temperature in the heater info structure
            // a negative delta temp means the temp is above the setpoint
            heaterInfo[i].delta_temp = heaterInfo[i].temperature_setpoint - heaterInfo[i].current_temperature;
            tempDeltas[i] = heaterInfo[i].delta_temp;
            heaterInfo[i].was_on = heaterInfo[i].is_on;
            heaterInfo[i].is_on = false;
            if (startupComplete && !heaterInfo[i].setpoint_changed)
            {
               // only perform undertemp or overtemp limit checking once startup is complete
               // otherwise, virtually all slots will generate an undertemp limit alarm
               if (heaterInfo[i].current_temperature < (heaterInfo[i].temperature_setpoint - UNDERTEMP_DELTA_LIMIT_DEGREES))
               {
                  heaterInfo[i].is_undertemp = true;
               }
               else
               {
                  heaterInfo[i].is_undertemp = false;
               }

               if (heaterInfo[i].current_temperature > (heaterInfo[i].temperature_setpoint + OVERTEMP_DELTA_LIMIT_DEGREES))
               {
                  heaterInfo[i].is_overtemp = true;
               }
               else
               {
                  heaterInfo[i].is_overtemp = false;
               }

               int lowerHeaterThisShelf = 0;
               int upperHeaterThisShelf = 0;
               int shelfNumber = 0;

               if (heaterInfo[i].is_undertemp && heaterInfo[i].is_enabled && !heaterInfo[i].setpoint_changed)
               {
                  heaterInfo[i].seconds_undertemp++;
                  if ((heaterInfo[i].seconds_undertemp > UNDERTEMP_DELTA_LIMIT_SECONDS) && !heaterInfo[i].undertemp_oneshot)
                  {
                     if (debugHeatersPrintf)
                     {
                        (void)printf("Issue undertemp alarm for heater %d\n", i+1);
                     }

                     heaterInfo[i].undertemp_oneshot = true;
                     heaterInfo[i].is_enabled = false;
                     heaterInfo[i].is_undertemp_CSS = true;

                     // according to FRON-246, if a heater is undertemp the SHELF is disabled, not just the single heater
                     if (HEATER_LOCATION_LOWER == heaterInfo[i].location)
                     {
                        // this is a bottom/lower heater, also disable the corresponding top/upper heater
                        upperHeaterThisShelf = i - 1;
                        lowerHeaterThisShelf = i;
                        heaterInfo[i-1].is_enabled = false;
                     }
                     else
                     {
                        // this is an upper/top heater, also disable the corresponding bottom/lower heater
                        lowerHeaterThisShelf = i + 1;
                        upperHeaterThisShelf = i;
                        heaterInfo[i+1].is_enabled = false;
                     }

                     (void)turnHeaterOnOff(lowerHeaterThisShelf, HEATER_STATE_OFF);
                     (void)turnHeaterOnOff(upperHeaterThisShelf, HEATER_STATE_OFF);

                     switch (i)
                     {
                        default:
                        case SLOT1_TOP_HEATER_INDEX:     // NOSONAR
                        case SLOT1_BOTTOM_HEATER_INDEX:  // NOSONAR
                        // These cases are not redundant.
                           shelfNumber = SLOT1;
                           break;

                        case SLOT2_TOP_HEATER_INDEX:
                        case SLOT2_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT2;
                           break;

                        case SLOT3_TOP_HEATER_INDEX:
                        case SLOT3_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT3;
                           break;

                        case SLOT4_TOP_HEATER_INDEX:
                        case SLOT4_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT4;
                           break;

                        case SLOT5_TOP_HEATER_INDEX:
                        case SLOT5_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT5;
                           break;

                        case SLOT6_TOP_HEATER_INDEX:
                        case SLOT6_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT6;
                           break;
                     }

                     syslog(LOG_ERR, "%s Undertemp alarm for shelf %d, upper heater %d and lower heater %d disabled and turned off", SHELF_UNDER_TEMP_ERROR_CODE, shelfNumber, upperHeaterThisShelf, lowerHeaterThisShelf);
                     (void)strncpy(errorCodeString, SHELF_UNDER_TEMP_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     char location_str[LOGERROR_LOCATION_SIZE];
                     (void)snprintf(location_str, sizeof(location_str), "Shelf %d", shelfNumber);
                     (void)logError(SHELF_UNDER_TEMP_ERROR_CODE, location_str, "Undertemp alarm");
                  }
               }
               else
               {
                  heaterInfo[i].undertemp_oneshot = false;
                  heaterInfo[i].seconds_undertemp = 0;
               }

               if (heaterInfo[i].is_overtemp && heaterInfo[i].is_enabled && !inCleaningMode && !heaterInfo[i].setpoint_changed)
               {
                  heaterInfo[i].seconds_overtemp++;
                  if ((heaterInfo[i].seconds_overtemp > OVERTEMP_DELTA_LIMIT_SECONDS) && !heaterInfo[i].overtemp_oneshot)
                  {
                     if (debugHeatersPrintf)
                     {
                        (void)printf("Issue overtemp alarm for heater %d\n", i);
                     }

                     heaterInfo[i].overtemp_oneshot = true;
                     heaterInfo[i].is_overtemp_CSS = true;
                     heaterInfo[i].is_enabled = false;

                     // according to FRON-246, if a heater is overtemp the SHELF is disabled, not just the single heater
                     if (HEATER_LOCATION_LOWER == heaterInfo[i].location)
                     {
                        // this is a bottom/lower heater, also disable the corresponding top/upper heater
                        upperHeaterThisShelf = i - 1;
                        lowerHeaterThisShelf = i;
                        heaterInfo[i-1].is_enabled = false;
                     }
                     else
                     {
                        // this is an upper/top heater, also disable the corresponding bottom/lower heater
                        lowerHeaterThisShelf = i + 1;
                        upperHeaterThisShelf = i;
                        heaterInfo[i+1].is_enabled = false;
                     }

                     (void)turnHeaterOnOff(lowerHeaterThisShelf, HEATER_STATE_OFF);
                     (void)turnHeaterOnOff(upperHeaterThisShelf, HEATER_STATE_OFF);

                     int shelfNumber = 0;
                     switch (i)
                     {
                        default:
                        case SLOT1_TOP_HEATER_INDEX:     // NOSONAR
                        case SLOT1_BOTTOM_HEATER_INDEX:  // NOSONAR
                        // These cases are not redundant.
                           shelfNumber = SLOT1;
                           break;

                        case SLOT2_TOP_HEATER_INDEX:
                        case SLOT2_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT2;
                           break;

                        case SLOT3_TOP_HEATER_INDEX:
                        case SLOT3_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT3;
                           break;

                        case SLOT4_TOP_HEATER_INDEX:
                        case SLOT4_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT4;
                           break;

                        case SLOT5_TOP_HEATER_INDEX:
                        case SLOT5_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT5;
                           break;

                        case SLOT6_TOP_HEATER_INDEX:
                        case SLOT6_BOTTOM_HEATER_INDEX:
                           shelfNumber = SLOT6;
                           break;
                     }

                     syslog(LOG_ERR, "%s Overtemp alarm for shelf %d, upper heater %d and lower heater %d disabled and turned off", SHELF_OVER_TEMP_ERROR_CODE, shelfNumber, upperHeaterThisShelf, lowerHeaterThisShelf);
                     (void)strncpy(errorCodeString, SHELF_OVER_TEMP_ERROR_CODE, sizeof(errorCodeString) - 1);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     char location_str[LOGERROR_LOCATION_SIZE];
                     (void)snprintf(location_str, sizeof(location_str), "Shelf %d", shelfNumber);
                     (void)logError(SHELF_OVER_TEMP_ERROR_CODE, location_str, "Overtemp alarm");
                  }
               }
               else
               {
                  heaterInfo[i].overtemp_oneshot = false;
                  heaterInfo[i].seconds_overtemp = 0;
               }
            }
         }
      }

      selectionSort(tempDeltas, NUM_HEATERS);
      int totalHeatersOn = 0;
      bool found = false;

      // the array is assorted in ascending order, so the last maxHeatersOn are the deltas we care about
      for (int j = startingIndex; j < NUM_HEATERS; j++)
      {
         // for the maxHeatersOn highest deltas, find the corresponding heater
         found = false;
         for (int i = 0; (i < NUM_HEATERS) && !found && (totalHeatersOn < maxHeatersOn); i++)
         {
            if ((tempDeltas[j] == heaterInfo[i].delta_temp) && (tempDeltas[j] > 0))
            {
               // mark the heater state on
               heaterInfo[i].is_on = true;
               // zero the delta to handle multiple deltas of the same value
               heaterInfo[i].delta_temp = 0;
               totalHeatersOn++;
               found = true;
            }
         }
      }

      // first, turn off any heaters that were on but won't be this time interval
      // so that we don't violate the power budget
      // that way, if we turn on heaters early in the list but later ones in the list are still on
      // we turn them off first
      // at this point, is_on is the next state
      for (int i = 0; i < NUM_HEATERS; i++)
      {
         if (heaterInfo[i].was_on && !heaterInfo[i].is_on)
         {
            (void)turnHeaterOnOff(i, HEATER_STATE_OFF);
         }
      }

      // now, actually change the heater state
      int totalHeatersCurrentlyOn = 0;
      for (int i = 0; i < NUM_HEATERS; i++)
      {
         if (heaterInfo[i].is_on)
         {
            (void)turnHeaterOnOff(i, HEATER_STATE_ON);
            totalHeatersCurrentlyOn++;
            heaterInfo[i].seconds_on_time++;
         }
         else
         {
            (void)turnHeaterOnOff(i, HEATER_STATE_OFF);
         }
      }

      if (debugHeatersPrintf)
      {
         (void)printf("totalHeatersCurrentlyOn = %d\n", totalHeatersCurrentlyOn);
      }

      // once all the heaters are up to temperature
      int totalHeatersInTempRange = 0;
      int totalHeatersEnabled = 0;
      for (int i = 0; i < NUM_HEATERS; i++)
      {
         if (heaterInfo[i].is_enabled)
         {
            totalHeatersEnabled++;
         }
         if (heaterInfo[i].is_enabled && ((HEATER_LOCATION_UPPER == heaterInfo[i].location) && (heaterInfo[i].current_temperature >= (heaterInfo[i].temperature_setpoint - 10))))
         {
            totalHeatersInTempRange++;
         }
         if (heaterInfo[i].is_enabled && ((HEATER_LOCATION_LOWER == heaterInfo[i].location) && (heaterInfo[i].current_temperature >= heaterInfo[i].temperature_setpoint)))
         {
            totalHeatersInTempRange++;
         }
      }

      for (int i = 0; i < NUM_HEATERS; i++)
      {
         // if we're now within the legal +/- 5F range
         // clear the setpoint_changed flag
         // this was done to avoid over or under temp errors when a large change in the setpoint
         // has occurred, since we have no way to force a slot to cool down any faster
         // than turning the heaters off.
         if ((heaterInfo[i].current_temperature >= (heaterInfo[i].temperature_setpoint - SETPOINT_RANGE_PLUS_MINUS))
         && (heaterInfo[i].current_temperature <= (heaterInfo[i].temperature_setpoint + SETPOINT_RANGE_PLUS_MINUS)))
         {
            heaterInfo[i].setpoint_changed = false;
         }
      }

      if (totalHeatersEnabled && (totalHeatersEnabled == totalHeatersInTempRange))
      {
         if (!startupComplete)
         {
            syslog(LOG_NOTICE, "Startup complete - all heaters are within range of setpoint, startup time = %d", startupTimeSeconds/60);
            (void)logInternalEvent(STARTUP_COMPLETE_T);
            if (debugHeatersPrintf)
            {
               (void)printf("Startup complete - all heaters are within range of setpoint, startup time = %d\n", startupTimeSeconds/60);
            }
         }

         if (debugHeatersPrintf)
         {
            (void)printf("ALL HEATERS ARE AT TEMPERATURE!\n");
         }

         startupComplete = true;
         initialStartup = false;
         systemStatus = SYSTEM_STATUS_STARTUP_COMPLETE;
      }

      if (startupComplete)
      {
         if (startupTimeSeconds >= MAX_STARTUP_REACH_SETPOINT_TIME)
         {
            if (maxStartupTimeExceededOneShot)
            {
               // log the error
               syslog(LOG_ERR, "%s Startup not completed in 25 minutes +/- 5 minutes = %d", STARTUP_TIME_EXCEEDED_ERROR_CODE, startupTimeSeconds/60);
               (void)strncpy(errorCodeString, STARTUP_TIME_EXCEEDED_ERROR_CODE, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "Startup not completed on time, currently %d minutes", startupTimeSeconds / 60);
               (void)logError(STARTUP_TIME_EXCEEDED_ERROR_CODE, "System", descr_str);
               maxStartupTimeExceededOneShot = false;
            }
         }
         startupTimeSeconds = 0;
      }
      else
      {
         startupTimeSeconds++;
         if (startupTimeSeconds >= MAX_STARTUP_REACH_SETPOINT_TIME)
         {
            if (maxStartupTimeExceededOneShot)
            {
               // log the error
               syslog(LOG_ERR, "%s Startup not completed in 25 minutes +/- 5 minutes = %d", STARTUP_TIME_EXCEEDED_ERROR_CODE, startupTimeSeconds/60);
               (void)strncpy(errorCodeString, STARTUP_TIME_EXCEEDED_ERROR_CODE, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "Startup not completed on time, currently %d minutes", startupTimeSeconds / 60);
               (void)logError(STARTUP_TIME_EXCEEDED_ERROR_CODE, "System", descr_str);
               maxStartupTimeExceededOneShot = false;
            }
         }
      }
   }
   // check if the console write to CSV file is enabled, we're in startup, and we haven't timed out
   if ((access(HEATER_DATA_TRIGGER_FILE, F_OK) == 0) && !startupComplete && (startupTimeSeconds <= MAX_STARTUP_REACH_SETPOINT_TIME))
   {
      if (debugCSV)
      {
         (void)printf("logging trigger file exists startup NOT complete\n");
      }

      if (startupMessageReceived)
      {
         if (debugCSV)
         {
            (void)printf("logging startupMessageReceived\n");
         }

         debugHeatersWriteCSVFile = true;

         if (csvFile == NULL)
         {
            csvFile = fopen(HEATER_DATA_CSV_FILE, "w");
            (void)printf("csvFile = %p\n", csvFile);
            (void)fprintf(csvFile, "SECONDS, HEATER1, HEATER1ON, HEATER2, HEATER2ON, HEATER3, HEATER3ON, HEATER4, HEATER4ON, HEATER5, HEATER5ON, HEATER6, HEATER6ON, HEATER7, HEATER7ON, HEATER8, HEATER8ON, HEATER9, HEATER9ON, HEATER10, HEATER10ON, HEATER11, HEATER11ON, HEATER12, HEATER12ON, HEATSINK, AMBIENT, CURRENTDRAW, VOLTAGE\n");
            if (debugCSV)
            {
               (void)printf("logging created /tmp/heaterData.csv and wrote the header\n");
            }
            (void)gettimeofday(&csvStartTime, NULL);
            loggingLinesWritten = 0;
         }

         if (csvFile != NULL)
         {
            loggingLinesWritten++;
            if (debugCSV)
            {
               (void)printf("logging writing line %u of heater data\n", loggingLinesWritten);
            }

            struct timeval now;
            (void)gettimeofday(&now, NULL);
            (void)fprintf(csvFile, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %0.2f, %0.2f\n", loggingLinesWritten,
                  heaterInfo[0].current_temperature,  heaterInfo[0].is_on,
                  heaterInfo[1].current_temperature,  heaterInfo[1].is_on,
                  heaterInfo[2].current_temperature,  heaterInfo[2].is_on,
                  heaterInfo[3].current_temperature,  heaterInfo[3].is_on,
                  heaterInfo[4].current_temperature,  heaterInfo[4].is_on,
                  heaterInfo[5].current_temperature,  heaterInfo[5].is_on,
                  heaterInfo[6].current_temperature,  heaterInfo[6].is_on,
                  heaterInfo[7].current_temperature,  heaterInfo[7].is_on,
                  heaterInfo[8].current_temperature,  heaterInfo[8].is_on,
                  heaterInfo[9].current_temperature,  heaterInfo[9].is_on,
                  heaterInfo[10].current_temperature, heaterInfo[10].is_on,
                  heaterInfo[11].current_temperature, heaterInfo[11].is_on,
                  lookupTempFromRawCounts(rtdMappings[HEATSINK_RTD_INDEX].value, HEATSINK_RTD_INDEX),
                  lookupTempFromRawCounts(rtdMappings[AMBIENT_TEMP_RTD_INDEX].value, AMBIENT_TEMP_RTD_INDEX),
                  irms, voltage);
         }
      }
   }
   else
   {
      if (!startupComplete && debugCSV)
      {
         (void)printf("logging trigger file removed\n");
      }

      debugHeatersWriteCSVFile = false;
      if (csvFile != NULL)
      {
         (void)fclose(csvFile);
         sync();
         csvFile = NULL;

         char commandLine[COMMAND_LINE_BUFFER_SIZE];
         // Make sure targetFile appended to "mv /tmp/heaterData.csv " won't exceed COMMAND_LINE_BUFFER_SIZE.
         char targetFile[MAX_FILE_PATH - 24];
         (void)memset(targetFile, 0, sizeof(targetFile));
         int fileCount = getFileCount();
         if (0 != fileCount)
         {
            char oldestFileName[MAX_FILE_PATH];
            char newestFilename[MAX_FILE_PATH];
            (void)memset(oldestFileName, 0, sizeof(oldestFileName));
            (void)memset(newestFilename, 0, sizeof(newestFilename));
            (void)getNewestFile(newestFilename, sizeof(newestFilename));
            (void)printf("newestFilename = \"%s\"\n", newestFilename);
            (void)getOldestFile(oldestFileName, sizeof(oldestFileName));
            (void)printf("oldestFileName = \"%s\"\n", oldestFileName);
         }

         (void)memset(commandLine, 0, sizeof(commandLine));
         (void)getNextFilename(targetFile, sizeof(targetFile));
         (void)printf("logging moving %s to %s\n", HEATER_DATA_CSV_FILE, targetFile);
         (void)snprintf(commandLine, sizeof(commandLine) - 1, MOVE_HEATER_DATA_TO_LOG_DIRECTORY, HEATER_DATA_CSV_FILE, targetFile);
         (void)system(commandLine);

         (void)memset(commandLine, 0, sizeof(commandLine));
         (void)snprintf(commandLine, sizeof(commandLine) - 1, TOUCH_FILE, targetFile);
         (void)system(commandLine);
         sync();

         startupMessageReceived = false;
      }
      if (startupTimeSeconds >= MAX_STARTUP_REACH_SETPOINT_TIME)
      {
         if (maxStartupTimeExceededOneShot)
         {
            // log the error
            syslog(LOG_ERR, "%s Startup not completed in 25 minutes +/- 5 minutes = %d", STARTUP_TIME_EXCEEDED_ERROR_CODE, startupTimeSeconds/60);
            (void)strncpy(errorCodeString, STARTUP_TIME_EXCEEDED_ERROR_CODE, sizeof(errorCodeString) - 1);
            systemStatus = SYSTEM_STATUS_ERROR;
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Startup not completed on time, currently %d minutes", startupTimeSeconds / 60);
            (void)logError(STARTUP_TIME_EXCEEDED_ERROR_CODE, "System", descr_str);
            maxStartupTimeExceededOneShot = false;
         }
      }
   }
}


/*******************************************************************************************/
/*                                                                                         */
/* void printHeaters()                                                                     */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void printHeaters()
{
   for (int i = 0; i < NUM_HEATERS; i++)
   {
      (void)printf("Heater[%d] isEnabled:%d isOn:%d short:%d open:%d over:%d under:%d sp:%d cur:%d delta:%d\n",
            i,
            heaterInfo[i].is_enabled,
            heaterInfo[i].is_on,
            rtdMappings[i].is_shorted,
            rtdMappings[i].is_open,
            heaterInfo[i].is_overtemp,
            heaterInfo[i].is_undertemp,
            heaterInfo[i].temperature_setpoint,
            heaterInfo[i].current_temperature,
            heaterInfo[i].delta_temp);
   }

   (void)printf("\n\n");
}


/*******************************************************************************************/
/*                                                                                         */
/* void logHourlyStats()                                                                   */
/*                                                                                         */
/* Once per hour, write the current statistics to the syslog file, then clear them.        */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void logHourlyStats()
{
   syslog(LOG_INFO, "     LOCATION                VOLTAGE   ADC COUNTS  TEMPERATURE  ON TIME  ENABLED  SHORTED/OPEN");
   syslog(LOG_INFO, "===============================================================================================");

   for (int i = 0; i < NUM_HEATERS; i++)
   {
      int current_temperature = lookupTempFromRawCounts(rtdMappings[i].value, i);
      char str[16];
      (void)memset(str, 0, sizeof(str));

      // if the counts are above 340 degree counts, call that open
      if (rtdMappings[i].value > rtdMappings[i].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
      {
         (void)strncpy(str, "Open", sizeof(str));
      }

      if (rtdMappings[i].value < rtdMappings[i].temp_lookup_table[0].adc_raw_counts)
      {
         (void)strncpy(str, "Shorted", sizeof(str));
      }

      char thisLine[1024];
      (void)memset(thisLine, 0, sizeof(thisLine));
      (void)snprintf(thisLine, sizeof(thisLine), "%s     %0.5fV     %4d       %5d F       %4d      %1d       %s", labels[i], ((float)(rtdMappings[i].value) * 1.8) / 4096.0,  rtdMappings[i].value, current_temperature, heaterInfo[i].seconds_on_time, heaterInfo[i].is_enabled, str);
      syslog(LOG_INFO, thisLine);
   }

   // now, clear the stats
   for (int i = 0; i < NUM_HEATERS; i++)
   {
      heaterInfo[i].seconds_on_time = 0;
   }
}


/*******************************************************************************************/
/*                                                                                         */
/* void *heaterControlThread(void *)                                                       */
/*                                                                                         */
/* Run the heater control algorithm once per second. Figure out which heaters are the      */
/* furthest away from their setpoints, and turn on up to eight heaters to keep under       */
/* the allowed power budget of 15A from the wall outlet.                                   */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *heaterControlThread(void *)
{
   uint32_t runningTime = 0;
   struct timeval start;
   struct timeval end;
   uint32_t executionTime;
   uint32_t balance;

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)gettimeofday(&start, NULL);

      runHeaterAlgorithm();

      runningTime++;
      if (0 == (runningTime % ONE_HOUR_IN_SECONDS))
      {
         // once per hour, write the statistics to the syslog
         logHourlyStats();
      }

      if (debugHeatersPrintf)
      {
         printHeaters();
      }

      (void)gettimeofday(&end, NULL);

      executionTime = (((end.tv_sec - start.tv_sec) * ONE_SECOND_IN_MICROSECONDS) + (end.tv_usec - start.tv_usec));
      if (executionTime < ONE_SECOND_IN_MICROSECONDS)
      {
         balance = ONE_SECOND_IN_MICROSECONDS - executionTime;
//         printf("task execution took %d microseconds balance = %ld\n", executionTime, balance);
         usleep(balance);
      }
      else
      {
         // force a minimum thread sleep time
//         printf("Execution time took more than 1 second - %d.%d\n", (executionTime / ONE_SECOND_IN_MICROSECONDS), (executionTime % ONE_SECOND_IN_MICROSECONDS));
         usleep(MINIMUM_THREAD_SLEEP_TIME);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}

// ****************** Firmware update *******************
/*******************************************************************************************/
/*                                                                                         */
/* void escapeSpaces(char *inPath, char *outPath)                                          */
/*                                                                                         */
/* Surround the entire file path with quotes, and escape any embedded spaces with          */
/* "\\\" or scp will fail                                                                  */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void escapeSpaces(char *inPath, char *outPath)
{
  int i;
  int length = strlen(inPath);
  char *inPtr = inPath;
  char *outPtr = outPath;

  // surround the entire file path with quotes, and escape any embedded spaces with "\\\" or scp will fail
  (void)strcpy(outPtr, "\"");
  outPtr++;
  for (i = 0; i < length; i++, inPtr++, outPtr++)
  {
    if (*inPtr == ' ')
    {
      (void)strcat(outPtr, "\\");
      outPtr += 1;
    }
    *outPtr = *inPtr;
  }
  (void)strcat(outPtr, "\"");
}


/*******************************************************************************************/
/*                                                                                         */
/* int sshKeyscan()                                                                        */
/*                                                                                         */
/* Issue the SSH keyscan to validate the necessary information to prepare for the SCP.     */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int sshKeyscan()
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   (void)memset(commandLine, 0, sizeof(COMMAND_LINE_BUFFER_SIZE));
   (void)snprintf(commandLine, sizeof(commandLine), "%s", RM_KNOWN_HOSTS);
   (void)system(commandLine);
   (void)snprintf(commandLine, sizeof(commandLine), SSH_KEYSCAN, guiIPAddress1);
   ret = system(commandLine);
   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getUpdateFiles(char *username, char *password, char *path)                          */
/*                                                                                         */
/* SCP all of the update files from the specified remote location to /tmp                  */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int getUpdateFiles(char *username, char *password, char *path)
{
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   char debFiles[THIS_FILE_FILENAME_LENGTH];
   char manifestFile[THIS_FILE_FILENAME_LENGTH];
   int pathLength = strlen(path);

   (void)memset(debFiles, 0, sizeof(debFiles));
   (void)memset(manifestFile, 0, sizeof(manifestFile));
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)strncpy(debFiles, path, pathLength);
   (void)strncat(debFiles, "/*.deb", sizeof(debFiles) - strlen("/*.deb") - 1);
   (void)strncpy(manifestFile, path, pathLength);
   (void)strncat(manifestFile, "/controller.manifest", sizeof(manifestFile)- strlen("/controller.manifest") - 1);

   (void)snprintf(commandLine, sizeof(commandLine), SCP_UPDATE_FILES, password, username, guiIPAddress1, debFiles);
   int ret = system(commandLine);
   (void)snprintf(commandLine, sizeof(commandLine), SCP_UPDATE_FILES, password, username, guiIPAddress1, manifestFile);
   ret |= system(commandLine);

   return ret;
}

/*******************************************************************************************/
/*                                                                                         */
/* int checkManifest(char *manifestFilename)                                               */
/*                                                                                         */
/* Check the SHA256 values for each file specified in the manifest. If ANY of them         */
/* don't match, return failure.                                                            */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int checkManifest(char *manifestFilename)
{
   FILE *fp = NULL;
   FILE *shaFP = NULL;
   int ret = 0;
   int itemsRead = 0;
   char shaFile[THIS_FILE_FILENAME_LENGTH];
   char shaString[SHA_STRING_LENGTH];
   char computedSHAString[SHA_STRING_LENGTH];
   char thisFilename[THIS_FILE_FILENAME_LENGTH];
   char firmwareVersionString[FIRMWARE_VERSION_STRING_LENGTH];
   char commandLine[COMMAND_LINE_BUFFER_SIZE];

   fp = fopen(manifestFilename, "r");
   manifestFileLineCount = 0;

   if (fp != NULL)
   {
      while (!feof(fp))
      {
         (void)memset(shaString, 0, SHA_STRING_LENGTH);
         (void)memset(thisFilename, 0, THIS_FILE_FILENAME_LENGTH);
         (void)memset(firmwareVersionString, 0, FIRMWARE_VERSION_STRING_LENGTH);
         (void)memset(commandLine, 0, COMMAND_LINE_BUFFER_SIZE);
         (void)memset(shaFile, 0, THIS_FILE_FILENAME_LENGTH);

         itemsRead = fscanf(fp, "%s %s %s", thisFilename, firmwareVersionString, shaString);
         if (MANIFEST_FILE_ITEMS_PER_LINE == itemsRead)
         {
            manifestFileLineCount++;
            if (!strncmp(thisFilename, FRONTIER_UHC_PACKAGE_FILENAME_ROOT, strlen(FRONTIER_UHC_PACKAGE_FILENAME_ROOT)))
            {
               manifestFileContainsFrontierUHC = true;
            }
            //(void)printf("itemsRead = %d\n", itemsRead);
            (void)printf("Filename = %s\n", thisFilename);
            (void)printf("firmwareVersionString = %s\n", firmwareVersionString);
            (void)printf("shaString = %s\n", shaString);
            (void)unlink(SHA_OUTPUT_FILE);
            (void)snprintf(commandLine, sizeof(commandLine), SHA256_COMMAND, thisFilename, SHA_OUTPUT_FILE);
            (void)system(commandLine);
            //system("cat sha.out");
            shaFP = fopen(SHA_OUTPUT_FILE, "r");
            if (shaFP != NULL)
            {
               itemsRead = fscanf(shaFP, "%s %s", computedSHAString, shaFile);
               (void)fclose(shaFP);
               if (!strcmp(computedSHAString, shaString))
               {
                  (void)printf("SHA is valid for %s\n", shaFile);
               }
               else
               {
                  (void)printf("SHA FAILED for %s\n", shaFile);
                  ret = 1;
                  break;
               }
            }
         }
         else if (manifestFileLineCount > 0)
         {
            ret = 0;
            break;
         }
         else
         {
            ret = 1;
            break;
         }
      }
      (void)fclose(fp);
   }
   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int installPackages(char *manifestFilename)                                             */
/*                                                                                         */
/* Install all of the packages retrieved via SCP into /tmp.                                */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int installPackages(char *manifestFilename)
{
   FILE *fp = NULL;
   int ret = 0;
   int itemsRead = 0;
   char shaString[SHA_STRING_LENGTH];
   char thisFilename[THIS_FILE_FILENAME_LENGTH];
   char firmwareVersionString[FIRMWARE_VERSION_STRING_LENGTH];
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   char installCommand[MAX_FILE_PATH];
   char resultString[MAX_FILE_PATH];
   uint32_t linesRead = 0;

   // initialize the result buffer
   (void)memset(firmwareUpdateResultBuffer, 0, sizeof(firmwareUpdateResultBuffer));
   (void)memset(frontierUHCPackageFilename, 0, sizeof(frontierUHCPackageFilename));
   processFrontierInstaller = false;

   // attempt to read the manifest file and process all of the packages listed in it
   fp = fopen(manifestFilename, "r");
   if (fp != NULL)
   {
      // process each file in listed in the manifest file
      while (!feof(fp))
      {
         (void)memset(shaString, 0, SHA_STRING_LENGTH);
         (void)memset(thisFilename, 0, THIS_FILE_FILENAME_LENGTH);
         (void)memset(firmwareVersionString, 0, FIRMWARE_VERSION_STRING_LENGTH);
         (void)memset(commandLine, 0, COMMAND_LINE_BUFFER_SIZE);

         itemsRead = fscanf(fp, "%s %s %s", thisFilename, firmwareVersionString, shaString);
         if (MANIFEST_FILE_ITEMS_PER_LINE == itemsRead)
         {
            linesRead++;
            if (!strncmp(thisFilename, FRONTIER_UHC_PACKAGE_FILENAME_ROOT, strlen(FRONTIER_UHC_PACKAGE_FILENAME_ROOT)))
            {
               // if this is the frontier-uhc package, skip it to process ALL other packages first
               strncpy(frontierUHCPackageFilename, thisFilename, sizeof(frontierUHCPackageFilename)- 1);
               continue;
            }
            (void)memset(installCommand, 0, sizeof(installCommand));
            (void)memset(resultString, 0, sizeof(resultString));
            (void)snprintf(installCommand, sizeof(installCommand), DPKG_INSTALL_OVERWRITE_COMMAND, TEMP_DIRECTORY, thisFilename);

            if (debugPrintf)
            {
               (void)printf("Attempting %s\n", installCommand);
               syslog(LOG_NOTICE, "Attempting %s", installCommand);
            }

            // attempt to install this package
            int result = system(installCommand);
            // build the status for this file
            if (result == 0)
            {
               (void)snprintf(resultString, sizeof(resultString), "%s %s\n", thisFilename, "Success");
               syslog(LOG_NOTICE, "%s %s", thisFilename, "Success");
            }
            else
            {
               (void)snprintf(resultString, sizeof(resultString), "%s Failed %d\n", thisFilename, result >> 8);
               syslog(LOG_ERR, "%s Failed %d", thisFilename, result >> 8);
               (void)logError("", "System", resultString);
            }

            // once we've processed a file, remove it from /tmp
            char removeFilename[MAX_FILE_PATH];
            (void)memset(removeFilename, 0, sizeof(removeFilename));
            (void)snprintf(removeFilename, sizeof(removeFilename), "%s/%s", TEMP_DIRECTORY, thisFilename);
            (void)unlink(removeFilename);

            // append the status for this file to previous results
            (void)strcat(firmwareUpdateResultBuffer, resultString);
         }
      }
   }
   else
   {
      (void)snprintf(firmwareUpdateResultBuffer, sizeof(firmwareUpdateResultBuffer), "Could not open manifest file %s %s\n", manifestFilename, "Failed");
      syslog(LOG_NOTICE, "Could not open manifest file %s %s\n", manifestFilename, "Failed");
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void *firmwareUpdatePackageThread(void *)                                               */
/*                                                                                         */
/* Handle the frontier-uhc firmware update package. Note that invoking dpkg to install     */
/* frontier-uhc will cause this instance of the application to be killed off so that the   */
/* installer can overwrite the executable.                                                 */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *firmwareUpdatePackageThread(void *)
{
   char installCommand[COMMAND_LINE_BUFFER_SIZE];

   numThreadsRunning++;
   while (!sigTermReceived)
   {
      if (processFrontierInstaller)
      {
         (void)memset(installCommand, 0, sizeof(installCommand));
         (void)snprintf(installCommand, strlen(DPKG_INSTALL_OVERWRITE_COMMAND) + strlen(TEMP_DIRECTORY) + strlen(frontierUHCPackageFilename) + 1, DPKG_INSTALL_OVERWRITE_COMMAND, TEMP_DIRECTORY, frontierUHCPackageFilename);

         if (debugPrintf)
         {
            (void)printf("Attempting %s\n", installCommand);
            syslog(LOG_NOTICE, "Attempting %s", installCommand);
         }

         // attempt to install this package
         (void)system(installCommand);
         (void)printf("firmwareUpdatePackageThread return from system command %s - Rebooting device\n", installCommand);
         processFrontierInstaller = false;
         sleep(1);
         (void)system(REBOOT_COMMAND);
      }
      sleep(1);
   }
   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *firmwareUpdateListenerThread(void *)                                              */
/*                                                                                         */
/* Listen for a FirmwareUpdate message. Fetch the update files via SCP int /tmp and        */
/* install them. Build and send the FirmwareResponse message.                              */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *firmwareUpdateListenerThread(void *)
{
   char message[FIRMWARE_UPDATE_BUFFER_SIZE];
   char escapedFullFilePath[MAX_FILE_PATH];
   firmwareUpdateResponsePublisher = zmq_socket(context, ZMQ_PUB);
   assert(firmwareUpdateResponsePublisher != 0);

   char ig1CommandRespPubPort[IP_STRING_SIZE + 14];   // Allow space for the tcp:// and :portnumber
   char ig1CommandReqSubPort[IP_STRING_SIZE + 14];    // Allow space for the tcp:// and :portnumber
   (void)snprintf(ig1CommandRespPubPort, sizeof(ig1CommandRespPubPort), "tcp://%s:%d", controllerIPAddress, FIRMWARE_UPDATE_RESULT_PORT_CONTROLLER);
   (void)snprintf(ig1CommandReqSubPort, sizeof(ig1CommandReqSubPort), "tcp://%s:%d", guiIPAddress1, FIRMWARE_UPDATE_PORT_GUI1);
   (void)printf("firmwareUpdateListenerThread ig1CommandRespPubPort = %s\n", ig1CommandRespPubPort);
   (void)printf("firmwareUpdateListenerThread ig1CommandReqSubPort = %s\n", ig1CommandReqSubPort);

    int rs = zmq_bind(firmwareUpdateResponsePublisher, ig1CommandRespPubPort);
    assert(rs == 0);

   firmwareUpdateListener = zmq_socket(context, ZMQ_SUB);
   assert(firmwareUpdateListener != 0);

   const char messageType[] = { 10, 4, 'F', 'W', 'U', 'P' };
   int rc = zmq_setsockopt(firmwareUpdateListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(firmwareUpdateListener, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(firmwareUpdateListener, ig1CommandReqSubPort);
   assert(rc == 0);

   (void)sleep(1);

   uint32_t sequenceNumber = 0U;

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));
      (void)memset(escapedFullFilePath, 0, sizeof(escapedFullFilePath));

      rc = zmq_recv(firmwareUpdateListener, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      FirmwareUpdate deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "firmwareUpdateListenerThread ERROR: Unable to deserialize!\n";
         }

         syslog(LOG_ERR, "firmwareUpdateListenerThread: Unable to deserialize!");
         (void)logError("", "firmwareUpdateListenerThread", "Unable to deserialize");
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n";
         deserialized.PrintDebugString();

         cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
         cout << "          sequence number: " << deserialized.sequence_number() << "\n";
         cout << "                 username: " << deserialized.username() << "\n";
         cout << "                 password: " << deserialized.password() << "\n";
         cout << "                file path: " << deserialized.file_path() << "\n";
      }

      // init the SSH keys
      int ret = sshKeyscan();

      if (0 == ret)
      {
         // fetch
         char user[deserialized.username().length()+1];
         (void)memset(user, 0, sizeof(user));
         (void)memcpy(user, (const void*)deserialized.username().c_str(), deserialized.username().length());

         char password[deserialized.password().length()+1];
         (void)memset(password, 0, sizeof(password));
         (void)memcpy(password, (const void*)deserialized.password().c_str(), deserialized.password().length());

         char path[deserialized.file_path().length()+1];
         (void)memset(path, 0, sizeof(path));
         (void)memcpy(path, (const void*)deserialized.file_path().c_str(), deserialized.file_path().length());

         // handle any spaces in the provided file path and append *.deb to the end for the scp
         escapeSpaces(path, escapedFullFilePath);

         ret = getUpdateFiles(user, password, escapedFullFilePath);

         if (0 == ret)
         {
            // Clear result buffer before starting.
            (void)memset(firmwareUpdateResultBuffer, 0, sizeof(firmwareUpdateResultBuffer));

            // check the manifest file and validate packages
            ret = checkManifest(controllerManifestFilename);

            if (0 != ret)
            {
               (void)snprintf(firmwareUpdateResultBuffer, sizeof(firmwareUpdateResultBuffer), "%s\n", "checkManifest Failed");
               syslog(LOG_ERR, "checkManifest Failed");
               (void)logError("", "firmwareUpdateListenerThread", "checkManifest failed");
            }
            else
            {
               // install
               ret = installPackages(controllerManifestFilename);

               if (0 != ret)
               {
                  (void)snprintf(firmwareUpdateResultBuffer, sizeof(firmwareUpdateResultBuffer), "%s\n", "installPackages Failed");
                  syslog(LOG_ERR, "installPackages Failed");
                  (void)logError("", "firmwareUpdateListenerThread", "installPackages failed");
               }
            }
         }
         else
         {
            (void)snprintf(firmwareUpdateResultBuffer, sizeof(firmwareUpdateResultBuffer), "%s\n", "getUpdateFiles Failed");
            syslog(LOG_ERR, "getUpdateFiles Failed");
            (void)logError("", "firmwareUpdateListenerThread", "getUpdateFiles failed");
         }
      }
      else
      {
         (void)snprintf(firmwareUpdateResultBuffer, sizeof(firmwareUpdateResultBuffer), "%s\n", "sshKeyscan Failed");
         syslog(LOG_ERR, "sshKeyscan Failed");
         (void)logError("", "firmwareUpdateListenerThread", "sshKeyscan failed");
      }

      // now that we've processed all other packages successfully, if the manifest contains the frontier-uhc package
      // set send the FWRS message
      // process the frontier-uhc package LAST
      if (manifestFileContainsFrontierUHC && (0 == ret))
      {
         // installed correctly. Since we get killed off by dpkg, we can't
         // report failure anyway
         char resultString[MAX_FILE_PATH];
         (void)memset(resultString, 0, sizeof(resultString));
         (void)snprintf(resultString, strlen("%s %s\n") + strlen(frontierUHCPackageFilename) + strlen("Success") + 1, "%s %s\n", frontierUHCPackageFilename, "Success");
         // append the status for this file to previous results
         (void)strcat(firmwareUpdateResultBuffer, resultString);
         syslog(LOG_NOTICE, "%s %s", frontierUHCPackageFilename, "Success");
      }
      // Publish ZeroMQ FirmwareUpdateResult message here
      FirmwareUpdateResult cr;
      cr.set_topic("FWRS");
      cr.set_controller_ip_address(controllerIPAddress);
      cr.set_sequence_number(sequenceNumber++);
      string resultString(firmwareUpdateResultBuffer);

      if (debugPrintf)
      {
         (void)printf("firmwareUpdateResultBuffer = \n%s\n", firmwareUpdateResultBuffer);
      }

      cr.set_result_text(resultString);

      string serialized;
      if ( !cr.SerializeToString( &serialized ) )
      {
         if (debugPrintf)
         {
            cerr << "firmwareUpdateListenerThread ERROR: Unable to serialize!\n";
         }
         syslog(LOG_ERR, "firmwareUpdateListenerThread: Unable to serialize!");
         (void)logError("", "firmwareUpdateListenerThread", "Unable to serialize");
      }

      char bytes[serialized.length()];
      (void)memcpy(bytes, serialized.data(), serialized.length());

      rs = zmq_send(firmwareUpdateResponsePublisher, bytes, serialized.length(), 0);
      assert(rs == (int)serialized.length());

      if (debugPrintf)
      {
         cout << "Bytes sent: " << rs << "\n";
      }
      // now that we've processed all other packages, if the manifest contains the frontier-uhc package
      // set the flag that the firmwareUpdatePackageThread is looking for.
      // process the frontier-uhc package LAST
      if (manifestFileContainsFrontierUHC)
      {
         processFrontierInstaller = true;
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *heartBeatListenerThread(void *)                                                   */
/*                                                                                         */
/* Listen for a HeartBeat message from the primary GUI device.                             */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *heartBeatListenerThread(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   heartBeatListener = zmq_socket(context, ZMQ_SUB);
   assert(heartBeatListener != 0);

   char heartbeatPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(heartbeatPort, sizeof(heartbeatPort), "tcp://%s:%d", guiIPAddress1, HEARTBEAT_PORT_GUI1);
   (void)printf("heartBeatListenerThread heartbeatPort = %s\n", heartbeatPort);

   char messageType[] = { 10, 2, 'H', 'B' };
   int rc = zmq_setsockopt(heartBeatListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(heartBeatListener, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(heartBeatListener, heartbeatPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));
      rc = zmq_recv(heartBeatListener, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      HeartBeat deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "heartBeatListenerThread ERROR: Unable to deserialize!");
            (void)logError("", "heartBeatListenerThread", "Unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n";
         deserialized.PrintDebugString();

         cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
         cout << "          sequence number: " << deserialized.sequence_number() << "\n";
      }

      lastTimeGUI1Heard = 0;
      gui1MissingOneShot = true;
      bothGuisMissingOneShot = true;
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *heartBeatListenerThread2(void *)                                                  */
/*                                                                                         */
/* Listen for a HeartBeat message from the secondary GUI device.                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *heartBeatListenerThread2(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   heartBeatListener2 = zmq_socket(context, ZMQ_SUB);
   assert(heartBeatListener2 != 0);

   char heartbeatPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(heartbeatPort, sizeof(heartbeatPort), "tcp://%s:%d", guiIPAddress2, HEARTBEAT_PORT_GUI2);
   (void)printf("heartBeatListenerThread2 heartbeatPort = %s\n", heartbeatPort);

   char messageType[] = { 10, 2, 'H', 'B' };
   int rc = zmq_setsockopt(heartBeatListener2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(heartBeatListener2, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(heartBeatListener2, heartbeatPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));
      rc = zmq_recv(heartBeatListener2, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      HeartBeat deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "heartBeatListenerThread2 ERROR: Unable to deserialize!");
            (void)logError("", "heartBeatListenerThread2", "Unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n";
         deserialized.PrintDebugString();

         cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
         cout << "          sequence number: " << deserialized.sequence_number() << "\n";
      }

      lastTimeGUI2Heard = 0;
      gui2MissingOneShot = true;
      bothGuisMissingOneShot = true;
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *statusPublisherThread(void *)                                                     */
/*                                                                                         */
/* Once per second, read the system data (RTD temps, fan status, etc) and publish it.      */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *statusPublisherThread(void *)
{
   char bytes[DEFAULT_CSS_MESSAGE_BUFFER_SIZE];
   char statusPublisherPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(statusPublisherPort, sizeof(statusPublisherPort), "tcp://%s:%d", controllerIPAddress, CURRENT_SYSTEM_STATE_PORT_CONTROLLER);
   (void)printf("statusPublisherThread statusPublisherPort = %s\n", statusPublisherPort);

   statusPublisher = zmq_socket(context, ZMQ_PUB);
   assert(statusPublisher != 0);

   int rc = zmq_bind(statusPublisher, statusPublisherPort);
   assert(rc == 0);

   bool is_error = false;
   uint32_t sequenceNumber = 0;
   struct timeval start;
   struct timeval end;
   uint32_t executionTime;
   uint32_t balance;
   bool fanError = false;
   int fanTachCount = 0;

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)gettimeofday(&start, NULL);
      lastTimeGUI1Heard++;
      lastTimeGUI2Heard++;
      CurrentSystemState s;
      ::google::protobuf::Timestamp* timestamp = new ::google::protobuf::Timestamp();
      struct timeval now;
      (void)gettimeofday(&now, NULL);
      timestamp->set_seconds(now.tv_sec);
      timestamp->set_nanos(now.tv_usec * 1000);

      fanError = false;
      // log an error if the tach value is 0 and the fan is on and it's more than 3 times in a row
      fanTachCount = getFanTach(FAN1_INDEX);
      if ((0 == fanTachCount) && (FAN_STATE_ON == fanInfo[FAN1_INDEX].fan_on) && (fanInfo[FAN1_INDEX].consecutive_tach_failures++ >= FAN_TACH_CONSECUTIVE_FAILURES))
      {
         fanError = true;
         // turn the fan off - the only way to reset it is to turn it back on
         (void)setFanOnOff(FAN1_INDEX, FAN_STATE_OFF);
         if (fan1FailureOneShot)
         {
            syslog(LOG_ERR, "%s Fan 1 tach failure", FAN_FAILURE_ERROR_CODE);
            (void)strncpy(errorCodeString, FAN_FAILURE_ERROR_CODE, sizeof(errorCodeString) - 1);
            systemStatus = SYSTEM_STATUS_ERROR;
            (void)logError(FAN_FAILURE_ERROR_CODE, "Fan 1", "Fan tach failure");
            is_error = true;
            fan1FailureOneShot = false;
         }
         (void)setFanOnOff(FAN1_INDEX, FAN_STATE_ON);
      }
      else if (fanTachCount)
      {
         fanInfo[FAN1_INDEX].consecutive_tach_failures = 0;
      }

      // log an error if the tach value is 0 and the fan is on
      fanTachCount = getFanTach(FAN2_INDEX);
      if ((0 == fanTachCount) && (FAN_STATE_ON == fanInfo[FAN2_INDEX].fan_on) && (fanInfo[FAN2_INDEX].consecutive_tach_failures++ >= FAN_TACH_CONSECUTIVE_FAILURES))
      {
         fanError = true;
         // turn the fan off - the only way to reset it is to turn it back on
         (void)setFanOnOff(FAN2_INDEX, FAN_STATE_OFF);
         if (fan2FailureOneShot)
         {
            syslog(LOG_ERR, "%s Fan 2 tach failure", FAN_FAILURE_ERROR_CODE);
            (void)strncpy(errorCodeString, FAN_FAILURE_ERROR_CODE, sizeof(errorCodeString) - 2);
            systemStatus = SYSTEM_STATUS_ERROR;
            (void)logError(FAN_FAILURE_ERROR_CODE, "Fan 2", "Fan tach failure");
            is_error = true;
            fan2FailureOneShot = false;
         }
         (void)setFanOnOff(FAN2_INDEX, FAN_STATE_ON);
      }
      else if (fanTachCount)
      {
         fanInfo[FAN2_INDEX].consecutive_tach_failures = 0;
      }

      // if hardware A02 or greater, check both fans for current limit tripped
      if (!fanError && (controllerBoardRevision > 0))
      {
         bool limitTripped = getFanCurrentLimit(FAN1_INDEX);
         if (limitTripped)
         {
            fanInfo[FAN1_INDEX].current_limit_delay_count++;
            if (fanInfo[FAN1_INDEX].current_limit_delay_count >= FAN_OVERCURRENT_DELAY_COUNT)
            {
               if (FAN_STATE_ON == fanInfo[FAN1_INDEX].fan_on)
               {
                  // turn the fan off - this should reset the current limit bit
                  (void)setFanOnOff(FAN1_INDEX, FAN_STATE_OFF);
                  fanInfo[FAN1_INDEX].current_limit_auto_correct_count++;
                  if (fan1CurrentLimitOneShot && (fanInfo[FAN1_INDEX].current_limit_auto_correct_count >= FAN_OVERCURRENT_AUTO_CORRECT_LIMIT))
                  {
                     syslog(LOG_ERR, "%s Fan 1 current limit", FAN_OVERCURRENT_ERROR_CODE);
                     (void)strncpy(errorCodeString, FAN_OVERCURRENT_ERROR_CODE, sizeof(errorCodeString) - 2);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     (void)logError(FAN_OVERCURRENT_ERROR_CODE, "Fan 1", "Fan current limit");
                     is_error = true;
                     fan1CurrentLimitOneShot = false;
                  }
                  if (fanInfo[FAN1_INDEX].current_limit_auto_correct_count < FAN_OVERCURRENT_AUTO_CORRECT_LIMIT)
                  {
                     (void)setFanOnOff(FAN1_INDEX, FAN_STATE_ON);
                  }
               }
            }
         }
         else
         {
            fanInfo[FAN1_INDEX].current_limit_delay_count = 0;
            fanInfo[FAN1_INDEX].current_limit_auto_correct_count = 0;
         }

         limitTripped = getFanCurrentLimit(FAN2_INDEX);
         if (limitTripped)
         {
            fanInfo[FAN2_INDEX].current_limit_delay_count++;
            if (fanInfo[FAN2_INDEX].current_limit_delay_count >= FAN_OVERCURRENT_DELAY_COUNT)
            {
               if (FAN_STATE_ON == fanInfo[FAN2_INDEX].fan_on)
               {
                  // turn the fan off - this should reset the current limit bit
                  (void)setFanOnOff(FAN2_INDEX, FAN_STATE_OFF);
                  fanInfo[FAN2_INDEX].current_limit_auto_correct_count++;
                  if (fan2CurrentLimitOneShot && (fanInfo[FAN2_INDEX].current_limit_auto_correct_count >= FAN_OVERCURRENT_AUTO_CORRECT_LIMIT))
                  {
                     syslog(LOG_ERR, "%s Fan 2 current limit", FAN_OVERCURRENT_ERROR_CODE);
                     (void)strncpy(errorCodeString, FAN_OVERCURRENT_ERROR_CODE, sizeof(errorCodeString) - 2);
                     systemStatus = SYSTEM_STATUS_ERROR;
                     (void)logError(FAN_OVERCURRENT_ERROR_CODE, "Fan 2", "Fan current limit");
                     is_error = true;
                     fan2CurrentLimitOneShot = false;
                  }
                  if (fanInfo[FAN2_INDEX].current_limit_auto_correct_count < FAN_OVERCURRENT_AUTO_CORRECT_LIMIT)
                  {
                     (void)setFanOnOff(FAN2_INDEX, FAN_STATE_ON);
                  }
               }
            }
         }
         else
         {
            fanInfo[FAN2_INDEX].current_limit_delay_count = 0;
            fanInfo[FAN2_INDEX].current_limit_auto_correct_count = 0;
         }
      }

      if (!softShutdownCheckOneshot)
      {
         if (access(SOFT_SHUTDOWN_FILE, F_OK) == 0)
         {
            (void)unlink(RM_SOFT_SHUTDOWN_FILE);
         }

         softShutdownCheckOneshot = true;
      }

      s.set_topic(CURRENT_SYSTEM_STATE_TOPIC);
      s.mutable_system_data()->set_heatsink_temp(lookupTempFromRawCounts(rtdMappings[HEATSINK_RTD_INDEX].value, HEATSINK_RTD_INDEX));
      // Vantron asked for the ambient temp to be added to the CSS even though the sensor for it won't be available until rev A02 hardware
      if (controllerBoardRevision > 0)
      {
         // the controller board HW revision was bumped from 0 to 1 for A02 hardware
         s.mutable_system_data()->set_ambient_temp(lookupTempFromRawCounts(rtdMappings[AMBIENT_TEMP_RTD_INDEX].value, AMBIENT_TEMP_RTD_INDEX));
      }
      else
      {
         // prior to HW rev A02, we report the HEATSINK temp as the AMBIENT temp
         s.mutable_system_data()->set_ambient_temp(lookupTempFromRawCounts(rtdMappings[HEATSINK_RTD_INDEX].value, HEATSINK_RTD_INDEX));
      }
      s.mutable_system_data()->set_fan_state1((uhc::FanState)fanInfo[0].fan_on);
      s.mutable_system_data()->set_fan_state2((uhc::FanState)fanInfo[1].fan_on);
      s.mutable_system_data()->set_allocated_current_time(timestamp);
      timestamp = new ::google::protobuf::Timestamp();
      timestamp->set_seconds(getSytemUptime());
      timestamp->set_nanos(0);
      s.mutable_system_data()->set_allocated_system_up_time(timestamp);
      s.mutable_system_data()->set_current_power_consumption(irms);
      s.mutable_system_data()->set_system_status(systemStatus);
      s.mutable_system_data()->set_configured_eco_mode_temp(heaterInfo[0].eco_mode_setpoint);
      s.mutable_system_data()->set_configured_eco_mode_minutes(15);
      s.mutable_system_data()->set_eco_mode_state(ECO_MODE_OFF);
      s.mutable_system_data()->set_shutdown_requested(false);
      s.mutable_system_data()->set_last_command_received(lastCommandReceived);
      s.mutable_system_data()->set_controller_ip_address(controllerIPAddress);
      s.mutable_system_data()->set_intelligent_glass_1_ip_address(guiIPAddress1);
      s.mutable_system_data()->set_intelligent_glass_2_ip_address(guiIPAddress2);
      timestamp = new ::google::protobuf::Timestamp();
      timestamp->set_seconds(lastTimeGUI1Heard);
      timestamp->set_nanos(0);
      s.mutable_system_data()->set_allocated_last_time_intelligent_glass_1_heard(timestamp);
      timestamp = new ::google::protobuf::Timestamp();
      timestamp->set_seconds(lastTimeGUI2Heard);
      timestamp->set_nanos(0);
      s.mutable_system_data()->set_allocated_last_time_intelligent_glass_2_heard(timestamp);
      s.set_hardware_revision((uint32_t)controllerBoardRevision);
      s.mutable_system_data()->set_alarm_code(ALARM_CODE_NONE);
      s.mutable_system_data()->set_heatsink_over_temp(heatsinkOvertemp);
      if (heatsinkOvertemp)
      {
         s.mutable_system_data()->set_alarm_code(ALARM_CODE_HEATSINK_OVER_TEMP);
         is_error = true;
      }

      s.mutable_system_data()->set_heatsink_temp(heatsinkTemp);
      s.mutable_system_data()->set_logging_is_event_driven(loggingIsEventDriven);
      s.mutable_system_data()->set_logging_period_seconds(loggingPeriodSeconds);
      s.mutable_system_data()->set_nso_mode(nsoMode);
      s.mutable_system_data()->set_demo_mode(demoMode);

      // only start logging
      if (getSytemUptime() > TWO_MINUTES_IN_SECONDS)
      {
         if (powerMonitorBad)
         {
            if (powerMonitorBadOneShot)
            {
               s.mutable_system_data()->set_alarm_code(ALARM_CODE_HARDWARE_FAILURE);
               syslog(LOG_ERR, "Hardware failure - power monitor");
               (void)strncpy(errorCodeString, HARDWARE_FAILURE_ERROR_CODE, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               alarmCode = ALARM_CODE_HARDWARE_FAILURE;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "Hardware failure - power monitor");
               (void)logError(HARDWARE_FAILURE_ERROR_CODE, "Hardware failure - power monitor", descr_str);
               is_error = true;
               powerMonitorBadOneShot = false;
            }
         }
         else
         {
            powerMonitorBadOneShot = true;
         }

         if (lastTimeGUI1Heard >= GUI_NO_COMMUNICATION_TIME_LIMIT)
         {
            if (gui1MissingOneShot)
            {
               s.mutable_system_data()->set_alarm_code(ALARM_CODE_INTELLIGENT_GLASS_FAILURE);
               syslog(LOG_ERR, "%s Intelligent Glass 1 - not heard from in more than %d seconds", SINGLE_GUI_COMM_LOSS_ERROR, lastTimeGUI1Heard);
               (void)strncpy(errorCodeString, SINGLE_GUI_COMM_LOSS_ERROR, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "not heard from in more than %d seconds", lastTimeGUI1Heard);
               (void)logError(SINGLE_GUI_COMM_LOSS_ERROR, "Glass 1", descr_str);
               is_error = true;
               gui1MissingOneShot = false;
            }
         }

         if (lastTimeGUI2Heard >= GUI_NO_COMMUNICATION_TIME_LIMIT)
         {
            if (gui2MissingOneShot)
            {
               s.mutable_system_data()->set_alarm_code(ALARM_CODE_INTELLIGENT_GLASS_FAILURE);
               syslog(LOG_ERR, "%s Intelligent Glass 2 - not heard from in more than %d seconds", SINGLE_GUI_COMM_LOSS_ERROR, lastTimeGUI2Heard);
               (void)strncpy(errorCodeString, SINGLE_GUI_COMM_LOSS_ERROR, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "not heard from in more than %d seconds", lastTimeGUI2Heard);
               (void)logError(SINGLE_GUI_COMM_LOSS_ERROR, "Glass 2", descr_str);
               is_error = true;
               gui2MissingOneShot = false;
            }
         }

         if ((lastTimeGUI1Heard >= GUI_NO_COMMUNICATION_TIME_LIMIT) && (lastTimeGUI2Heard >= GUI_NO_COMMUNICATION_TIME_LIMIT))
         {
            // both GUIs are down for more than 3 minutes
            // per Shawn Thompson, shut down the unit.
            // turn all heaters off
            s.mutable_system_data()->set_alarm_code(ALARM_CODE_INTELLIGENT_GLASS_FAILURE);
            for (int j = 0; j < NUM_HEATERS; j++)
            {
               (void)enableDisableHeater(j, HEATER_DISABLED);
               (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
            }

            if (bothGuisMissingOneShot)
            {
               syslog(LOG_ERR, "%s Both Intelligent Glass devices not heard from in more than %d seconds", BOTH_GUIS_COMM_LOSS_ERROR_CODE, GUI_NO_COMMUNICATION_TIME_LIMIT);
               syslog(LOG_ERR, "Heaters turned off and disabled");
               (void)strncpy(errorCodeString, BOTH_GUIS_COMM_LOSS_ERROR_CODE, sizeof(errorCodeString) - 1);
               systemStatus = SYSTEM_STATUS_ERROR;
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "not heard from in more than %d seconds", GUI_NO_COMMUNICATION_TIME_LIMIT);
               (void)logError(BOTH_GUIS_COMM_LOSS_ERROR_CODE, "Glass both", descr_str);
               is_error = true;
               bothGuisMissingOneShot= false;
            }
         }
      }

      if ((false == ethernetUp) && (ethernetDownTimeSeconds >= ETHERNET_NO_COMMUNICATION_TIME_LIMIT))
      {
         // turn all heaters off
         for (int j = 0; j < NUM_HEATERS; j++)
         {
            (void)enableDisableHeater(j, HEATER_DISABLED);
            (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
         }
         if (ethernetErrorOneShot)
         {
            ethernetErrorOneShot = false;
            syslog(LOG_ERR, "%s Ethernet down", ETHERNET_DOWN_ERROR_CODE);
            syslog(LOG_ERR, "Heaters turned off and disabled");
            (void)strncpy(errorCodeString, ETHERNET_DOWN_ERROR_CODE, sizeof(errorCodeString) - 1);
            systemStatus = SYSTEM_STATUS_ERROR;
            (void)logError(ETHERNET_DOWN_ERROR_CODE, "Ethernet down", "Ethernet down");
            is_error = true;
         }
      }
      if (ethernetUp && !previousEthernetUp)
      {
         //printf("====================Ethernet Re-established =================\n");
         syslog(LOG_NOTICE, "Ethernet back up");
         (void)logError("", "Ethernet back up", "Ethernet back up");
      }

      for (int i = 0; i < TOTAL_SLOTS; i++)
      {
         (void)s.add_slot_data();
         SlotNumber sn = static_cast<SlotNumber>(i+1);
         SlotData* newSlotData = s.mutable_slot_data(i);
         newSlotData->set_slot_number(sn);
         newSlotData->mutable_heater_location_upper()->set_state((uhc::HeaterState)heaterInfo[i*2].state);
         newSlotData->mutable_heater_location_upper()->set_location((uhc::HeaterLocation)heaterInfo[i*2].location);
         newSlotData->mutable_heater_location_upper()->set_thermistor_temp(lookupTempFromRawCounts(rtdMappings[i*2].value, i*2));
         newSlotData->mutable_heater_location_upper()->set_setpoint_temp(heaterInfo[i*2].temperature_setpoint);

         uhc::LEDState led_state;
         if (HEATER_STATE_ON == heaterInfo[i*2].state)
         {
            led_state = LED_STATE_RED;
         }
         else
         {
            led_state = LED_STATE_OFF;
         }

         newSlotData->mutable_heater_location_upper()->set_led_state(led_state);

         if (rtdMappings[i*2].is_open)
         {
            newSlotData->mutable_heater_location_upper()->set_is_open(true);
//            (void)printf("heater[%d] (upper) is open sequenceNumber %d\n", i*2, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_upper()->set_is_open(false);
         }

         if (rtdMappings[i*2].is_shorted)
         {
            newSlotData->mutable_heater_location_upper()->set_is_shorted(true);
//            (void)printf("heater[%d] (upper) is shorted sequenceNumber %d\n", i*2, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_upper()->set_is_shorted(false);
         }

         if (heaterInfo[i*2].is_undertemp_CSS)
         {
            newSlotData->mutable_heater_location_upper()->set_is_undertemp(true);
            s.mutable_system_data()->set_alarm_code(ALARM_CODE_SLOT_UNDER_TEMP);
//            (void)printf("heater[%d] (upper) is undertemp sequenceNumber %d\n", i*2, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_upper()->set_is_undertemp(false);
         }

         if (heaterInfo[i*2].is_overtemp_CSS)
         {
            newSlotData->mutable_heater_location_upper()->set_is_overtemp(true);
            s.mutable_system_data()->set_alarm_code(ALARM_CODE_SLOT_OVER_TEMP);
//            (void)printf("heater[%d] (upper) is overtemp sequenceNumber %d\n", i*2, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_upper()->set_is_overtemp(false);
         }

         if (heaterInfo[i*2].is_enabled)
         {
            newSlotData->mutable_heater_location_upper()->set_is_enabled(true);
         }
         else
         {
            newSlotData->mutable_heater_location_upper()->set_is_enabled(false);
         }

         ::google::protobuf::Timestamp* ts = new ::google::protobuf::Timestamp();
         ts->set_seconds(heaterInfo[i*2].start_time.seconds());
         ts->set_nanos(0);
         newSlotData->mutable_heater_location_upper()->set_allocated_start_time(ts);
         ::google::protobuf::Timestamp* ts1 = new ::google::protobuf::Timestamp();
         ts1->set_seconds(heaterInfo[i*2].end_time.seconds());
         ts1->set_nanos(0);
         newSlotData->mutable_heater_location_upper()->set_allocated_end_time(ts1);

         newSlotData->mutable_heater_location_lower()->set_state((uhc::HeaterState)heaterInfo[(i*2)+1].state);
         newSlotData->mutable_heater_location_lower()->set_location((uhc::HeaterLocation)heaterInfo[(i*2)+1].location);
         newSlotData->mutable_heater_location_lower()->set_thermistor_temp(lookupTempFromRawCounts(rtdMappings[(i*2)+1].value, (i*2)+1));
         newSlotData->mutable_heater_location_lower()->set_setpoint_temp(heaterInfo[(i*2)+1].temperature_setpoint);

         if (HEATER_STATE_ON == heaterInfo[(i*2)+1].state)
         {
            led_state = LED_STATE_RED;
         }
         else
         {
            led_state = LED_STATE_OFF;
         }

         newSlotData->mutable_heater_location_lower()->set_led_state(led_state);

         if (rtdMappings[(i*2)+1].is_open)
         {
            newSlotData->mutable_heater_location_lower()->set_is_open(true);
//            (void)printf("heater[%d] (lower) is open sequenceNumber %d\n", (i*2)+1, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_lower()->set_is_open(false);
         }

         if (rtdMappings[(i*2)+1].is_shorted)
         {
            newSlotData->mutable_heater_location_lower()->set_is_shorted(true);
//            (void)printf("heater[%d] (lower) is shorted sequenceNumber %d\n", (i*2)+1, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_lower()->set_is_shorted(false);
         }

         if (heaterInfo[(i*2)+1].is_undertemp_CSS)
         {
            newSlotData->mutable_heater_location_lower()->set_is_undertemp(true);
            s.mutable_system_data()->set_alarm_code(ALARM_CODE_SLOT_UNDER_TEMP);
//            (void)printf("heater[%d] (lower) is undertemp sequenceNumber %d\n", (i*2)+1, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_lower()->set_is_undertemp(false);
         }

         if (heaterInfo[(i*2)+1].is_overtemp_CSS)
         {
            newSlotData->mutable_heater_location_lower()->set_is_overtemp(true);
            s.mutable_system_data()->set_alarm_code(ALARM_CODE_SLOT_OVER_TEMP);
//            (void)printf("heater[%d] (lower) is overtemp sequenceNumber %d\n", (i*2)+1, sequenceNumber + 1);
            is_error = true;
         }
         else
         {
            newSlotData->mutable_heater_location_lower()->set_is_overtemp(false);
         }

         if (heaterInfo[(i*2)+1].is_enabled)
         {
            newSlotData->mutable_heater_location_lower()->set_is_enabled(true);
         }
         else
         {
            newSlotData->mutable_heater_location_lower()->set_is_enabled(false);
         }

         ::google::protobuf::Timestamp* ts2 = new ::google::protobuf::Timestamp();
         ts2->set_seconds(heaterInfo[(i*2)+1].start_time.seconds());
         ts2->set_nanos(0);
         newSlotData->mutable_heater_location_lower()->set_allocated_start_time(ts2);
         ::google::protobuf::Timestamp* ts3 = new ::google::protobuf::Timestamp();
         ts3->set_seconds(heaterInfo[(i*2)+1].end_time.seconds());
         ts3->set_nanos(0);
         newSlotData->mutable_heater_location_lower()->set_allocated_end_time(ts3);
      }

      s.set_serial_number(serialNumber);
      s.set_model_number(modelNumber);
      s.set_firmware_version(FRONTIER_UHC_FIRMWARE_VERSION);
      s.set_sequence_number(sequenceNumber++);
      s.mutable_system_data()->set_hp_error_code(errorCodeString);
      if (strlen(errorCodeString) != 0)
      {
         errorReportCount++;
         if (errorReportCount > REPORT_ERROR_COUNT)
         {
            (void)memset(errorCodeString, 0, sizeof(errorCodeString));
            errorReportCount = 0;
         }
      }

      if (is_error && debugCSS)
      {
         cout << "Packet contains error state Serialization:\n\n" << s.DebugString() << "\n\n";
         s.PrintDebugString();
      }

      string serialized;
      if (!s.SerializeToString(&serialized))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to serialize!\n";
            syslog(LOG_ERR, "statusPublisherThread ERROR: Unable to serialize!");
            (void)logError("", "statusPublisherThread", "unable to serialize");
         }
      }

      memset(bytes, 0, DEFAULT_CSS_MESSAGE_BUFFER_SIZE);
      (void)memcpy(bytes, serialized.data(), serialized.length());

      rc = zmq_send(statusPublisher, bytes, serialized.length(), 0);
      assert(rc == (int)serialized.length());

      if (debugPrintf)
      {
         cout << "Bytes sent: " << rc << "\n";
      }

      (void)gettimeofday(&end, NULL);

      executionTime = (((end.tv_sec - start.tv_sec) * ONE_SECOND_IN_MICROSECONDS) + (end.tv_usec - start.tv_usec));
      if (executionTime < ONE_SECOND_IN_MICROSECONDS)
      {
         balance = ONE_SECOND_IN_MICROSECONDS - executionTime;
//         printf("task execution took %d microseconds balance = %ld\n", executionTime, balance);
         usleep(balance);
      }
      else
      {
         // force a minimum thread sleep time
//         printf("Execution time took more than 1 second - %d.%d\n", (executionTime / ONE_SECOND_IN_MICROSECONDS), (executionTime % ONE_SECOND_IN_MICROSECONDS));
         usleep(MINIMUM_THREAD_SLEEP_TIME);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *rtdPublisherThread(void *)                                                        */
/*                                                                                         */
/* Once per second, read the RTD data (RTD raw counts, temp, shorted, etc) and publish it. */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *rtdPublisherThread(void *)
{
   char bytes[DEFAULT_RTD_MESSAGE_BUFFER_SIZE];
   char rtdPublisherPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(rtdPublisherPort, sizeof(rtdPublisherPort), "tcp://%s:%d", controllerIPAddress, RTD_DATA_PUBLISHER_PORT);
   (void)printf("rtdPublisherThread rtdPublisherPort = %s\n", rtdPublisherPort);

   rtdPublisher = zmq_socket(context, ZMQ_PUB);
   assert(rtdPublisher != 0);

   int rc = zmq_bind(rtdPublisher, rtdPublisherPort);
   assert(rc == 0);

   uint32_t sequenceNumber = 0;
   struct timeval start;
   struct timeval end;
   uint32_t executionTime;
   uint32_t balance;

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)gettimeofday(&start, NULL);
      ReadRTDs s;

      s.set_topic(RTD_DATA_PUBLISHER_TOPIC);
      s.set_hardware_revision((uint32_t)controllerBoardRevision);
      s.set_controller_ip_address(controllerIPAddress);

      for (int i = 0; i < NUM_RTDs; i++)
      {
         (void)s.add_rtd_data();
         RTDData* newRTDData = s.mutable_rtd_data(i);
         newRTDData->set_rtd_number((uint32_t)i+1);
         newRTDData->set_location(labels[i]);
         newRTDData->set_raw_counts((uint32_t)rtdMappings[i].value);
         newRTDData->set_temperature((int32_t)lookupTempFromRawCounts(rtdMappings[i].value, i));
         newRTDData->set_voltage(((float)(rtdMappings[i].value) * 1.8) / 4096.0);
         newRTDData->set_temp_data_filename(rtdMappings[i].temp_data_filename);

         if (rtdMappings[i].is_open)
         {
            newRTDData->set_is_open(true);
         }
         else
         {
            newRTDData->set_is_open(false);
         }
         if (rtdMappings[i].is_shorted)
         {
            newRTDData->set_is_shorted(true);
         }
         else
         {
            newRTDData->set_is_shorted(false);
         }
      }

      s.set_sequence_number(sequenceNumber++);
      if (debugCSS)
      {
         cout << "Packet contains error state Serialization:\n\n" << s.DebugString() << "\n\n";
         s.PrintDebugString();
      }

      string serialized;
      if (!s.SerializeToString(&serialized))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to serialize!\n";
            syslog(LOG_ERR, "rtdPublisherThread ERROR: Unable to serialize!");
            (void)logError("", "rtdPublisherThread", "unable to serialize");
         }
      }

      memset(bytes, 0, DEFAULT_RTD_MESSAGE_BUFFER_SIZE);
      (void)memcpy(bytes, serialized.data(), serialized.length());

      rc = zmq_send(rtdPublisher, bytes, serialized.length(), 0);
      assert(rc == (int)serialized.length());

      if (debugPrintf)
      {
         cout << "Bytes sent: " << rc << "\n";
      }

      (void)gettimeofday(&end, NULL);

      executionTime = (((end.tv_sec - start.tv_sec) * ONE_SECOND_IN_MICROSECONDS) + (end.tv_usec - start.tv_usec));
      if (executionTime < ONE_SECOND_IN_MICROSECONDS)
      {
         balance = ONE_SECOND_IN_MICROSECONDS - executionTime;
//         printf("task execution took %d microseconds balance = %ld\n", executionTime, balance);
         usleep(balance);
      }
      else
      {
         // force a minimum thread sleep time
//         printf("Execution time took more than 1 second - %d.%d\n", (executionTime / ONE_SECOND_IN_MICROSECONDS), (executionTime % ONE_SECOND_IN_MICROSECONDS));
         usleep(MINIMUM_THREAD_SLEEP_TIME);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/**
 * @brief Return a string representing the current system status.
 *
 * @return A const char * to the appropriate string.
 */
static const char *systemStatusStr()
{
   const char *status_str;

   switch (systemStatus)
   {
      default:
      case SYSTEM_STATUS_UNKNOWN:
         status_str = "Unknown";
         break;

      case SYSTEM_STATUS_NORMAL:
         if (inCleaningMode)
         {
            status_str = "Clean Mode";
         }
         else
         {
            status_str = "Normal Operation";
         }
         break;

      case SYSTEM_STATUS_ERROR:
         status_str = "Error";
         break;

      case SYSTEM_STATUS_STARTUP:
         status_str = "Startup";
         break;

      case SYSTEM_STATUS_STARTUP_COMPLETE:
         status_str = "Startup Complete";
         break;
   }

   return status_str;
}


/**
 * @brief Return a string representing the current status of a given slot.
 *
 * @param[in] slot_number The zero-based slot number for which to return the status string.
 *
 * @return A const char * to the appropriate string.
 */
static const char *slotStatusStr(int slot_number)
{
   const char *status_str;

   if (!heaterInfo[slot_number * 2].is_enabled)
   {
      status_str = "OFF";
   }
   else if (!startupComplete)
   {
      status_str = "Preheat";
   }
   else if (heaterInfo[slot_number * 2].eco_mode_on)
   {
      status_str = "ECO";
   }
   else
   {
      HEATER_GPIO_SYSFS_INFO const *uh = &heaterInfo[slot_number * 2];
      HEATER_GPIO_SYSFS_INFO const *lh = &heaterInfo[(slot_number * 2) + 1];
      RTD_MUX_MAPPING const *ur = &rtdMappings[slot_number * 2];
      RTD_MUX_MAPPING const *lr = &rtdMappings[(slot_number * 2) + 1];
      if (ur->is_open || uh->is_overtemp || ur->is_shorted || uh->is_undertemp
      ||  lr->is_open || lh->is_overtemp || lr->is_shorted || lh->is_undertemp)
      {
         status_str = "Error";
      }
      else
      {
         status_str = "Ready";
      }
   }

   return status_str;
}


/**
 * @brief Return a string representing the current status of a given heater.
 *
 * @param[in] slot_number The zero-based heater number for which to return the status string.
 *
 * @return A const char * to the appropriate string.
 */
static const char *heaterStatusStr(int heater_number)
{
   const char *status_str;

   if (!heaterInfo[heater_number].is_enabled)
   {
      status_str = "Disabled";
   }
   else
   {
      // If a heater is enabled, any state other than ON is OFF.
      status_str = (uhc::HeaterState::HEATER_STATE_ON == heaterInfo[heater_number].state) ? "ON" : "OFF";
   }

   return status_str;
}


/**
 * @brief Return a string with the name of the given command.
 *
 * @param[in] command The command whose name to return.
 *
 * @return A const char * to the appropriate string.
 */
static const char *commandStr(uhc::SystemCommands command)
{
   const char *command_str;

   switch (command)
   {
      case SYSTEM_COMMAND_UNKNOWN:
         command_str = "Unknown";
         break;

      case SYSTEM_COMMAND_HEATER_ON:
         command_str = "Heater_on";
         break;

      case SYSTEM_COMMAND_HEATER_OFF:
         command_str = "Heater_off";
         break;

      case SYSTEM_COMMAND_SHUTDOWN_REQUESTED:
         command_str = "Shutdown_requested";
         break;

      case SYSTEM_COMMAND_EMERGENCY_STOP:
         command_str = "Emergency_stop";
         break;

      case SYSTEM_COMMAND_STARTUP:
         command_str = "Startup";
         break;

      case SYSTEM_COMMAND_IDLE:
         command_str = "Idle";
         break;

      case SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT:
         command_str = "Update_slot_temp_setpoint";
         break;

      case SYSTEM_COMMAND_SET_DURATION:
         command_str = "Set_duration";
         break;

      case SYSTEM_COMMAND_SET_ECO_MODE_TIME:
         command_str = "Set_eco_mode_time";
         break;

      case SYSTEM_COMMAND_SET_ECO_MODE_TEMP:
         command_str = "Set_eco_mode_temp";
         break;

      case SYSTEM_COMMAND_FAN_ON:
         command_str = "Fan_on";
         break;

      case SYSTEM_COMMAND_FAN_OFF:
         command_str = "Fan_off";
         break;

      case SYSTEM_COMMAND_CLEANING_MODE_ON:
         command_str = "Cleaning_mode_on";
         break;

      case SYSTEM_COMMAND_CLEANING_MODE_OFF:
         command_str = "Cleaning_mode_off";
         break;

      case SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT:
         command_str = "Set_heater_temp_setpoint";
         break;

      case SYSTEM_COMMAND_ESTABLISH_LINK:
         command_str = "Establish_link";
         break;

      case SYSTEM_COMMAND_ECO_MODE_ON:
         command_str = "Eco_mode_on";
         break;

      case SYSTEM_COMMAND_ECO_MODE_OFF:
         command_str = "Eco_mode_off";
         break;

      default:
         command_str = "";
         break;
   }

   return command_str;
}


static const char *internalStr(internal_event_t event)
{
   const char *internal_str;

   switch (event)
   {
      case STARTUP_COMPLETE_T:
         internal_str = "Startup_complete";
         break;

      case TIMESYNC_RECEIVED_T:
         internal_str = "Timesync received";
         break;

      default:
         internal_str = "";
         break;
   }

   return internal_str;
}


/**
 * @brief Return a struct tm with the current local time.
 *
 * @param[out] nowtm A pointer to the struct tm to populate.
 */
static void nowLocalGet(struct tm *nowtm)
{
   struct timeval now;
   (void)gettimeofday(&now, NULL);
   time_t nowtime = now.tv_sec;
   (void)localtime_r(&nowtime, nowtm);
}


/**
 * @brief Get the current date (year/month/day).
 *
 * @return A date_t containing the current date.
 */
static date_t currentDateGet()
{
   struct tm nowtm;
   nowLocalGet(&nowtm);

   date_t date;
   date.year = nowtm.tm_year + 1900;
   date.month = nowtm.tm_mon + 1;
   date.day = nowtm.tm_mday;

   return date;
}


/**
 * @brief Compare two date_t values for equality.
 *
 * @param[in] date1 The first date.
 *
 * @param[in] date2 The second date.
 *
 * @return true if the dates are equal.
 *
 * @return false if the dates are not equal.
 */
static inline bool datesEqual(const date_t &date1, const date_t &date2)
{
   return (date1.year == date2.year) && (date1.month == date2.month) && (date1.day == date2.day);
}


/**
 * @brief Create the file name (full path) for a status/error/event log file for the given date.
 *
 * @param[in] name A pointer to where to put the created name.
 *
 * @param name_len The size of memory at *name.
 *
 * @param date The date to use for creating the name.
 */
static void logfileNameCreate(char *name, size_t name_len, date_t date)
{
   // Assumes the template has placeholders for, in order:
   // parent directory
   // year
   // month
   // day
   int gen_len = snprintf(name, name_len, EESLOGFILENAMETEMPLATE,
                                          SD_CARD_LOG_DIRECTORY,
                                          date.year, date.month, date.day);
   assert(gen_len < (int)name_len);
}


/**
 * @brief Open the status/error/event log file.
 *
 * Create the log file name from the given date, and attempt to open the file. If it already exists, it is
 * opened for append. If it does not already exist, it is created and then opened for append.
 *
 * This function checks the sdCardExists flag before attempting to open the file. If the SD card is not present,
 * no attempt is made to open the file, and the function returns a NULL file pointer. This simplifies the higher
 * level code since it doesn't have to check for the SD card.
 *
 * @param name The of name (full path) of the file to open.
 *
 * @return A file pointer if the open succeeded, or 0 if it failed.
 */
static FILE *logfileOpen(const char *name)
{
   static bool error_reported = false;
   FILE *fp = 0;

   if (sigTermReceived)
   {
      // if we're exiting, return
      return fp;
   }

   if (sdCardExists)
   {
      // Open the file for append. Create it if it does not already exist.
      fp = fopen(name, "a");

      if (0 == fp)
      {
         // Error opening log file for write. Fix and remount the SD card, then try again to open the file.
         if (!fsckAttempted)
         {
            fsckAttempted = true;
            switch(sdCardType)
            {
               case FAT32:
                  (void)system(SD_CARD_FSCK_CMD);
                  (void)system(UNMOUNT_SD_CARD_COMMAND);
                  (void)system(SD_CARD_MOUNT_VFAT_COMMAND);
                  break;

               case EXFAT:
                  (void)system(SD_CARD_EXFAT_FSCK_CMD);
                  (void)system(UNMOUNT_SD_CARD_COMMAND);
                  (void)system(SD_CARD_EXFAT_MOUNT_COMMAND);
                  break;

               case UNKNOWN:
               default:
                  return NULL;
            }
         }
         fp = fopen(name, "a");
      }
      else
      {
         fsckAttempted = false;
      }

      if (0 == fp)
      {
         if (!error_reported)
         {
            // Log this error only the first time.
            syslog(LOG_ERR, SD_CARD_MISSING_OR_CORRUPT ": Unable to open log file %s", name);
            error_reported = true;
         }
      }
      else
      {
         error_reported = false;
      }
   }

   return fp;
}


/**
 * @brief Write the log header to the status/error/event log file.
 *
 * @param[in] fileName The name of the  log file.
 */
static void logHeaderWrite(const char *fileName)
{
   FILE *logfileHandle = logfileOpen(fileName);
   if (0 != logfileHandle)
   {
      // Read the most recent 25 errors file to get the content and the number of errors.
      std::string recent_errors[25];
      size_t num_recent_errors = 0;
      std::ifstream recent_errors_file(RECENT_ERRORS_FILE);
      while ((num_recent_errors < 25) && getline(recent_errors_file, recent_errors[num_recent_errors]))
      {
         ++num_recent_errors;
      }

      // The log file template is intended to produce a log file that is in CSV format, suitable for importing
      // into a spreadsheet program. The log file header records should have an empty cell in column A.

      // The first record, static text, comes in row 3 (rows 1 and 2 are empty).
      (void)fprintf(logfileHandle, "\n\n\"\",\" ****** R E P O R T ******\"\n");

      // The second record, current date/time, comes in row 5.
      struct tm nowtm;
      nowLocalGet(&nowtm);
      char timestr[TIME_STR_SIZE];     // Long enough to hold mm-dd-yyyy hh:MM:ss AM.
      size_t gen_len = strftime(timestr, sizeof(timestr), "%m-%d-%Y %I:%M:%S %p", &nowtm);
      assert(0 != gen_len);
      (void)fprintf(logfileHandle, "\n\"\",\"%s\"\n", timestr);

      // The third record, static text, comes in row 7.
      (void)fprintf(logfileHandle, "\n\"\",\" ===== UNIT DESCRIPTION =====\"\n");

      // The fourth record, unit type, comes in row 9.
      // TODO: The value part is static text for now, replace this with the actual dynamically defined unit type.
      (void)fprintf(logfileHandle, "\n\"\",\" Unit Type                    %s\"\n", "FRONTIER: UHC-600");

      // The fifth record, control software part number, comes in row 10.
      // TODO: Use the real software part number.
      (void)fprintf(logfileHandle, "\"\",\" Control Software - HP Part No.        %s\"\n", "###########");

      // The sixth record, control software release, comes in row 11.
      // TODO. Use the real software release.
      (void)fprintf(logfileHandle, "\"\",\" Control Software - Release       %s\"\n", FRONTIER_UHC_FIRMWARE_VERSION);

      // The seventh record, customer ID, comes in row 12.
      (void)fprintf(logfileHandle, "\"\",\" Customer ID        %s\"\n", "McD");

      // Row 13 is empty, row 14 begins the error log (most recent 25 errors).
      (void)fprintf(logfileHandle, "\n\"\",\" ===== ERROR LOG =====\"\n");

      // Row 15 has the total number of errors in this section, row 16 is blank.
      (void)fprintf(logfileHandle, "\"\",\" Tot Num Errors = %u\"\n\n", num_recent_errors);

      // Row 17 has the current date and time.
      (void)fprintf(logfileHandle, "\"\",\" A. %s *NOW*\"\n", timestr);

      // Rows 18 - 42 each have an error message or "---- Empty ----"
      char error_tag = 'B';
      for (size_t i = 0; i < 25; ++i)
      {
         // Could compare i to num_recent_errors for this, but checking for empty string is more straightforward.
         if (!recent_errors[i].empty())
         {
            (void)fprintf(logfileHandle, "\"\",\" %c. %s\"\n", error_tag, recent_errors[i].c_str());
         }
         else
         {
            (void)fprintf(logfileHandle, "\"\",\" %c. ---- Empty ----\"\n", error_tag);
         }

         ++error_tag;
      }

      // Row 43 is blank, row 44 has the END marker.
      (void)fprintf(logfileHandle, "\n\"\",\" ******  - E N D -  ******\"\n");

      // Row 45 is blank, row 46 has the log header record. All of the log records, including the log
      // header, start in column A rather than column B (like all the header records up til now).
      (void)fprintf(logfileHandle, "\n"
               "\" .... DATE ....\",\" ... TIME ...\",\"UTC\",\"DST Enabled\","
               "\" .. STATUS ..\",\"FAN 1 STAT\",\"FAN 2 STAT\","
               "\" AMBIENT\",\" HEAT SINK\",\"Line Voltage\",\"Current Draw\","
               "\"SHELF 1 STAT\",\"SHELF 1 UPR STAT\",\" SHELF 1 UPR SETPT\",\" SHELF 1 UPR TEMP\",\"SHELF 1 LWR STAT\",\" SHELF 1 LWR SETPT\",\" SHELF 1 LWR TEMP\","
               "\"SHELF 2 STAT\",\"SHELF 2 UPR STAT\",\" SHELF 2 UPR SETPT\",\" SHELF 2 UPR TEMP\",\"SHELF 2 LWR STAT\",\" SHELF 2 LWR SETPT\",\" SHELF 2 LWR TEMP\","
               "\"SHELF 3 STAT\",\"SHELF 3 UPR STAT\",\" SHELF 3 UPR SETPT\",\" SHELF 3 UPR TEMP\",\"SHELF 3 LWR STAT\",\" SHELF 3 LWR SETPT\",\" SHELF 3 LWR TEMP\","
               "\"SHELF 4 STAT\",\"SHELF 4 UPR STAT\",\" SHELF 4 UPR SETPT\",\" SHELF 4 UPR TEMP\",\"SHELF 4 LWR STAT\",\" SHELF 4 LWR SETPT\",\" SHELF 4 LWR TEMP\","
               "\"SHELF 5 STAT\",\"SHELF 5 UPR STAT\",\" SHELF 5 UPR SETPT\",\" SHELF 5 UPR TEMP\",\"SHELF 5 LWR STAT\",\" SHELF 5 LWR SETPT\",\" SHELF 5 LWR TEMP\","
               "\"SHELF 6 STAT\",\"SHELF 6 UPR STAT\",\" SHELF 6 UPR SETPT\",\" SHELF 6 UPR TEMP\",\"SHELF 6 LWR STAT\",\" SHELF 6 LWR SETPT\",\" SHELF 6 LWR TEMP\","
               "\" ... EVENTS ...\",\" ERROR\""
               "\n");

      (void)fflush(logfileHandle);
      (void)fclose(logfileHandle);
   }
}


/**
 * @brief Write an error or event record to the status/error/event log file.
 *
 * @param[in] logfileHandle The name of the log file.
 *
 * @param[in] ee The error or event data to write.
 */
static void logErrorEventWrite(const char *fileName, const error_event_t &ee)
{
   FILE *logfileHandle = logfileOpen(fileName);
   if (0 != logfileHandle)
   {
      // Get the date and time string for columns A and B. Format them with "," between them to
      // make it easy to use it in the CSV output.
      struct tm nowtm;
      time_t nowtime = ee.timestamp.tv_sec;
      (void)localtime_r(&nowtime, &nowtm);
      char timestr_colAB[TIME_STR_SIZE];    // Long enough to hold mm-dd-yyyy","hh:MM:ss AM.
      size_t gen_len = strftime(timestr_colAB, sizeof(timestr_colAB), "%m/%d/%Y\",\"%H:%M:%S", &nowtm);
      assert(0 != gen_len);

      const char *fmt = "\"%s\","                                                         // Date and time (columns A and B)
                        "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\","    // Columns C - BA (51 columns) are empty.
                        "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\","    // 12 empty fields on this line and on the one above.
                        "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\","    // 12 more empty fields.
                        "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\","    // 12 more empty fields.
                        "\"\",\"\",\"\","                                                 // And three more makes 51.
                        "\"%s\",\"%s\""                                                   // Events, error.
                        "\n";                                                             // Terminate the record with a newline.

      char error_event_str[LOGERROR_LOCATION_SIZE + LOGERROR_DESCR_SIZE + 32];   // Add space for the error code

      if (ERROR_T == ee.type)
      {
         (void)snprintf(error_event_str, sizeof(error_event_str), "%s @ %s: %s",
                                                                  ee.data.error_data.error_code,
                                                                  ee.data.error_data.location,
                                                                  ee.data.error_data.description);
         (void)fprintf(logfileHandle, fmt, timestr_colAB, "", error_event_str);
         (void)fflush(logfileHandle);

         // Also record the error in the most recent 25 errors log.
         logAppendRecentErrors(ee.timestamp, error_event_str);
      }
      else if (COMMAND_EVENT_T == ee.type)
      {
         switch (ee.data.command_event_data.command_data_len)
         {
            default:
            case 0:
               (void)snprintf(error_event_str, sizeof(error_event_str), "command %s from %s",
                                                                        commandStr(ee.data.command_event_data.command),
                                                                        ee.data.command_event_data.sender_ip_address);
               break;

            case 1:
               (void)snprintf(error_event_str, sizeof(error_event_str), "command %s from %s: %d",
                                                                        commandStr(ee.data.command_event_data.command),
                                                                        ee.data.command_event_data.sender_ip_address,
                                                                        ee.data.command_event_data.command_data[0]);
               break;

            case 2:
               (void)snprintf(error_event_str, sizeof(error_event_str), "command %s from %s: %d, %d",
                                                                        commandStr(ee.data.command_event_data.command),
                                                                        ee.data.command_event_data.sender_ip_address,
                                                                        ee.data.command_event_data.command_data[0],
                                                                        ee.data.command_event_data.command_data[1]);
               break;

            case 3:
               (void)snprintf(error_event_str, sizeof(error_event_str), "command %s from %s: %d, %d, %d",
                                                                        commandStr(ee.data.command_event_data.command),
                                                                        ee.data.command_event_data.sender_ip_address,
                                                                        ee.data.command_event_data.command_data[0],
                                                                        ee.data.command_event_data.command_data[1],
                                                                        ee.data.command_event_data.command_data[2]);
               break;
         }

         (void)fprintf(logfileHandle, fmt, timestr_colAB, error_event_str, "");
         (void)fflush(logfileHandle);
      }
      else if (INTERNAL_EVENT_T == ee.type)
      {
         (void)snprintf(error_event_str, sizeof(error_event_str), "internal: %s",
                                                                  internalStr(ee.data.internal_event_data.event));
         (void)fprintf(logfileHandle, fmt, timestr_colAB, error_event_str, "");
         (void)fflush(logfileHandle);
      }
      else
      {
         // Some undefined data, there's nothing to do.
      }

      (void)fclose(logfileHandle);
   }
}


/**
 * @brief Write a status record to the status/error/event log file.
 *
 * @param[in] fileName The name of the log file.
 */
static void logStatusWrite(const char *fileName)
{
   FILE *logfileHandle = logfileOpen(fileName);
   if (0 != logfileHandle)
   {
      // Get the date and time string for columns A and B. Format them with "," between them to
      // make it easy to use it in the CSV output.
      struct tm nowtm;
      nowLocalGet(&nowtm);
      char timestr_colAB[TIME_STR_SIZE];     // Long enough to hold mm-dd-yyyy","hh:MM:ss AM.
      size_t gen_len = strftime(timestr_colAB, sizeof(timestr_colAB), "%m/%d/%Y\",\"%H:%M:%S", &nowtm);
      assert(0 != gen_len);

      // Calculate the UTC offset in the proper format: +/-mm:ss.
      int utc_offset_minutes = abs(nowtm.tm_gmtoff) % 60;
      int utc_offset_hours = nowtm.tm_gmtoff / 60;
      char utc_offset_str[UTC_OFFSET_SIZE];
      (void)snprintf(utc_offset_str, sizeof(utc_offset_str), "%+03d:%02d", utc_offset_hours, utc_offset_minutes);

      // Write the status record.
      (void)fprintf(logfileHandle,
               "\"%s\",\"%s\",\"%d\","                               // Date and time, UTC offset, DST enabled.
               "\"%s\","                                             // Status (clean/normal).
               "\"%s\",\"%s\","                                      // Fan 1 and Fan 2 status.
               "\"%d\",\"%d\","                                      // Ambient and heat sink temperatures.
               "\"%.0f\",\"%.1f\","                                  // Voltage and current.
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 1 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 2 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 3 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 4 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 5 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"%s\",\"%s\",\"%d\",\"%d\",\"%s\",\"%d\",\"%d\","   // Shelf 6 status, upper(status, setpoint, temp), lower(status, setpoint, temp).
               "\"\",\"\""                                           // Events, error. These fields are empty in a status message.
               "\n",                                                 // Terminate the record with a newline.
               timestr_colAB,                                        // Date and time.
               utc_offset_str,                                       // UTC offset as +/-mmss.
               (nowtm.tm_isdst > 0) ? 1 : 0,                         // DST enabled.
               systemStatusStr(),                                    // System status.
               0 == fanInfo[0].fan_on ? "OFF" : "ON",                // Fan 1 status.
               0 == fanInfo[1].fan_on ? "OFF" : "ON",                // Fan 2 status.
               0,                                                    // Ambient temperature. TODO: Fill this in once we're measuring it.
               heatsinkTemp,                                         // Heatsink temperature.
               voltage, irms,                                        // Voltage and current.
               slotStatusStr(0),                                     // Shelf 1 status.
               heaterStatusStr(0),                                   // Shelf 1 upper heater status.
               heaterInfo[0].temperature_setpoint,                   // Shelf 1 upper heater setpoint.
               heaterInfo[0].current_temperature,                    // Shelf 1 upper heater temperature.
               heaterStatusStr(1),                                   // Shelf 1 lower heater status.
               heaterInfo[1].temperature_setpoint,                   // Shelf 1 lower heater setpoint.
               heaterInfo[1].current_temperature,                    // Shelf 1 lower heater temperature.

               slotStatusStr(1),                                     // Shelf 2 status.
               heaterStatusStr(2),                                   // Shelf 2 upper heater status.
               heaterInfo[2].temperature_setpoint,                   // Shelf 2 upper heater setpoint.
               heaterInfo[2].current_temperature,                    // Shelf 2 upper heater temperature.
               heaterStatusStr(3),                                   // Shelf 2 lower heater status.
               heaterInfo[3].temperature_setpoint,                   // Shelf 2 lower heater setpoint.
               heaterInfo[3].current_temperature,                    // Shelf 2 lower heater temperature.

               slotStatusStr(2),                                     // Shelf 3 status.
               heaterStatusStr(4),                                   // Shelf 3 upper heater status.
               heaterInfo[4].temperature_setpoint,                   // Shelf 3 upper heater setpoint.
               heaterInfo[4].current_temperature,                    // Shelf 3 upper heater temperature.
               heaterStatusStr(5),                                   // Shelf 3 lower heater status.
               heaterInfo[5].temperature_setpoint,                   // Shelf 3 lower heater setpoint.
               heaterInfo[5].current_temperature,                    // Shelf 3 lower heater temperature.

               slotStatusStr(3),                                     // Shelf 4 status.
               heaterStatusStr(6),                                   // Shelf 4 upper heater status.
               heaterInfo[6].temperature_setpoint,                   // Shelf 4 upper heater setpoint.
               heaterInfo[6].current_temperature,                    // Shelf 4 upper heater temperature.
               heaterStatusStr(7),                                   // Shelf 4 lower heater status.
               heaterInfo[7].temperature_setpoint,                   // Shelf 4 lower heater setpoint.
               heaterInfo[7].current_temperature,                    // Shelf 4 lower heater temperature.

               slotStatusStr(4),                                     // Shelf 5 status.
               heaterStatusStr(8),                                   // Shelf 5 upper heater status.
               heaterInfo[8].temperature_setpoint,                   // Shelf 5 upper heater setpoint.
               heaterInfo[8].current_temperature,                    // Shelf 5 upper heater temperature.
               heaterStatusStr(9),                                   // Shelf 5 lower heater status.
               heaterInfo[9].temperature_setpoint,                   // Shelf 5 lower heater setpoint.
               heaterInfo[9].current_temperature,                    // Shelf 5 lower heater temperature.

               slotStatusStr(5),                                     // Shelf 6 status.
               heaterStatusStr(10),                                  // Shelf 6 upper heater status.
               heaterInfo[10].temperature_setpoint,                  // Shelf 6 upper heater setpoint.
               heaterInfo[10].current_temperature,                   // Shelf 6 upper heater temperature.
               heaterStatusStr(11),                                  // Shelf 6 lower heater status.
               heaterInfo[11].temperature_setpoint,                  // Shelf 6 lower heater setpoint.
               heaterInfo[11].current_temperature                    // Shelf 6 lower heater temperature.
               );

      (void)fflush(logfileHandle);
      (void)fclose(logfileHandle);
   }
}


/**
 * @brief Report a command event to the logger.
 *
 * @param[in] command The command to log.
 *
 * @param[in] sender_ip_address The IP address of the command sender.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address)
{
   error_event_t ee;
   ee.type = COMMAND_EVENT_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.command_event_data.command = command;
   (void)strncpy(ee.data.command_event_data.sender_ip_address, sender_ip_address.c_str(), sizeof(ee.data.command_event_data.sender_ip_address));
   ee.data.command_event_data.command_data_len = 0;
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Report a command event to the logger.
 *
 * Command data meaning depends on the command.
 *
 * @param[in] command The command to log.
 *
 * @param[in] sender_ip_address The IP address of the command sender.
 *
 * @param[in] data1 The first command data value.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1)
{
   error_event_t ee;
   ee.type = COMMAND_EVENT_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.command_event_data.command = command;
   (void)strncpy(ee.data.command_event_data.sender_ip_address, sender_ip_address.c_str(), sizeof(ee.data.command_event_data.sender_ip_address));
   ee.data.command_event_data.command_data_len = 1;
   ee.data.command_event_data.command_data[0] = data1;
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Report a command event to the logger.
 *
 * Command data meaning depends on the command.
 *
 * @param[in] command The command to log.
 *
 * @param[in] sender_ip_address The IP address of the command sender.
 *
 * @param[in] data1 The first command data value.
 *
 * @param[in] data2 The second command data value.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1, int data2)
{
   error_event_t ee;
   ee.type = COMMAND_EVENT_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.command_event_data.command = command;
   (void)strncpy(ee.data.command_event_data.sender_ip_address, sender_ip_address.c_str(), sizeof(ee.data.command_event_data.sender_ip_address));
   ee.data.command_event_data.command_data_len = 2;
   ee.data.command_event_data.command_data[0] = data1;
   ee.data.command_event_data.command_data[1] = data2;
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Report a command event to the logger.
 *
 * Command data meaning depends on the command.
 *
 * @param[in] command The command to log.
 *
 * @param[in] sender_ip_address The IP address of the command sender.
 *
 * @param[in] data1 The first command data value.
 *
 * @param[in] data2 The second command data value.
 *
 * @param[in] data3 The third command data value.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logCommandEvent(uhc::SystemCommands command, const ::std::string &sender_ip_address, int data1, int data2, int data3)
{
   error_event_t ee;
   ee.type = COMMAND_EVENT_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.command_event_data.command = command;
   (void)strncpy(ee.data.command_event_data.sender_ip_address, sender_ip_address.c_str(), sizeof(ee.data.command_event_data.sender_ip_address));
   ee.data.command_event_data.command_data_len = 3;
   ee.data.command_event_data.command_data[0] = data1;
   ee.data.command_event_data.command_data[1] = data2;
   ee.data.command_event_data.command_data[2] = data3;
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Report an internal event to the logger.
 *
 * @param[in] event The internal event to log.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logInternalEvent(internal_event_t event)
{
   error_event_t ee;
   ee.type = INTERNAL_EVENT_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.internal_event_data.event = event;
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Report an error to the logger.
 *
 * @param[in] error_code A pointer to the error code string. Must persist.
 *
 * @param[in] location A pointer to the location string. Not required to persist.
 *
 * @param[in] location A pointer to the description string. Not required to persist.
 *
 * @retval 0 No error occurred.
 * @retval ETIMEDOUT The write to the log queue timed out.
 */
static int logError(char const *error_code, char const *location, char const *description)
{
   error_event_t ee;
   ee.type = ERROR_T;
   (void)gettimeofday(&ee.timestamp, NULL);
   ee.data.error_data.error_code = error_code;
   (void)strncpy(ee.data.error_data.location, location, sizeof(ee.data.error_data.location));
   (void)strncpy(ee.data.error_data.description, description, sizeof(ee.data.error_data.description));
   int status = log_queue.put(ee, LOGEVENT_TIMEOUT_MS);

   return status;
}


/**
 * @brief Append the given error event to the most recent 25 errors log.
 *
 * @param[in] timestamp The error's timestamp.
 *
 * @param[in] errorstr The pre-formatted error string.
 */
static void logAppendRecentErrors(struct timeval timestamp, const char *errorstr)
{
   time_t errortime = timestamp.tv_sec;
   struct tm errortm;
   (void)localtime_r(&errortime, &errortm);
   char timestr[TIME_STR_SIZE];    // Long enough to hold mm-dd-yyyy hh:MM:ss AM.
   size_t gen_len = strftime(timestr, sizeof(timestr), "%m-%d-%Y %I:%M:%S %p", &errortm);
   assert(0 != gen_len);

   // timestr + error code + location + error description + field separators and some padding.
   char errorrec[TIME_STR_SIZE + ERROR_CODE_LENGTH + LOGERROR_LOCATION_SIZE + LOGERROR_DESCR_SIZE + 10];
   (void)snprintf(errorrec, sizeof(errorrec), "%s %s", timestr, errorstr);

   // Create a new recent errors file containing just this new error record.
   string rmcmd("rm -f " RECENT_ERRORS_TEMP_FILE);
   (void)system(rmcmd.c_str());

   string createcmd("echo ");
   createcmd += errorrec;
   createcmd += " > " RECENT_ERRORS_TEMP_FILE;
   (void)system(createcmd.c_str());

   // Append the most recent 24 errors from the recent errors log file.
   string appendcmd("head -n 24 " RECENT_ERRORS_FILE " >> " RECENT_ERRORS_TEMP_FILE);
   (void)system(appendcmd.c_str());

   // Move the new recent errors log file to the log directory.
   string mvcmd("mv -f " RECENT_ERRORS_TEMP_FILE " " RECENT_ERRORS_FILE);
   (void)system(mvcmd.c_str());
}

/**
 * @brief Stop the status / error / event logger thread.
 *
 * This is intended to quickly stop the logger thread during a soft power down, to ensure files
 * are closed when the power to the SD card and eMMC eventually disappears.
 */
static void logStop()
{
   static const error_event_t ee_stop =
   {
      .type = STOP_T
   };

   (void)log_queue.shove_front(ee_stop);

   (void)printf("logStop completed\n");
}


/**
 * @brief The status / error / event logger thread.
 *
 * Reads the log queue with a timeout (default 3 seconds). If there's something in the queue,
 * writes it as an error or event record to the log. If it times out, writes a status record
 * to the log.
 *
 * If the log file cannot be opened (i.e. the SD card is not present), the logger thread needs
 * to continue pulling items from the log queue so that the other threads don't get clogged
 * up when they attempt to write to the queue.
 *
 * @return pthread_exit(NULL)
 */
void *loggerThread(void *)
{
   (void)printf("loggerThread starting\n");

   date_t currentDate = currentDateGet();
   char fileName[MAX_FILE_PATH];
   logfileNameCreate(fileName, sizeof(fileName), currentDate);

   // When starting up, write a header to the log file.
   logHeaderWrite(fileName);

   using namespace std::chrono;
   steady_clock::time_point next_log_time(time_point_cast<milliseconds>(steady_clock::now()) + seconds(EESLOGINTERVAL_SEC));

   numThreadsRunning++;
   while (!sigTermReceived)
   {
      // Read the log queue.
      error_event_t ee;
      int queue_status = log_queue.get(ee, next_log_time);

      // First, check the type to see if we're supposed to stop.
      if (STOP_T == ee.type)
      {
         // The log file is currently closed. Just kill the thread.
         (void)printf("loggerThread received STOP\n");
         break;
      }

      // Before we even decide what to do, we need to see if the current log file is still valid.
      // If we rolled over to the next day, or if the system clock got changed and it's now a different
      // day, we need to switch to a different log file.
      date_t today = currentDateGet();
      if (!datesEqual(currentDate, today))
      {
         currentDate = today;

         // We're on a new day. Get the log file name for today.
         logfileNameCreate(fileName, sizeof(fileName), currentDate);

         // Since we're opening a new log file, write the log file header.
         logHeaderWrite(fileName);
      }

      if (0 == queue_status)
      {
         // An error or event arrived. Log it.
         logErrorEventWrite(fileName, ee);
      }
      else if (-ETIMEDOUT == queue_status)
      {
         // We hit the timeout. Write a standard log record.
         logStatusWrite(fileName);

         // Bump the next log time.
         next_log_time += seconds(EESLOGINTERVAL_SEC);
      }
      else
      {
         // Something else terminated the get(). Ignore it.
      }
   }

   (void)write(fanInfo[0].fd_on, FAN_ON_STRING, 1);
   (void)printf("loggerThread stopped\n");

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *timeSyncSubscriberThread(void *)                                                  */
/*                                                                                         */
/* Receive the TimeSync message from GUI1 and set the controller's clock from it.          */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *timeSyncSubscriberThread(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   char guiIPandPort[IP_STRING_SIZE + 14];   // Allow space for the tcp:// and :portnumber
   (void)memset(guiIPandPort, 0, sizeof(guiIPandPort));
   (void)snprintf(guiIPandPort, sizeof(guiIPandPort), "tcp://%s:%d", guiIPAddress1, TIME_SYNC_PORT_GUI1);
   (void)printf("timeSyncSubscriberThread guiIPandPort = %s\n", guiIPandPort);

   timeSyncSubscriber = zmq_socket(context, ZMQ_SUB);
   assert(timeSyncSubscriber != 0);

   char messageType[] = { 10, 4, 'T', 'I', 'M', 'E' };
   int rc = zmq_setsockopt(timeSyncSubscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(timeSyncSubscriber, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(timeSyncSubscriber, guiIPandPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));
      rc = zmq_recv(timeSyncSubscriber, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      TimeSync deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "timeSyncSubscriberThread ERROR: Unable to deserialize!");
            (void)logError("", "timeSyncSubscriberThread", "unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n\n";
         deserialized.PrintDebugString();

         cout << "deserialized current time: " << TimeUtil::ToString(deserialized.current_time()) << "\n";
      }

      string dateString = TimeUtil::ToString(deserialized.current_time());
      string cmd = "date --set ";
      cmd += dateString;
      timeSyncReceived = true;
      if (debugPrintf)
      {
         (void)printf("TimeSync setting time to %s\n", dateString.c_str());
      }

      syslog(LOG_INFO, "TimeSync setting time to %s", dateString.c_str());
      (void)system(cmd.c_str());
      (void)logInternalEvent(TIMESYNC_RECEIVED_T);

      if (deserialized.time_zone().length())
      {
         char commandLine[COMMAND_LINE_BUFFER_SIZE];
         (void)memset(commandLine, 0, sizeof(commandLine));
         (void)snprintf(commandLine, sizeof(commandLine) - 1, SET_TIMEZONE_COMMAND, deserialized.time_zone().c_str());
         (void)system(commandLine);
         timeZoneConfigured = true;
      }

      lastTimeGUI1Heard = 0;
      gui1MissingOneShot = true;
      bothGuisMissingOneShot = true;
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *timeSyncSubscriberThread2(void *)                                                 */
/*                                                                                         */
/* Receive the TimeSync message from GUI2 and set the controller's clock from it.          */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *timeSyncSubscriberThread2(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   char guiIPandPort[IP_STRING_SIZE + 14];   // Allow space for the tcp:// and :portnumber
   (void)memset(guiIPandPort, 0, sizeof(guiIPandPort));
   (void)snprintf(guiIPandPort, sizeof(guiIPandPort), "tcp://%s:%d", guiIPAddress2, TIME_SYNC_PORT_GUI2);
   (void)printf("timeSyncSubscriberThread2 guiIPandPort = %s\n", guiIPandPort);

   timeSyncSubscriber2 = zmq_socket(context, ZMQ_SUB);
   assert(timeSyncSubscriber2 != 0);

   char messageType[] = { 10, 4, 'T', 'I', 'M', 'E' };
   int rc = zmq_setsockopt(timeSyncSubscriber2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(timeSyncSubscriber2, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(timeSyncSubscriber2, guiIPandPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));
      rc = zmq_recv(timeSyncSubscriber2, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      TimeSync deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "timeSyncSubscriberThread2 ERROR: Unable to deserialize!");
            (void)logError("", "timeSyncSubscriberThread2", "unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n\n";
         deserialized.PrintDebugString();

         cout << "deserialized current time: " << TimeUtil::ToString(deserialized.current_time()) << "\n";
      }

      string dateString = TimeUtil::ToString(deserialized.current_time());
      string cmd = "date --set ";
      cmd += dateString;
      timeSyncReceived = true;
      if (debugPrintf)
      {
         (void)printf("TimeSync2 setting time to %s\n", dateString.c_str());
      }

      syslog(LOG_INFO, "TimeSync2 setting time to %s", dateString.c_str());
      (void)system(cmd.c_str());

      if (deserialized.time_zone().length())
      {
         char commandLine[COMMAND_LINE_BUFFER_SIZE];
         (void)memset(commandLine, 0, sizeof(commandLine));
         (void)snprintf(commandLine, sizeof(commandLine) - 1, SET_TIMEZONE_COMMAND, deserialized.time_zone().c_str());
         (void)system(commandLine);
         timeZoneConfigured = true;
      }

      lastTimeGUI2Heard = 0;
      gui2MissingOneShot = true;
      bothGuisMissingOneShot = true;
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* uhc::SystemCommandResponses processCommand((SystemCommand systemCommand)                */
/*                                                                                         */
/* Process received command from either of the GUIs.                                       */
/*                                                                                         */
/* Returns: uhc::SystemCommandResponses                                                    */
/*                                                                                         */
/*******************************************************************************************/
uhc::SystemCommandResponses processCommand(SystemCommand systemCommand)
{
   uhc::SystemCommandResponses ret = SYSTEM_COMMAND_RESPONSE_BAD_PARAMETER;
   int funcRet = 0;
   int channelIndexTopHeater = 0;
   int channelIndexBottomHeater = 0;
   uint16_t newTemp;
   google::protobuf::Timestamp timestamp;
   timestamp.set_seconds(0);
   timestamp.set_nanos(0);

   switch (systemCommand.command())
   {
      case SYSTEM_COMMAND_HEATER_ON:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.heater_index());
         // turning on a heater really only enables it for the heater algorithm
         // which has to abide by the current draw limits
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_HEATER_ON from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Safety - SYSTEM_COMMAND_HEATER_ON from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Heater ON command", descr_str);
         }
         else
         {
            funcRet = enableDisableHeater(systemCommand.heater_index(), HEATER_ENABLED);
            funcRet |= setHeaterTimesIndex(systemCommand.heater_index(), systemCommand.start_time(), systemCommand.end_time());
            if (0 == funcRet)
            {
               ret = SYSTEM_COMMAND_RESPONSE_OK;
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_HEATER_ON from %s received for heater %u", systemCommand.sender_ip_address().c_str(), systemCommand.heater_index());
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
               syslog(LOG_ERR, "SYSTEM_COMMAND_HEATER_ON from %s failed for heater %u", systemCommand.sender_ip_address().c_str(), systemCommand.heater_index());
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "Failed to turn ON heater %u", systemCommand.heater_index());
               (void)logError("", "Heater ON command", descr_str);
            }
         }
         break;

      case SYSTEM_COMMAND_HEATER_OFF:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.heater_index());
         // turning off a heater really disables both heaters for a slot
         funcRet = turnHeaterOnOff(systemCommand.heater_index(), HEATER_STATE_OFF);
         funcRet |= setHeaterTimesIndex(systemCommand.heater_index(), timestamp, timestamp);
         funcRet |= enableDisableHeater(systemCommand.heater_index(), HEATER_DISABLED);
         if (0 == funcRet)
         {
            ret = SYSTEM_COMMAND_RESPONSE_OK;
            syslog(LOG_NOTICE, "SYSTEM_COMMAND_HEATER_OFF from %s received for heater %u", systemCommand.sender_ip_address().c_str(), systemCommand.heater_index());
         }
         else
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "SYSTEM_COMMAND_HEATER_OFF from %s failed for heater %u", systemCommand.sender_ip_address().c_str(), systemCommand.heater_index());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_HEATER_OFF from %s failed for heater %u", systemCommand.sender_ip_address().c_str(), systemCommand.heater_index());
            (void)logError("", "Heater OFF command", descr_str);
         }
         break;

      case SYSTEM_COMMAND_SHUTDOWN_REQUESTED:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         // turn all heaters off
         for (int i = SLOT1_TOP_HEATER_INDEX; i < NUM_HEATERS; i++)
         {
            funcRet |= turnHeaterOnOff(i, HEATER_STATE_OFF);
            funcRet |= enableDisableHeater(i, HEATER_DISABLED);
         }

         if (0 == funcRet)
         {
            ret = SYSTEM_COMMAND_RESPONSE_SHUTDOWN_PENDING;
            shutdownRequested = true;
            (void)system(TOUCH_SOFT_SHUTDOWN_FILE);
            syslog(LOG_NOTICE, "SYSTEM_COMMAND_SHUTDOWN_REQUESTED from %s received", systemCommand.sender_ip_address().c_str());
         }
         else
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "SYSTEM_COMMAND_SHUTDOWN_REQUESTED from %s failed", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_SHUTDOWN_REQUESTED from %s failed", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Shutdown command", descr_str);
         }
         break;

      case SYSTEM_COMMAND_EMERGENCY_STOP:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         // turn all heaters off
         for (int i = SLOT1_TOP_HEATER_INDEX; i < NUM_HEATERS; i++)
         {
            funcRet |= enableDisableHeater(i, HEATER_DISABLED);
            funcRet |= turnHeaterOnOff(i, HEATER_STATE_OFF);
         }

         if (0 == funcRet)
         {
            ret = SYSTEM_COMMAND_RESPONSE_OK;
            syslog(LOG_NOTICE, "SYSTEM_COMMAND_EMERGENCY_STOP received from %s", systemCommand.sender_ip_address().c_str());
         }
         else
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "SYSTEM_COMMAND_EMERGENCY_STOP from %s failed", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_EMERGENCY_STOP from %s failed", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Emergency Stop command", descr_str);
         }
         break;

      case SYSTEM_COMMAND_STARTUP:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         // pre-heat all of the slots to the desired setpoint as quickly as possible
         // has to be within 25 minutes +/- 5 minutes.
         // also, enable all of the heaters
         startupMessageReceived = true;
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_STARTUP from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Startup command", "Safety - startup command when cleaning mode is ON");
         }
         else
         {
            for (int i = 0; i < NUM_HEATERS; i++)
            {
               funcRet |= enableDisableHeater(i, HEATER_ENABLED);
            }

            if (0 == funcRet)
            {
               ret = SYSTEM_COMMAND_RESPONSE_OK;
               systemStatus = SYSTEM_STATUS_STARTUP;
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_STARTUP received from %s System in startup", systemCommand.sender_ip_address().c_str());
               startupTimeSeconds = 0;
               maxStartupTimeExceededOneShot = true;
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_STARTUP received from %s failed", systemCommand.sender_ip_address().c_str());
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_STARTUP received from %s failed", systemCommand.sender_ip_address().c_str());
               (void)logError("", "Startup command", descr_str);
            }
         }
         break;

      case SYSTEM_COMMAND_IDLE:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_IDLE from %s", systemCommand.sender_ip_address().c_str());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         systemStatus = SYSTEM_STATUS_NORMAL;
         break;

      case SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.slot_number(),
                                                        (int)systemCommand.temperature());
         // you can only set the temperature for a slot, but there are two heaters
         // so, set the setpoint for both heaters of a slot
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Safety - SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Update Slot Temp Setpoint command", descr_str);
         }
         else
         {
            if ((systemCommand.temperature() >= setpointLowLimit) && (systemCommand.temperature() <= setpointHighLimit))
            {
               setSetpointSlot((int)systemCommand.slot_number(), (int)systemCommand.temperature());
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT from %s Temperature setpoint slot %d temp = %u", systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.temperature());
               ret = SYSTEM_COMMAND_RESPONSE_OK;
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_BAD_PARAMETER;
               syslog(LOG_ERR, "SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT from %s Temperature setpoint out of range slot %d temp = %u", systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.temperature());
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT from %s Temperature setpoint out of range slot %d temp = %u", systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.temperature());
               (void)logError("", "Update Slot Temp command", descr_str);
            }
         }
         break;

      case SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.slot_number(),
                                                        (int)systemCommand.heater_location_upper_setpoint_temperature(),
                                                        (int)systemCommand.heater_location_lower_setpoint_temperature());
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Safety - SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Set Heater Temp Setpoint command", descr_str);
         }
         else
         {
            if (  (systemCommand.heater_location_upper_setpoint_temperature() >= setpointLowLimit)
               && (systemCommand.heater_location_upper_setpoint_temperature() <= setpointHighLimit)
               && (systemCommand.heater_location_lower_setpoint_temperature() >= setpointLowLimit)
               && (systemCommand.heater_location_lower_setpoint_temperature() <= setpointHighLimit)
               )
            {
               lookupHeaterIndices(systemCommand.slot_number(), &channelIndexTopHeater, &channelIndexBottomHeater);
               funcRet =  setSetpointHeater(channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature());
               funcRet |= setSetpointHeater(channelIndexBottomHeater, systemCommand.heater_location_lower_setpoint_temperature());
               if (0 == funcRet)
               {
                  heaterInfo[channelIndexTopHeater].saved_setpoint = systemCommand.heater_location_upper_setpoint_temperature();
                  heaterInfo[channelIndexBottomHeater].saved_setpoint = systemCommand.heater_location_lower_setpoint_temperature();
                  ret = SYSTEM_COMMAND_RESPONSE_OK;
                  syslog(LOG_NOTICE, "SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s Setting Temperature setpoint heater %d temp = %u  heater %d  temp = %u",
                     systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature(), channelIndexBottomHeater,
                     systemCommand.heater_location_lower_setpoint_temperature());
               }
               else
               {
                  ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
                  syslog(LOG_ERR, "SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s Setting Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                     systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature(), channelIndexBottomHeater,
                     systemCommand.heater_location_lower_setpoint_temperature());
                  char descr_str[LOGERROR_DESCR_SIZE];
                  (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s Setting Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                                                               systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature(),
                                                               channelIndexBottomHeater, systemCommand.heater_location_lower_setpoint_temperature());
                  (void)logError("", "Set Heater Temp Setpoint command", descr_str);
               }
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_BAD_PARAMETER;
               syslog(LOG_ERR, "SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s Temperature setpoint out of range heater %d temp = %u  heater %d  temp = %u",
                     systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature(), channelIndexBottomHeater,
                     systemCommand.heater_location_lower_setpoint_temperature());
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT from %s Temperature setpoint out of range heater %d temp = %u  heater %d  temp = %u",
                                                            systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, systemCommand.heater_location_upper_setpoint_temperature(),
                                                            channelIndexBottomHeater, systemCommand.heater_location_lower_setpoint_temperature());
               (void)logError("", "Set Heater Temp Setpoint command", descr_str);
            }
         }
         break;

      case SYSTEM_COMMAND_SET_DURATION:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_SET_DURATION from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SYSTEM_COMMAND_SET_ECO_MODE_TIME:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_SET_ECO_MODE_TIME from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SYSTEM_COMMAND_SET_ECO_MODE_TEMP:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.temperature());
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_SET_ECO_MODE_TEMP from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Safety - SYSTEM_COMMAND_SET_ECO_MODE_TEMP from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "Set Eco Mode Temp command", descr_str);
         }
         else
         {
            if ((systemCommand.temperature() >= tempLookupTable[0].degreesF) && (systemCommand.temperature() <= heaterInfo[0].temperature_setpoint))
            {
               newTemp = systemCommand.temperature();
            }
            else
            {
               newTemp = DEFAULT_ECO_MODE_SETPOINT;
            }

            for (int i = SLOT1_TOP_HEATER_INDEX; i < NUM_HEATERS; i++)
            {
               heaterInfo[i].eco_mode_setpoint = newTemp;
            }

            syslog(LOG_NOTICE, "SYSTEM_COMMAND_SET_ECO_MODE_TEMP from %s temp %hu", systemCommand.sender_ip_address().c_str(), newTemp);
            ret = SYSTEM_COMMAND_RESPONSE_OK;
         }
         break;

      case SYSTEM_COMMAND_ECO_MODE_ON:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(), (int)systemCommand.slot_number());
         lookupHeaterIndices(systemCommand.slot_number(), &channelIndexTopHeater, &channelIndexBottomHeater);
         if (inCleaningMode)
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "Safety - SYSTEM_COMMAND_SET_ECO_MODE_TEMP from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "Safety - SYSTEM_COMMAND_SET_ECO_MODE_TEMP from %s when CLEANING MODE is ON", systemCommand.sender_ip_address().c_str());
            (void)logError("", "ECO mode ON command", descr_str);
         }
         else if (heaterInfo[channelIndexTopHeater].eco_mode_is_on || heaterInfo[channelIndexBottomHeater].eco_mode_is_on)
         {
            // ECO mode is already on for this slot; eat the message
               ret = SYSTEM_COMMAND_RESPONSE_OK;
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_ECO_MODE_ON from %s for slot %d but ECO mode is already on", systemCommand.sender_ip_address().c_str(), systemCommand.slot_number());
         }
         else
         {
            heaterInfo[channelIndexTopHeater].eco_mode_is_on = true;
            heaterInfo[channelIndexBottomHeater].eco_mode_is_on = true;
            heaterInfo[channelIndexTopHeater].saved_setpoint = heaterInfo[channelIndexTopHeater].temperature_setpoint;
            heaterInfo[channelIndexBottomHeater].saved_setpoint = heaterInfo[channelIndexBottomHeater].temperature_setpoint;
            heaterInfo[channelIndexTopHeater].eco_mode_on = true;
            heaterInfo[channelIndexBottomHeater].eco_mode_on = true;
            funcRet =  setSetpointHeater(channelIndexTopHeater, heaterInfo[channelIndexTopHeater].eco_mode_setpoint);
            funcRet |= setSetpointHeater(channelIndexBottomHeater, heaterInfo[channelIndexBottomHeater].eco_mode_setpoint);
            heaterInfo[channelIndexTopHeater].setpoint_changed = true;
            heaterInfo[channelIndexBottomHeater].setpoint_changed = true;
            if (0 == funcRet)
            {
               ret = SYSTEM_COMMAND_RESPONSE_OK;
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_ECO_MODE_ON from %s Setting Eco Mode Temperature setpoint heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].eco_mode_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].eco_mode_setpoint);
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
               syslog(LOG_ERR, "SYSTEM_COMMAND_ECO_MODE_ON from %s Setting Eco Mode Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].eco_mode_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].eco_mode_setpoint);
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_ECO_MODE_ON from %s Setting Eco Mode Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].eco_mode_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].eco_mode_setpoint);
               (void)logError("", "ECO mode ON command", descr_str);
            }
         }
         break;

      case SYSTEM_COMMAND_ECO_MODE_OFF:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(), (int)systemCommand.slot_number());
         lookupHeaterIndices(systemCommand.slot_number(), &channelIndexTopHeater, &channelIndexBottomHeater);
         if (heaterInfo[channelIndexTopHeater].eco_mode_is_on || heaterInfo[channelIndexBottomHeater].eco_mode_is_on)
         {
            heaterInfo[channelIndexTopHeater].temperature_setpoint = heaterInfo[channelIndexTopHeater].saved_setpoint;
            heaterInfo[channelIndexBottomHeater].temperature_setpoint = heaterInfo[channelIndexBottomHeater].saved_setpoint;
            heaterInfo[channelIndexTopHeater].eco_mode_on = false;
            heaterInfo[channelIndexBottomHeater].eco_mode_on = false;
            heaterInfo[channelIndexTopHeater].setpoint_changed = true;
            heaterInfo[channelIndexBottomHeater].setpoint_changed = true;
            funcRet =  setSetpointHeater(channelIndexTopHeater, heaterInfo[channelIndexTopHeater].temperature_setpoint);
            funcRet |= setSetpointHeater(channelIndexBottomHeater, heaterInfo[channelIndexBottomHeater].temperature_setpoint);
            if (0 == funcRet)
            {
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_ECO_MODE_OFF from %s Setting Eco Mode Off Temperature setpoint heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].temperature_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].temperature_setpoint);
               ret = SYSTEM_COMMAND_RESPONSE_OK;
            }
            else
            {
               ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
               syslog(LOG_ERR, "SYSTEM_COMMAND_ECO_MODE_OFF from %s Setting Eco Mode Off Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].temperature_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].temperature_setpoint);
               char descr_str[LOGERROR_DESCR_SIZE];
               (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_ECO_MODE_OFF from %s Setting Eco Mode Off Temperature setpoint failed heater %d temp = %u  heater %d  temp = %u",
                  systemCommand.sender_ip_address().c_str(), channelIndexTopHeater, heaterInfo[channelIndexTopHeater].temperature_setpoint, channelIndexBottomHeater,
                  heaterInfo[channelIndexBottomHeater].temperature_setpoint);
               (void)logError("", "ECO mode OFF command", descr_str);
            }
         }
         else
         {
            // ECO mode isn't on; just eat the message and return OK
               syslog(LOG_NOTICE, "SYSTEM_COMMAND_ECO_MODE_OFF from %s for slot %d but ECO mode is already off",
                  systemCommand.sender_ip_address().c_str(), systemCommand.slot_number());
               ret = SYSTEM_COMMAND_RESPONSE_OK;
         }
         break;

      case SYSTEM_COMMAND_FAN_ON:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(), (int)systemCommand.fan_number());
         switch (systemCommand.fan_number())
         {
            default:
            case FAN_NUMBER_BOTH:
            case FAN_NUMBER_UNKNOWN:
               fanInfo[FAN1_INDEX].current_limit_auto_correct_count = 0;
               fanInfo[FAN2_INDEX].current_limit_auto_correct_count = 0;
               fanInfo[FAN1_INDEX].current_limit_delay_count = 0;
               fanInfo[FAN2_INDEX].current_limit_delay_count = 0;
               funcRet = turnFansOnOff(FAN_STATE_ON);
               fan1CurrentLimitOneShot = true;
               fan1FailureOneShot = true;
               fan2CurrentLimitOneShot = true;
               fan2FailureOneShot = true;
               break;

            case FAN_NUMBER_1:
               fanInfo[FAN1_INDEX].current_limit_auto_correct_count = 0;
               fanInfo[FAN1_INDEX].current_limit_delay_count = 0;
               funcRet = setFanOnOff(FAN1_INDEX, FAN_STATE_ON);
               fan1CurrentLimitOneShot = true;
               fan1FailureOneShot = true;
               break;

            case FAN_NUMBER_2:
               fanInfo[FAN2_INDEX].current_limit_auto_correct_count = 0;
               fanInfo[FAN2_INDEX].current_limit_delay_count = 0;
               funcRet = setFanOnOff(FAN2_INDEX, FAN_STATE_ON);
               fan2CurrentLimitOneShot = true;
               fan2FailureOneShot = true;
               break;
         }

         if (FUNCTION_SUCCESS == funcRet)
         {
            syslog(LOG_NOTICE, "SYSTEM_COMMAND_FAN_ON from %s turning fan number %d on", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            ret = SYSTEM_COMMAND_RESPONSE_OK;
         }
         else
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "SYSTEM_COMMAND_FAN_ON from %s turning fan number %d on", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_FAN_ON from %s failed turning fan number %d on", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            (void)logError("", "Fan ON command", descr_str);
         }
         break;

      case SYSTEM_COMMAND_FAN_OFF:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address(),
                                                        (int)systemCommand.fan_number());
         switch (systemCommand.fan_number())
         {
            default:
            case FAN_NUMBER_BOTH:
            case FAN_NUMBER_UNKNOWN:
               funcRet = turnFansOnOff(FAN_STATE_OFF);
               break;

            case FAN_NUMBER_1:
               funcRet = setFanOnOff(FAN1_INDEX, FAN_STATE_OFF);
               break;

            case FAN_NUMBER_2:
               funcRet = setFanOnOff(FAN2_INDEX, FAN_STATE_OFF);
               break;
         }

         if (FUNCTION_SUCCESS == funcRet)
         {
            syslog(LOG_NOTICE, "SYSTEM_COMMAND_FAN_OFF from %s turning fan number %d off", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            ret = SYSTEM_COMMAND_RESPONSE_OK;
         }
         else
         {
            ret = SYSTEM_COMMAND_RESPONSE_FAILURE;
            syslog(LOG_ERR, "SYSTEM_COMMAND_FAN_OFF from %s failed turning fan number %d off", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            char descr_str[LOGERROR_DESCR_SIZE];
            (void)snprintf(descr_str, sizeof(descr_str), "SYSTEM_COMMAND_FAN_ON from %s failed turning fan number %d on", systemCommand.sender_ip_address().c_str(), systemCommand.fan_number());
            (void)logError("", "Fan ON command", descr_str);
         }
         break;

      case SYSTEM_COMMAND_CLEANING_MODE_ON:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         if (!inCleaningMode)
         {
            for (int i = SLOT1_TOP_HEATER_INDEX; i < NUM_HEATERS; i++)
            {
               heaterInfo[i].saved_setpoint = heaterInfo[i].temperature_setpoint;
               heaterInfo[i].temperature_setpoint = heaterInfo[i].cleaning_mode_setpoint;
               heaterInfo[i].setpoint_changed = true;
            }
         }

         syslog(LOG_NOTICE, "SYSTEM_COMMAND_CLEANING_MODE_ON from %s", systemCommand.sender_ip_address().c_str());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         inCleaningMode = true;
         break;

      case SYSTEM_COMMAND_CLEANING_MODE_OFF:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         if (inCleaningMode)
         {
            for (int i = SLOT1_TOP_HEATER_INDEX; i < NUM_HEATERS; i++)
            {
               heaterInfo[i].temperature_setpoint = heaterInfo[i].saved_setpoint;
               heaterInfo[i].setpoint_changed = true;
            }
         }

         syslog(LOG_NOTICE, "SYSTEM_COMMAND_CLEANING_MODE_OFF from %s", systemCommand.sender_ip_address().c_str());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         inCleaningMode = false;
         break;

      case SYSTEM_COMMAND_ESTABLISH_LINK:
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         ret = SYSTEM_COMMAND_RESPONSE_LINK_ESTABLISHED;
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_ESTABLISH_LINK from %s Link with GUI Established", systemCommand.sender_ip_address().c_str());
         break;

      case SYSTEM_COMMAND_UNKNOWN:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_UNKNOWN from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         ret = SYSTEM_COMMAND_RESPONSE_UNKNOWN;
         break;

      case SYSTEM_COMMAND_NSO_MODE_ON:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_NSO_MODE_ON from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         nsoMode = true;
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SYSTEM_COMMAND_DEMO_MODE_ON:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_DEMO_MODE_ON from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         demoMode = true;
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SYSTEM_COMMAND_DEMO_MODE_OFF:
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_DEMO_MODE_OFF from %s", systemCommand.sender_ip_address().c_str());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         demoMode = false;
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SYSTEM_COMMAND_CONFIGURE_LOGGING:
         loggingIsEventDriven = systemCommand.logging_is_event_driven();
         if (systemCommand.logging_period_seconds() != 0)
         {
            loggingPeriodSeconds = systemCommand.logging_period_seconds();
         }
         else
         {
            loggingPeriodSeconds = DEFAULT_LOGGING_PERIOD_SECONDS;
         }
         syslog(LOG_NOTICE, "SYSTEM_COMMAND_CONFIGURE_LOGGING from %s Event Driven: %d  Period in seconds: %d", systemCommand.sender_ip_address().c_str(), systemCommand.logging_is_event_driven(), systemCommand.logging_period_seconds());
         (void)logCommandEvent(systemCommand.command(), systemCommand.sender_ip_address());
         ret = SYSTEM_COMMAND_RESPONSE_OK;
         break;

      case SystemCommands_INT_MIN_SENTINEL_DO_NOT_USE_:
      case SystemCommands_INT_MAX_SENTINEL_DO_NOT_USE_:
      default:
         break;
   }

   lastCommandReceived = systemCommand.command();

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void *commandHandlerThread(void *)                                                      */
/*                                                                                         */
/* Receive the SystemCommnnd message from GUI1, process it, and send the                   */
/* SystemCommandResponse message.                                                          */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *commandHandlerThread(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   uint32_t sequenceNumber = 0;

   commandResponsePublisher = zmq_socket(context, ZMQ_PUB);
   assert(commandResponsePublisher != 0);
   (void)printf("commandHandlerThread commandResponsePublisher  = %p\n", commandResponsePublisher);

   char commandRespPubPort[IP_STRING_SIZE+ 14];    // Allow space for the tcp:// and :portnumber
   (void)memset(commandRespPubPort, 0, sizeof(commandRespPubPort));
   (void)snprintf(commandRespPubPort, sizeof(commandRespPubPort), "tcp://%s:%d", controllerIPAddress, SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER);
   (void)printf("commandHandlerThread commandRespPubPort = %s\n", commandRespPubPort);
   int rs = zmq_bind(commandResponsePublisher, commandRespPubPort);
   assert(rs == 0);

   char igCommandReqSubPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(igCommandReqSubPort, sizeof(igCommandReqSubPort), "tcp://%s:%d", guiIPAddress1, SYSTEM_COMMAND_PORT_GUI1);
   (void)printf("commandHandlerThread igCommandReqSubPort = %s\n", igCommandReqSubPort);

   commandListener = zmq_socket(context, ZMQ_SUB);
   assert(commandListener != 0);

   char messageType[] = { 10, 3, 'C', 'M', 'D' };
   int rc = zmq_setsockopt(commandListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(commandListener, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(commandListener, igCommandReqSubPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));

      rc = zmq_recv(commandListener, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      SystemCommand deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "commandHandlerThread ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to deserialize!");
            (void)logError("", "commandHandlerThread", "unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n";

         cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
         cout << "          sequence number: " << deserialized.sequence_number() << "\n";
         cout << "                  command: " << deserialized.command() << "\n";
      }

      // Handle command here
      uhc::SystemCommandResponses ret = processCommand(deserialized);

      // Publish ZeroMQ SystemCommandResponse message here
      SystemCommandResponse cr;
      cr.set_topic(SYSTEM_COMMAND_RESPONSE_TOPIC);
      cr.set_sequence_number(sequenceNumber++);
      cr.set_requester_ip_address(deserialized.sender_ip_address());
      cr.set_command(deserialized.command());
      cr.set_response(ret);
      cr.set_slot_number(deserialized.slot_number());
      cr.set_temperature(deserialized.temperature());

      string serialized;
      if (!cr.SerializeToString(&serialized))
      {
         if (debugPrintf)
         {
            cerr << "commandHandlerThread ERROR: Unable to serialize!\n";
            syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to serialize!");
            (void)logError("", "commandHandlerThread", "unable to serialize");
         }
      }

      char bytes[serialized.length()];
      (void)memcpy(bytes, serialized.data(), serialized.length());

      int rs = zmq_send(commandResponsePublisher, bytes, serialized.length(), 0);
      assert(rs == (int)serialized.length());

      if (debugPrintf)
      {
         cout << "Bytes sent: " << rs << "\n";
      }

      lastTimeGUI1Heard = 0;
      gui1MissingOneShot = true;
      bothGuisMissingOneShot = true;

      if (shutdownRequested)
      {
         (void)system(SHUTDOWN_NOW_COMMAND);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *commandHandlerThread2(void *)                                                     */
/*                                                                                         */
/* Receive the SystemCommnnd message from GUI2, process it, and send the                   */
/* SystemCommandResponse message.                                                          */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *commandHandlerThread2(void *)
{
   char message[DEFAULT_MESSAGE_BUFFER_SIZE];
   uint32_t sequenceNumber = 0;

   commandResponsePublisher2 = zmq_socket(context, ZMQ_PUB);
   assert(commandResponsePublisher2 != 0);
   (void)printf("commandHandlerThread2 commandResponsePublisher2  = %p\n", commandResponsePublisher2);

   char commandRespPubPort[IP_STRING_SIZE + 14];   // Allow space for the tcp:// and :portnumber
   (void)memset(commandRespPubPort, 0, sizeof(commandRespPubPort));
   (void)snprintf(commandRespPubPort, sizeof(commandRespPubPort), "tcp://%s:%d", controllerIPAddress, SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER2);
   (void)printf("commandHandlerThread2 commandRespPubPort = %s\n", commandRespPubPort);
   int rs = zmq_bind(commandResponsePublisher2, commandRespPubPort);
   assert(rs == 0);

   char igCommandReqSubPort[IP_STRING_SIZE + 14];  // Allow space for the tcp:// and :portnumber
   (void)snprintf(igCommandReqSubPort, sizeof(igCommandReqSubPort), "tcp://%s:%d", guiIPAddress2, SYSTEM_COMMAND_PORT_GUI2);
   (void)printf("commandHandlerThread2 igCommandReqSubPort = %s\n", igCommandReqSubPort);

   commandListener2 = zmq_socket(context, ZMQ_SUB);
   assert(commandListener2 != 0);

   char messageType[] = { 10, 3, 'C', 'M', 'D' };
   int rc = zmq_setsockopt(commandListener2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
   assert(rc == 0);

   int noTimeout = -1;
   rc = zmq_setsockopt(commandListener2, ZMQ_RCVTIMEO, &noTimeout, sizeof(noTimeout));
   assert(rc == 0);

   rc = zmq_connect(commandListener2, igCommandReqSubPort);
   assert(rc == 0);

   (void)sleep(1);

   numThreadsRunning++;
   while(!sigTermReceived)
   {
      (void)memset(message, 0, sizeof(message));

      rc = zmq_recv(commandListener2, message, sizeof(message), 0);
      if (rc == -1)
      {
         continue;
      }

      // Deserialization
      string s(message, rc);
      SystemCommand deserialized;
      if (!deserialized.ParseFromString(s))
      {
         if (debugPrintf)
         {
            cerr << "commandHandlerThread2 ERROR: Unable to deserialize!\n";
            syslog(LOG_ERR, "commandHandlerThread2 ERROR: Unable to deserialize!");
            (void)logError("", "commandHandlerThread2", "unable to deserialize");
         }
      }

      if (debugPrintf)
      {
         cout << "Deserialization:\n";

         cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
         cout << "          sequence number: " << deserialized.sequence_number() << "\n";
         cout << "                  command: " << deserialized.command() << "\n";
      }

      // Handle command here
      uhc::SystemCommandResponses ret = processCommand(deserialized);

      // Publish ZeroMQ SystemCommandResponse message here
      SystemCommandResponse cr;
      cr.set_topic(SYSTEM_COMMAND_RESPONSE_TOPIC);
      cr.set_sequence_number(sequenceNumber++);
      cr.set_requester_ip_address(deserialized.sender_ip_address());
      cr.set_command(deserialized.command());
      cr.set_response(ret);
      cr.set_slot_number(deserialized.slot_number());
      cr.set_temperature(deserialized.temperature());

      string serialized;
      if (!cr.SerializeToString(&serialized))
      {
         if (debugPrintf)
         {
            cerr << "commandHandlerThread2 ERROR: Unable to serialize!\n";
            syslog(LOG_ERR, "commandHandlerThread2 ERROR: Unable to serialize!");
            (void)logError("", "commandHandlerThread2", "unable to serialize");
         }
      }

      char bytes[serialized.length()];
      (void)memcpy(bytes, serialized.data(), serialized.length());

      int rs = zmq_send(commandResponsePublisher2, bytes, serialized.length(), 0);
      assert(rs == (int)serialized.length());

      if (debugPrintf)
      {
         cout << "Bytes sent: " << rs << "\n";
      }

      lastTimeGUI2Heard = 0;
      gui2MissingOneShot = true;
      bothGuisMissingOneShot = true;

      if (shutdownRequested)
      {
         (void)system(SHUTDOWN_NOW_COMMAND);
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}

void *softPowerdownThread(void *)
{
   (void)printf("softPowerdownThread starting\n");

   int fd = open(WARN_OUT_PULSE_VALUE, O_RDONLY);
   if (fd >= 1)
   {
      char value;
      int poll_ret;
      struct pollfd poll_gpio;
      poll_gpio.fd = fd;
      poll_gpio.events = POLLPRI | POLLERR;
      poll_gpio.revents = 0;

      // The GPIO has already been configured as input, with interrupt enabled on rising edge.
      (void)read(fd, &value, 1);

      bool sag_detected = false;
	  numThreadsRunning++;
      while (!sag_detected && !sigTermReceived)
      {
         // Block until there's a change in value.
         (void)read(fd, &value, 1);
         (void)lseek(fd, 0, SEEK_SET);
         poll_ret = poll(&poll_gpio, 1, -1);

         // The return value will not be 0 (timeout) since we specified to wait forever.
         // The return value could be negative, meaning the poll was interrupted.
         if (1 == poll_ret)
         {
            // Value changed.
            cout << "Voltage sag detected." << endl;
            sag_detected = true;

            // First, turn off I/Os to reduce current draw on the 12V supply, to increase
            // BBB run time.
            int fd_220vac = open(HEATER_220VAC_RELAY, O_WRONLY);
            if (fd_220vac >= 1)
            {
               (void)write(fd_220vac, VAC220_OFF_STRING, 1);
            }

            for (int i = 0; i < NUM_FANS; ++i)
            {
               (void)write(fanInfo[i].fd_on, FAN_OFF_STRING, 1);
            }

            for (int i = 0; i < NUM_HEATERS; ++i)
            {
               enableDisableHeater(i, false);
               (void)write(heaterInfo[i].fd, HEATER_OFF_STRING, 1);
            }

            // TODO: Figure out if there's a good way to disable SPI peripherals and set SPI pins low.

            // Stop the kernel logging service.
            closelog();
            (void)system(STOP_KERNEL_LOG_CMD);

            // Stop our status/error/event logging thread.
            logStop();

            cout << "syslog stopped, internal logging stop signaled" << endl;
         }
      }
   }

   numThreadsRunning--;
   pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* int runSelfTests()                                                                      */
/*                                                                                         */
/* Run the self tests.                                                                     */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int runSelfTests()
{
   char topString[] = {"TOP"};
   char bottomString[] = {"BOTTOM"};
   char *locationString = NULL;

   // turn on the fans
   int ret = turnFansOnOff(FAN_STATE_ON);
   // read the RTDs and check for open or short
   for (int i = 0; i < NUM_HEATERS; i++)
   {
      int rawCounts = readADCChannel(i);
      if (HEATER_POSITION_TOP == heaterInfo[i].location)
      {
         locationString = topString;
      }
      else
      {
         locationString = bottomString;
      }

      // if the counts are above 340 degree counts, call that open
      if (rawCounts > rtdMappings[i].temp_lookup_table[NUM_TEMP_LOOKUP_ENTRIES-10].adc_raw_counts)
      {
         rtdMappings[i].is_open = true;
         heaterInfo[i].is_enabled = false;
         syslog(LOG_ERR, "%s Temp probe open failure for heater %d location %s - heater disabled", TEMP_PROBE_OPEN_ERROR_CODE, i, locationString);
         (void)strncpy(errorCodeString, TEMP_PROBE_OPEN_ERROR_CODE, sizeof(errorCodeString) - 1);
         systemStatus = SYSTEM_STATUS_ERROR;
         ret = 1;
      }

      // if the counts are less than the first entry in the temp table, call that "shorted"
      if (rawCounts < rtdMappings[i].temp_lookup_table[0].adc_raw_counts)
      {
         rtdMappings[i].is_shorted = true;
         heaterInfo[i].is_enabled = false;
         syslog(LOG_ERR, "%s Temp probe closed failure for heater %d - heater disabled", TEMP_PROBE_CLOSED_ERROR_CODE, i);
         (void)strncpy(errorCodeString, TEMP_PROBE_CLOSED_ERROR_CODE, sizeof(errorCodeString) - 1);
         systemStatus = SYSTEM_STATUS_ERROR;
         ret = 1;
      }
   }

   // check the power monitor for AC line voltage
   float volts = 0.0;
   int j = 0;
   vrms_retries = 0;
   (void)atm90e26_vrms_get(&volts);
   for (j = 0; (j < POWER_METER_MAX_READS) && (0.0 == volts); j++)
   {
      (void)atm90e26_vrms_get(&volts);
      if (j > 0)
      {
         vrms_retries++;
      }
   }
   if (POWER_METER_MAX_READS == j)
   {
      powerMonitorBad = true;
      ret = 1;
   }

   // delay 3 seconds to allow the fans to start up correctly
   sleep(3);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initializeZeroMQ()                                                                  */
/*                                                                                         */
/* Initialize ZeroMQ. Create the necessary ZMQ contexts                                    */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int initializeZeroMQ()
{
   int ret = 0;

   // create the contexts for ZeroMQ
   context = zmq_ctx_new();
   if (context == NULL)
   {
      ret = 1;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initializeHardware()                                                                */
/*                                                                                         */
/* Initialize all of the controller hardware.                                              */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int initializeHardware()
{
   int ret = initBoardRevIDGPIOs();
   ret |= initUnusedGPIOs();
   ret |= initFanGPIOs();
   ret |= initHeaterGPIOs();
   ret |= initSPIGPIOs();
   ret |= initPowerMonGPIOs();
   ret |= atm90e26_init(POWER_METER_SPI_DEVICE, POWER_METER_DEFAULT_SPI_SPEED);
   ret |= atm90e26_start(lgain_16, LINE_SAG_VOLTS);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getSetpointLimits()                                                                 */
/*                                                                                         */
/* Read the lower and upper setpoint limit files.                                          */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int getSetpointLimits()
{
   int ret = 0;

   FILE *fp = fopen(SETPOINT_LOW_LIMIT_FILE, "r");
   if (fp != NULL)
   {
      (void)memset(setpointLowLimitText, 0, sizeof(setpointLowLimitText)); // make sure NULL terminated
      int bytesRead = fread(setpointLowLimitText, 1, SETPOINT_LIMIT_SIZE-1, fp);
      if (bytesRead > 0)
      {
         // setpoint Low Limit
         setpointLowLimit = (uint16_t)atoi(setpointLowLimitText);
      }

      (void)fclose(fp);
   }

   fp = fopen(SETPOINT_HIGH_LIMIT_FILE, "r");
   if (fp != NULL)
   {
      (void)memset(setpointHighLimitText, 0, sizeof(setpointHighLimitText));  // make sure NULL terminated
      int bytesRead = fread(setpointHighLimitText, 1, SETPOINT_LIMIT_SIZE-1, fp);
      if (bytesRead > 0)
      {
         // setpoint Low Limit
         setpointHighLimit = (uint16_t)atoi(setpointHighLimitText);
      }

      (void)fclose(fp);
   }

   if (setpointLowLimit < DEFAULT_SETPOINT_LOW_LIMIT)
   {
      setpointLowLimit = DEFAULT_SETPOINT_LOW_LIMIT;
   }

   if (setpointHighLimit > DEFAULT_SETPOINT_HIGH_LIMIT)
   {
      setpointHighLimit = DEFAULT_SETPOINT_HIGH_LIMIT;
   }

   if ((setpointHighLimit <= setpointLowLimit) || (setpointLowLimit >= setpointHighLimit))
   {
      setpointLowLimit = DEFAULT_SETPOINT_LOW_LIMIT;
      setpointHighLimit = DEFAULT_SETPOINT_HIGH_LIMIT;
   }

   (void)printf("SETPOINT LIMITS   Low = %d  High = %d\n", setpointLowLimit, setpointHighLimit);

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getSerialAndModel()                                                                 */
/*                                                                                         */
/* Read the serial number and device model name files.                                     */
/*                                                                                         */
/* Returns: int ret - 0=success, 1 = failure                                               */
/*                                                                                         */
/*******************************************************************************************/
int getSerialAndModel()
{
   int ret = 0;

   FILE *fp = fopen(SERIAL_NUMBER_FILE, "r");
   if (fp != NULL)
   {
      (void)memset(serialNumber, 0, sizeof(serialNumber)); // make sure NULL terminated
      int bytesRead = fread(serialNumber, 1, SERIAL_NUMBER_SIZE, fp);
      if (bytesRead < 0)
      {
         // set up default serial number
         (void)strncpy(serialNumber, DEFAULT_SERIAL_NUMBER, sizeof(serialNumber));
      }

      (void)fclose(fp);
   }

   fp = fopen(MODEL_NUMBER_FILE, "r");
   if (fp != NULL)
   {
      (void)memset(modelNumber, 0, sizeof(modelNumber));   // make sure NULL terminated
      int bytesRead = fread(modelNumber, 1, MODEL_NUMBER_SIZE, fp);
      if (bytesRead < 0)
      {
         // set up default model number
         (void)strncpy(modelNumber, DEFAULT_MODEL_NUMBER, sizeof(modelNumber));
      }

      (void)fclose(fp);
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int initializeDataStructures()                                                          */
/*                                                                                         */
/* Initialize all of the controller data structures.                                       */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initializeDataStructures()
{
   int ret = initFanDataStructures();
   ret |= initADCData();
   ret |= initRTDMappings();
   ret |= readTemperatureLookupFiles();
   ret |= initPGA117SPI();
   ret |= initHeaterDataStructures();
   ret |= getSerialAndModel();
   ret |= getSetpointLimits();
   ret |= system(COPY_LOGROTATE_FILE);
   ret |= system(COPY_SYSLOG_NG_FILE);
   lastTimeGUI1Heard = 0;
   lastTimeGUI2Heard = 0;
   gui1MissingOneShot = true;
   gui2MissingOneShot = true;
   bothGuisMissingOneShot = true;

   return ret;
}



/*******************************************************************************************/
/*                                                                                         */
/* int initSDCardSupport()                                                                 */
/*                                                                                         */
/* Check and initialize the microSD card support for verbose logging (30+ days)            */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int initSDCardSupport()
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   char readBuffer[DEFAULT_MESSAGE_BUFFER_SIZE];
   struct stat st;
   FILE *fp = NULL;

   (void)memset(&st, 0, sizeof(st));

   // make sure the directory to mount is present
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)snprintf(commandLine, sizeof(commandLine) - 1, SD_CARD_MKDIR_MOUNT_POINT, SD_CARD_MOUNT_POINT);
   (void)system(commandLine);

   // check the SD card format type
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)snprintf(commandLine, sizeof(commandLine) - 1, CHECK_SD_CARD_TYPE_COMMAND);
   (void)system(commandLine);

   fp = fopen(SD_CARD_TYPE_FILE, "r");
   if (fp != NULL)
   {
      (void)memset(readBuffer, 0, sizeof(readBuffer));
      (void)fread(readBuffer, 1, sizeof(readBuffer) - 1, fp);
      (void)fclose(fp);
      if (!strncmp(readBuffer, SD_CARD_TYPE_FAT32_STRING, strlen(SD_CARD_TYPE_FAT32_STRING)))
      {
         sdCardType = FAT32;
      }
      else if (!strncmp(readBuffer, SD_CARD_TYPE_EXFAT_STRING, strlen(SD_CARD_TYPE_EXFAT_STRING)))
      {
         sdCardType = EXFAT;
      }
      else
      {
         sdCardType = UNKNOWN;
      }
   }

   // create the mount point
   switch(sdCardType)
   {
      case FAT32:
         (void)system(UNMOUNT_SD_CARD_COMMAND);
         (void)memset(commandLine, 0, sizeof(commandLine));
         (void)snprintf(commandLine, sizeof(commandLine) - 1, SD_CARD_MOUNT_COMMAND, SD_CARD_DEVICE, SD_CARD_MOUNT_POINT);
         (void)system(commandLine);
         break;

      case EXFAT:
         (void)system(UNMOUNT_SD_CARD_COMMAND);
         (void)system(SD_CARD_EXFAT_MOUNT_COMMAND);
         break;

      default:
         ret = 1;
         sdCardExists = false;
         return ret;
         break;
   }

   // make sure it exists
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)unlink(BLKID_TXT_FILE);
   (void)snprintf(commandLine, sizeof(commandLine) - 1, CHECK_SD_CARD_VIA_BLKID, SD_CARD_DEVICE, BLKID_TXT_FILE);
   (void)system(commandLine);

   // check to see if the device exists
   // if the file is empty, /dev/mmcblk0p1 doesn't exist
   (void)stat(BLKID_TXT_FILE, &st);
   if (st.st_size > 0)
   {
      ret = 0;
      sdCardExists = true;
   }
   else
   {
      ret = 1;
      sdCardExists = false;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int checkSDCardExists()                                                                 */
/*                                                                                         */
/* Check if the microSD card is still there                                                */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int checkSDCardExists()
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   struct stat st;

   (void)memset(&st, 0, sizeof(st));

   // make sure it exists
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)unlink(BLKID_TXT_FILE);
   (void)snprintf(commandLine, sizeof(commandLine) - 1, CHECK_SD_CARD_VIA_BLKID, SD_CARD_DEVICE, BLKID_TXT_FILE);
   (void)system(commandLine);

   // check to see if the device exists
   // if the file is empty, /dev/mmcblk0p1 doesn't exist
   (void)stat(BLKID_TXT_FILE, &st);
   if (st.st_size > 0)
   {
      ret = 0;
      sdCardExists = true;
   }
   else
   {
      ret = 1;
      sdCardExists = false;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int getSDCardType()                                                                     */
/*                                                                                         */
/* Check the format of the microSD card. Supported types are FAT32(VFAT) and EXFAT         */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int getSDCardType()
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   struct stat st;

   (void)memset(&st, 0, sizeof(st));

   // make sure it exists
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)unlink(SD_CARD_TYPE_FILE);
   (void)snprintf(commandLine, sizeof(commandLine) - 1, CHECK_SD_CARD_TYPE_COMMAND);
   (void)system(commandLine);

   // check to see if the device exists
   // if the file is empty, /dev/mmcblk0p1 doesn't exist
   (void)stat(SD_CARD_TYPE_FILE, &st);
   if (st.st_size > 0)
   {
      ret = 0;
      sdCardExists = true;
   }
   else
   {
      ret = 1;
      sdCardExists = false;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* int checkEthernetUpDown()                                                               */
/*                                                                                         */
/* Check to see if the Ethernet connection is up or down                                   */
/*                                                                                         */
/* Returns: int ret - 0 = success, 1 = failure                                             */
/*                                                                                         */
/*******************************************************************************************/
int checkEthernetUpDown()
{
   int ret = 0;
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   struct stat st;

   (void)memset(&st, 0, sizeof(st));

   // check to see that the Ethernet is up
   (void)memset(commandLine, 0, sizeof(commandLine));
   (void)unlink(ETHERNET_UP_DOWN_FILE);
   (void)strncpy(commandLine, ETHERNET_UP_DOWN_COMMAND, sizeof(commandLine) - 1);
   (void)system(commandLine);

   // if the file exists and isn't empty
   // then the Ethernet is up
   (void)stat(ETHERNET_UP_DOWN_FILE, &st);
   if (st.st_size > 0)
   {
      ret = 0;
      ethernetUp = true;
   }
   else
   {
      ret = 1;
      ethernetUp = false;
   }

   return ret;
}


/*******************************************************************************************/
/*                                                                                         */
/* void createThreads()                                                                    */
/*                                                                                         */
/* Start all of the worker threads in the application. By design, all of the threads       */
/* run the entire time the application is running until the power is removed.              */
/*                                                                                         */
/* Returns: None                                                                           */
/*                                                                                         */
/*******************************************************************************************/
void createThreads()
{
    if (pthread_create( &statusPublisherThread_ID, NULL, statusPublisherThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the statusPublisherThread.\n");
      exit(-1);
    }

   if (pthread_create(&loggerThread_ID, NULL, loggerThread, NULL))
   {
      (void)printf("Fail...Cannot spaw the loggerThread.\n");
      exit(-1);
   }

    if (pthread_create( &commandHandlerThread_ID, NULL, commandHandlerThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the commandHandlerThread.\n");
      exit(-1);
    }

    if (pthread_create( &commandHandlerThread2_ID, NULL, commandHandlerThread2, NULL))
    {
      (void)printf("Fail...Cannot spawn the commandHandlerThread2.\n");
      exit(-1);
    }

    if (pthread_create( &readADCThread_ID, NULL, readADCThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the readADCThread.\n");
      exit(-1);
    }

    if (pthread_create( &heaterControlThread_ID, NULL, heaterControlThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the heaterControlThread.\n");
      exit(-1);
    }

    if (pthread_create( &heartBeatListenerThread_ID, NULL, heartBeatListenerThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the heartBeatListenerThread.\n");
      exit(-1);
    }

    if (pthread_create( &heartBeatListenerThread2_ID, NULL, heartBeatListenerThread2, NULL))
    {
      (void)printf("Fail...Cannot spawn the heartBeatListenerThread2.\n");
      exit(-1);
    }

    if (pthread_create( &timeSyncSubscriberThread_ID, NULL, timeSyncSubscriberThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the timeSyncSubscriberThread.\n");
      exit(-1);
    }
#if(0)
    if (pthread_create( &timeSyncSubscriberThread2_ID, NULL, timeSyncSubscriberThread2, NULL))
    {
      (void)printf("Fail...Cannot spawn the timeSyncSubscriberThread2.\n");
      exit(-1);
    }
#endif
    if (pthread_create( &firmwareUpdateListenerThread_ID, NULL, firmwareUpdateListenerThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the firmwareUpdateListenerThread.\n");
      exit(-1);
    }

    if (pthread_create( &firmwareUpdatePackageThread_ID, NULL, firmwareUpdatePackageThread, NULL))
    {
      (void)printf("Fail...Cannot spawn the firmwareUpdatePackageThread.\n");
      exit(-1);
    }

   if (pthread_create(&softPowerdownThread_ID, NULL, softPowerdownThread, NULL))
   {
      (void)printf("Fail...Cannot spawn the softPowerdownThread.\n");
      exit(-1);
   }

   if (pthread_create(&rtdPublisherThread_ID, NULL, rtdPublisherThread, NULL))
   {
      (void)printf("Fail...Cannot spawn the rtdPublisherThread.\n");
      exit(-1);
   }
}


/*******************************************************************************************/
/*                                                                                         */
/* int main(int argc, char *argv[])                                                        */
/*                                                                                         */
/* Call the initialization functions and start all of the task threads.                    */
/* Never exits.                                                                            */
/*                                                                                         */
/*******************************************************************************************/
int main(int argc, char *argv[])
{
   bool previousSDCardState = false;
   struct timeval start;
   struct timeval end;
   uint32_t executionTime;
   uint32_t balance;

   // this is really only necessary for testing in order to stop the program
   struct sigaction sig_act;
   (void)memset(&sig_act, 0x00, sizeof(sig_act));
   sig_act.sa_handler = handle_signals;
   sigaction(SIGINT, &sig_act, NULL);
   sigaction(SIGTERM, &sig_act, NULL);
   sigaction(SIGUSR1, &sig_act, NULL);
   sigaction(SIGSEGV, &sig_act, NULL);
   sigaction(SIGABRT, &sig_act, NULL);

   // open syslog
   openlog(SYSLOG_IDENT_STRING, LOG_NDELAY, LOG_USER);
   syslog(LOG_NOTICE, "Frontier-UHC Firmware Version %s", FRONTIER_UHC_FIRMWARE_VERSION);
   printf("Frontier-UHC Firmware Version %s\n", FRONTIER_UHC_FIRMWARE_VERSION);
   char commandLine[COMMAND_LINE_BUFFER_SIZE];
   (void)memset(commandLine, 0, sizeof(COMMAND_LINE_BUFFER_SIZE));
   (void)snprintf(commandLine, sizeof(commandLine), "/bin/echo %s > %s", FRONTIER_UHC_FIRMWARE_VERSION, FIRMWARE_VERSION_FILE);
   (void)system(commandLine);
   syslog(LOG_NOTICE, "Waiting for SYSTEM_COMMAND_STARTUP");

   if (!logDirectoryExists(HENNYPENNY_LOG_DIRECTORY))
   {
      syslog(LOG_WARNING, "Log file directory %s doesn't exist", HENNYPENNY_LOG_DIRECTORY);
   }

   if (initSDCardSupport())
   {
      syslog(LOG_ERR, "MicroSD card is missing - verbose logging disabled");
      (void)strncpy(errorCodeString, SD_CARD_MISSING_OR_CORRUPT, sizeof(errorCodeString) - 1);
      alarmCode = ALARM_CODE_SD_CARD_MISSING;
   }
   else
   {
      // SD_CARD_LOG_DIRECTORY is on the SD card.
      if (!logDirectoryExists(SD_CARD_LOG_TOP))
      {
         syslog(LOG_WARNING, "Log file directory %s doesn't exist", SD_CARD_LOG_TOP);
         (void)strncpy(errorCodeString, SD_CARD_MISSING_OR_CORRUPT, sizeof(errorCodeString) - 1);
         alarmCode = ALARM_CODE_SD_CARD_MISSING;
      }
      else
      {
         if (!logDirectoryExists(SD_CARD_LOG_DIRECTORY))
         {
            syslog(LOG_WARNING, "Log file directory %s doesn't exist", SD_CARD_LOG_DIRECTORY);
            (void)strncpy(errorCodeString, SD_CARD_MISSING_OR_CORRUPT, sizeof(errorCodeString) - 1);
            alarmCode = ALARM_CODE_SD_CARD_MISSING;
         }
      }
   }

   previousSDCardState = sdCardExists;

   // force a logrotate since they seem to not leave the UHC
   // powered up 24/7
   (void)system(FORCE_LOGROTATE_COMMAND);

   (void)memset(controllerIPAddress, 0, sizeof(controllerIPAddress));
   (void)memset(guiIPAddress1, 0, sizeof(guiIPAddress1));
   (void)memset(guiIPAddress2, 0, sizeof(guiIPAddress2));
   (void)memset(controllerManifestFilename, 0, sizeof(controllerManifestFilename));
   (void)strncpy(controllerManifestFilename, CONTROLLER_MANIFEST_FILENAME, sizeof(controllerManifestFilename));

   int i;
   if (4 == argc)
   {
      for (i = 1; i < argc; i++)
      {
         switch (i)
         {
            case 1:
               (void)strncpy(controllerIPAddress, argv[i], sizeof(controllerIPAddress));
               (void)printf("controllerIPAddress = %s\n", controllerIPAddress);
               break;

            case 2:
               (void)strncpy(guiIPAddress1, argv[i], sizeof(guiIPAddress1));
               (void)printf("guiIPAddress1 = %s\n", guiIPAddress1);
               break;

             case 3:
               (void)strncpy(guiIPAddress2, argv[i], sizeof(guiIPAddress2));
               (void)printf("guiIPAddress2 = %s\n", guiIPAddress2);
               break;

            default:
               break;
         }
      }
   }
   else
   {
      (void)printf("Usage frontier_uhc <controllerIPAddress> <GUI1IPAddress> <GUI2IPAddress>\n");
      exit(-1);
   }

   int ret = initializeHardware();
   if (0 == ret)
   {
      (void)printf("Board hardware initialization successful\n");
   }
   else
   {
      (void)printf("Board hardware initialization failed ret = %d\n", ret);
   }

   // this is a one shot done at powerup
   // Obviously, the board revision isn't going to be changed while operating
   // this needs to be done before the data strutures are initialized
   // since the RTD to temp lookup tables are dependent on board revision (switched from 3.3v bias to 1.8v bias)
   getBoardRev();
   (void)printf("Controller board hardware revision = %d\n", controllerBoardRevision);

   ret = initializeDataStructures();
   if (0 == ret)
   {
      (void)printf("Board data structure initialization successful\n");
   }
   else
   {
      (void)printf("Board data structure initialization failed ret = %d\n", ret);
   }

   // debug info for initial state of the heaters
   if (debugHeatersPrintf)
   {
      printHeaters();
   }

   float volts = 0.0;
   (void)atm90e26_vrms_get(&volts);
   int maxHeatersOn = 8;
   int startingIndex = 4;

   if (volts != 0.0)
   {
      if (volts <= 201.0)
      {
         maxHeatersOn = 10;
         startingIndex = 2;
      }
      else if ((volts > 201.0) && (volts <= 221.0))
      {
         maxHeatersOn = 9;
         startingIndex = 3;
      }
      else
      {
         maxHeatersOn = 8;
         startingIndex = 4;
      }
   }
   printf("AC Line voltage = %3.2f maxHeatersOn = %d startingIndex = %d\n", volts, maxHeatersOn, startingIndex);

   // run self tests
   if (0 == runSelfTests())
   {
      selfTestsOK = true;
   }
   else
   {
      selfTestsOK = false;
   }

   ret = initializeZeroMQ();
   if (0 == ret)
   {
      (void)printf("ZeroMQ initialization successful\n");
   }
   else
   {
      (void)printf("ZeroMQ initialization failed ret = %d\n", ret);
   }

   // start task threads
   createThreads();

   // this loop just spins forever until the power is cut.
   // the task threads do all of the heavy lifting.
   while (!sigTermReceived)
   {
      (void)gettimeofday(&start, NULL);

      // these file existence checks allow debug messages on the console to be turned on/off
      // while running; to start debug messages flowing, issue "touch /tmp/debug" for most messages,
      // and "touch /tmp/debugHeater" to debug the heater control
      // to stop messages issue "rm /tmp/debug" or "rm /tmp/debugHeaters" from the command line
      // you can have either/or, or both debugs turned on/off independently of each other
      // check if the console debug printf is enabled
      if (access(DEBUG_PRINTF_FILE, F_OK) == 0)
      {
         debugPrintf = true;
      }
      else
      {
         debugPrintf = false;
      }

      // check if the console debug Heaters printf is enabled
      if (access(DEBUG_HEATERS_PRINTF_FILE, F_OK) == 0)
      {
         debugHeatersPrintf = true;
      }
      else
      {
         debugHeatersPrintf = false;
      }

      // check for debugging CSS message enabled
      if (access(DEBUG_CSS_MESSAGE_FILE, F_OK) == 0)
      {
         debugCSS = true;
      }
      else
      {
         debugCSS = false;
      }

      // check for writing CSV debugging enabled
      if (access(DEBUG_CSV_FILE, F_OK) == 0)
      {
         debugCSV = true;
      }
      else
      {
         debugCSV = false;
      }

      // check if an SD card was added or removed
      // sets the sdCardExists flag
      checkSDCardExists();
      if (previousSDCardState != sdCardExists)
      {
         if (sdCardExists)
         {
            initSDCardSupport();
            syslog(LOG_INFO, "MicroSD card inserted");
         }
         else
         {
            alarmCode = ALARM_CODE_SD_CARD_MISSING;
            (void)strncpy(errorCodeString, SD_CARD_MISSING_OR_CORRUPT, sizeof(errorCodeString) - 1);
            syslog(LOG_ERR, "MicroSD card removed - verbose logging disabled");
         }
      }

      previousSDCardState = sdCardExists;

	   ret = checkEthernetUpDown();
	   if ((ret == 0) && ethernetUp)
	   {
          //printf("Ethernet UP\n");
         ethernetDownTimeSeconds = 0;
         if (!ethernetErrorOneShot)
         {
            // We logged E-220A to our log files, but never sent it to the backend because we had no
            // Ethernet connection. Now the connection is back, send that E-220A up. But don't report the
            // system status as error, and don't add it to the log files again.
            (void)strncpy(errorCodeString, ETHERNET_DOWN_ERROR_CODE, sizeof(errorCodeString) - 1);

            ethernetErrorOneShot = true;
         }
	  }
	  else
	  {
		  ethernetDownTimeSeconds++;
          //printf("Ethernet DOWN %d seconds\n", ethernetDownTimeSeconds);
	  }
	  previousEthernetUp = ethernetUp;

     (void)gettimeofday(&end, NULL);

     executionTime = (((end.tv_sec - start.tv_sec) * ONE_SECOND_IN_MICROSECONDS) + (end.tv_usec - start.tv_usec));
     if (executionTime < ONE_SECOND_IN_MICROSECONDS)
     {
        balance = ONE_SECOND_IN_MICROSECONDS - executionTime;
//        printf("task execution took %d microseconds balance = %ld\n", executionTime, balance);
        usleep(balance);
     }
     else
     {
        // force a minimum thread sleep time
//        printf("Execution time took more than 1 second - %d.%d\n", (executionTime / ONE_SECOND_IN_MICROSECONDS), (executionTime % ONE_SECOND_IN_MICROSECONDS));
        usleep(MINIMUM_THREAD_SLEEP_TIME);
     }
   }

   // wait up to 3 seconds to terminate program once we receive SIGTERM
   int seconds = 3;
   while (numThreadsRunning > 0)
   {
      sleep(1);
	  seconds--;
	  if (0 == seconds)
	  {
	     break;
	  }
   }
   // turn all heaters off
   for (int j = 0; j < NUM_HEATERS; j++)
   {
      (void)enableDisableHeater(j, HEATER_DISABLED);
      (void)turnHeaterOnOff(j, HEATER_STATE_OFF);
   }
   // turn fans off
   turnFansOnOff(FAN_STATE_OFF);

   printf("SIGTERM received - program exiting\n");
   syslog(LOG_NOTICE, "Left main loop - Exiting program");
   closeProgram();

   exit(0);
}
