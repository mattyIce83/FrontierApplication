/******************************************************************************/
/*                                                                            */
/* FILE:        ZMQSniffer.cpp                                                */
/*                                                                            */
/* DESCRIPTION: Routines to implement a ZeroMQ sniffer to display all ZMQ     */
/*              messages in the HennyPenny Frontier UHC for McDonalds         */
/*                                                                            */
/* AUTHOR(S):   USA Firmware, LLC                                             */
/*                                                                            */
/* DATE:        May 18, 2023                                                  */
/*                                                                            */
/* This is an unpublished work subject to Trade Secret and Copyright          */
/* protection by HennyPenny and USA Firmware, LLC                             */
/*                                                                            */
/* USA Firmware, LLC                                                          */
/* 10060 Brecksville Road Brecksville, OH 44141                               */
/*                                                                            */
/* EDIT HISTORY:                                                              */
/* May 18, 2023 - Initial release                                             */
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
#include <ctime>
#include <google/protobuf/util/time_util.h>
#include <zmq.h>
#include <dirent.h>
#include <sys/types.h>

#include "frontier_uhc.h"
#include "uhc.pb.h"
#include "uhc_proto.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

// global variables
void *context;
void *statusPublisher;
void *subscriber;
void *timeSyncSubscriber;
void *commandListener;
void *commandListener2;
void *commandResponseListener;
void *commandResponseListener2;
void *heartBeatListener;
void *heartBeatListener2;
void *firmwareResponseListener;
void *firmwareUpdateListener;

char controllerIPAddress[IP_STRING_SIZE];
char guiIPAddress1[IP_STRING_SIZE];
char guiIPAddress2[IP_STRING_SIZE];
char outputFilename[MAX_FILE_PATH];
char timeString[128];

// thread IDs
pthread_t statusListenerThread_ID;
pthread_t subscriberThread_ID;
pthread_t heartBeatListenerThread_ID;
pthread_t heartBeatListenerThread2_ID;
pthread_t timeSyncSubscriberThread_ID;
pthread_t firmwareUpdateListenerThread_ID;
pthread_t commandHandlerThread_ID;
pthread_t commandHandlerThread2_ID;
pthread_t commandResponseListenerThread_ID;
pthread_t commandResponseListenerThread2_ID;
pthread_t firmwareResponseListenerThread_ID;

bool debugPrintf = false;
bool verboseMode = false;

FILE *snifferOutFile = NULL;

/*******************************************************************************************/
/*                                                                                         */
/* static void handle_signals(int signum)                                                  */
/*                                                                                         */
/* SIGINT & SIGTERM handler: set doQuit to true for graceful termination                   */
/*                                                                                         */
/*******************************************************************************************/
static void handle_signals(int signum)
{
    int res;
    switch (signum)
    {
        case SIGUSR1:
        case SIGSEGV:
        case SIGABRT:
		case SIGINT:
            {
               switch (signum)
               {
                   case SIGSEGV:
                   case SIGABRT:
				   case SIGINT:
                       zmq_close (statusPublisher);
                       zmq_close (subscriber);
					   zmq_close (timeSyncSubscriber);
                       zmq_close (heartBeatListener);
                       zmq_close (heartBeatListener2);
                       zmq_close (commandListener);
                       zmq_close (commandListener2);
					   zmq_close (commandResponseListener);
					   zmq_close (commandResponseListener2);
                       zmq_close (firmwareUpdateListener);
                       zmq_close (firmwareResponseListener);
                       zmq_ctx_destroy (context);
					   if (snifferOutFile != NULL)
					   {
							fclose(snifferOutFile);
					   }
                       exit(0);

                   default:
                       break;
               }
            }
            break;

        default:
            break;
    }
}   // handle_signals()

// ****************** Utilitity functions ******************
// Print the current date and time
char *getTimeStampString()
{
 	memset(timeString, 0, sizeof(timeString));
	timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;

    char buffer [80];
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));

    sprintf(timeString, "%s:%03d", buffer, milli);
    return timeString;
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
	strcpy(outPtr, "\"");
	outPtr++;
	for (i = 0; i < length; i++, inPtr++, outPtr++)
	{
		if (*inPtr == ' ')
		{
			strcat(outPtr, "\\");
			outPtr += 1;
		}
		*outPtr = *inPtr;
	}
	strcat(outPtr, "/*.deb\"");
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
	char messageType[] = { 10, 4, 'F', 'W', 'U', 'P' };
	char ig1CommandReqSubPort[IP_STRING_SIZE];
	char escapedFullFilePath[MAX_FILE_PATH];
	int response = 0;
    char message[FIRMWARE_UPDATE_BUFFER_SIZE];

	sprintf(ig1CommandReqSubPort, "tcp://%s:%d", guiIPAddress1, FIRMWARE_UPDATE_PORT_GUI1);
    firmwareUpdateListener = zmq_socket(context, ZMQ_SUB);

    int rc = zmq_connect(firmwareUpdateListener, ig1CommandReqSubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(firmwareUpdateListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		memset(escapedFullFilePath, 0, sizeof(escapedFullFilePath));
		
        rc = zmq_recv(firmwareUpdateListener, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		FirmwareUpdate deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "firmwareUpdateListenerThread ERROR: Unable to deserialize!\n";
			}
//			syslog(LOG_ERR, "firmwareUpdateListenerThread: Unable to deserialize!");
//			pthread_exit(NULL);
		}
		printf("%s FirmwareUpdate received from %s user = %s pass = %s filePath = %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.username().c_str(), deserialized.password().c_str(), deserialized.file_path().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s FirmwareUpdate received from %s user = %s pass = %s filePath = %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.username().c_str(), deserialized.password().c_str(), deserialized.file_path().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);

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

		
    }
	pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *firmwareUpdateResponseListenerThread(void *)                                      */
/*                                                                                         */
/* Listen for a FirmwareResponse message.                                                  */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *firmwareResponseListenerThread(void *)
{
	char responseMessageType[] = { 10, 4, 'F', 'W', 'R', 'S' };
	char ig1CommandRespPubPort[IP_STRING_SIZE];
	char ig1CommandReqSubPort[IP_STRING_SIZE];
	char escapedFullFilePath[MAX_FILE_PATH];
	int response = 0;
	uint32_t sequenceNumber = 0;
    char message[FIRMWARE_UPDATE_BUFFER_SIZE];

	sprintf(ig1CommandRespPubPort, "tcp://%s:%d", controllerIPAddress, FIRMWARE_UPDATE_RESULT_PORT_CONTROLLER);

    firmwareResponseListener = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(firmwareResponseListener, ig1CommandRespPubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(firmwareResponseListener, ZMQ_SUBSCRIBE, responseMessageType, sizeof(responseMessageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		
        rc = zmq_recv(firmwareResponseListener, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		FirmwareUpdateResult deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "firmwareResponseListenerThread ERROR: Unable to deserialize!\n";
			}
//			syslog(LOG_ERR, "firmwareResponseListenerThread: Unable to deserialize!");
//			pthread_exit(NULL);
		}
		printf("%s FirmwareResponse received from %s response = %s seqNumber = %d\n", getTimeStampString(), deserialized.controller_ip_address().c_str(), deserialized.result_text().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s FirmwareResponse received from %s response = %s seqNumber = %d\n", getTimeStampString(), deserialized.controller_ip_address().c_str(), deserialized.result_text().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
			deserialized.PrintDebugString();

			cout << "        sender IP address: " << deserialized.controller_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
			cout << "              result text: " << deserialized.result_text() << "\n";
		}

		
    }
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
	char messageType[] = { 10, 2, 'H', 'B' };
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	char heartbeatPort[IP_STRING_SIZE];

    heartBeatListener = zmq_socket(context, ZMQ_SUB);
	sprintf(heartbeatPort, "tcp://%s:%d", guiIPAddress1, HEARTBEAT_PORT_GUI1);

    int rc = zmq_connect(heartBeatListener, heartbeatPort);
    assert(rc == 0);

    rc = zmq_setsockopt(heartBeatListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);


    while(1)
    {
		memset(message, 0, sizeof(message));
        rc = zmq_recv(heartBeatListener, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		HeartBeat deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "heartBeatListenerThread ERROR: Unable to deserialize!");
			}
		}
		printf("%s HEARTBEAT received from %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s HEARTBEAT received from %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
			deserialized.PrintDebugString();

//			cout << "           system up time: " << TimeUtil::ToString(deserialized.system_data().system_up_time()) << "\n";
			cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
		}
    }
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
	char messageType[] = { 10, 2, 'H', 'B' };
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	char heartbeatPort[IP_STRING_SIZE];

    heartBeatListener2 = zmq_socket(context, ZMQ_SUB);
	sprintf(heartbeatPort, "tcp://%s:%d", guiIPAddress2, HEARTBEAT_PORT_GUI2);

    int rc = zmq_connect(heartBeatListener2, heartbeatPort);
    assert(rc == 0);

    rc = zmq_setsockopt(heartBeatListener2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);


    while(1)
    {
		memset(message, 0, sizeof(message));
        rc = zmq_recv(heartBeatListener2, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		HeartBeat deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "heartBeatListenerThread2 ERROR: Unable to deserialize!");
			}
		}
		printf("%s HEARTBEAT received from %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s HEARTBEAT received from %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
			deserialized.PrintDebugString();

//			cout << "           system up time: " << TimeUtil::ToString(deserialized.system_data().system_up_time()) << "\n";
			cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
		}
    }
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
	char messageType[] = { 10, 4, 'T', 'I', 'M', 'E' };
	char guiIPandPort[IP_STRING_SIZE];

	memset(guiIPandPort, 0, sizeof(guiIPandPort));
	sprintf(guiIPandPort, "tcp://%s:%d", guiIPAddress1, TIME_SYNC_PORT_GUI1);

    timeSyncSubscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(timeSyncSubscriber, guiIPandPort);
    assert(rc == 0);

    rc = zmq_setsockopt(timeSyncSubscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    char message[DEFAULT_MESSAGE_BUFFER_SIZE];

    while(1)
    {
		memset(message, 0, sizeof(message));
        rc = zmq_recv(timeSyncSubscriber, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		TimeSync deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "timeSyncSubscriberThread ERROR: Unable to deserialize!");
			}
		}

		if (debugPrintf)
		{
			cout << "Deserialization:\n\n";
			deserialized.PrintDebugString();

			cout << "deserialized current time: " << TimeUtil::ToString(deserialized.current_time()) << "\n";
		}

        string dateString = TimeUtil::ToString(deserialized.current_time());
		string timeZone = deserialized.time_zone();
		printf("%s TimeSync received from %s new Time = %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), dateString.c_str(), deserialized.sequence_number());
		printf("%s  time zone = %s\n", getTimeStampString(), timeZone.c_str());
fprintf(snifferOutFile, "%s TimeSync received from %s new Time = %s seqNumber = %d\n", getTimeStampString(), deserialized.sender_ip_address().c_str(), dateString.c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s  time zone = %s\n", getTimeStampString(), timeZone.c_str());
fflush(snifferOutFile);
    }
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
	uint32_t uint32Value = 0;
	int i = 0;
	uint16_t temperature = 0;
	uint16_t upperTemp = 0;
	uint16_t lowerTemp = 0;
	google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
	timestamp->set_seconds(0);
	timestamp->set_nanos(0);
	
	switch (systemCommand.command())
	{
		case SYSTEM_COMMAND_HEATER_ON:
			// turning on a heater really only enables it for the heater algorithm
			// which has to abide by the current draw limits
			printf("%s SYSTEM_COMMAND_HEATER_ON received from %s slotNumber = %d heaterIndex = %d  seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.heater_index(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_HEATER_ON received from %s slotNumber = %d heaterIndex = %d  seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.heater_index(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_HEATER_OFF:
			// turning off a heater really disables both heaters for a slot
			printf("%s SYSTEM_COMMAND_HEATER_OFF received from %s slotNumber = %d heaterIndex = %d  seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.heater_index(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_HEATER_OFF received from %s slotNumber = %d heaterIndex = %d  seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.heater_index(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_SHUTDOWN_REQUESTED:
			// turn all heaters off
			printf("%s SYSTEM_COMMAND_SHUTDOWN_REQUESTED received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_SHUTDOWN_REQUESTED received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_EMERGENCY_STOP:
			// turn all heaters off
			printf("%s SYSTEM_COMMAND_EMERGENCY_STOP received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_EMERGENCY_STOP received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_STARTUP:
			// pre-heat all of the slots to the desired setpoint as quickly as possible
			// has to be within 25 minutes +/- 5 minutes.
			// also, enable all of the heaters
			printf("%s SYSTEM_COMMAND_STARTUP received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_STARTUP received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_IDLE:
			printf("%s SYSTEM_COMMAND_IDLE received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_IDLE received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT:
			// you can only set the temperature for a slot, but there are two heaters
			// so, set the setpoint for both heaters of a slot
			printf("%s SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT received from %s slot number = %d setpoint = %d  seqNumber =  %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.temperature(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT received from %s slot number = %d setpoint = %d  seqNumber =  %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), systemCommand.temperature(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;
			
		case SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT:
			upperTemp = systemCommand.heater_location_upper_setpoint_temperature();
			lowerTemp = systemCommand.heater_location_lower_setpoint_temperature();
			printf("%s SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT received from %s  slot Number = %d upper setpoint = %d  lower setpoint = %d seqNumber =  %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), upperTemp, lowerTemp, systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT received from %s  slot Number = %d upper setpoint = %d  lower setpoint = %d seqNumber =  %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.slot_number(), upperTemp, lowerTemp, systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_SET_DURATION:
			printf("%s SYSTEM_COMMAND_SET_DURATION received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_SET_DURATION received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_SET_ECO_MODE_TIME:
			printf("%s SYSTEM_COMMAND_SET_ECO_MODE_TIME received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_SET_ECO_MODE_TIME received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_SET_ECO_MODE_TEMP:
			printf("%s SYSTEM_COMMAND_SET_ECO_MODE_TEMP temperature = %d received from %s\n", getTimeStampString(), systemCommand.temperature(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_SET_ECO_MODE_TEMP temperature = %d received from %s\n", getTimeStampString(), systemCommand.temperature(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_FAN_ON:
			printf("%s SYSTEM_COMMAND_FAN_ON for %s received from %s seqNumber = %d\n", getTimeStampString(), FanNumber_Name(systemCommand.fan_number()).c_str(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_FAN_ON for %s received from %s seqNumber = %d\n", getTimeStampString(), FanNumber_Name(systemCommand.fan_number()).c_str(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_FAN_OFF:
			printf("%s SYSTEM_COMMAND_FAN_OFF for %s received from %s seqNumber = %d\n", getTimeStampString(), FanNumber_Name(systemCommand.fan_number()).c_str(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_FAN_OFF for %s received from %s seqNumber = %d\n", getTimeStampString(), FanNumber_Name(systemCommand.fan_number()).c_str(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_CLEANING_MODE_ON:
			printf("%s SYSTEM_COMMAND_CLEANING_MODE_ON received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_CLEANING_MODE_ON received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_CLEANING_MODE_OFF:
			printf("%s SYSTEM_COMMAND_CLEANING_MODE_OFF received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_CLEANING_MODE_OFF received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;
			
		case SYSTEM_COMMAND_ESTABLISH_LINK:
			printf("%s SYSTEM_COMMAND_ESTABLISH_LINK received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_ESTABLISH_LINK received from %s seqNumber = %d\n", getTimeStampString(), systemCommand.sender_ip_address().c_str(), systemCommand.sequence_number());
fflush(snifferOutFile);
			break;

		case SYSTEM_COMMAND_UNKNOWN:
			printf("%s SYSTEM_COMMAND_UNKNOWN received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fprintf(snifferOutFile, "%s SYSTEM_COMMAND_UNKNOWN received from %s\n", getTimeStampString(), systemCommand.sender_ip_address().c_str());
fflush(snifferOutFile);
			break;
	}
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
	char messageType[] = { 10, 3, 'C', 'M', 'D' };
	char igCommandReqSubPort[IP_STRING_SIZE];
	int response = 0;
	uint32_t sequenceNumber = 0;
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	uhc::SystemCommandResponses ret = SYSTEM_COMMAND_RESPONSE_OK;
	char commandRespPubPort[IP_STRING_SIZE];

	sprintf(igCommandReqSubPort, "tcp://%s:%d", guiIPAddress1, SYSTEM_COMMAND_PORT_GUI1);
printf("commandHandlerThread igCommandReqSubPort = %s\n", igCommandReqSubPort);
fprintf(snifferOutFile, "commandHandlerThread igCommandReqSubPort = %s\n", igCommandReqSubPort);
fflush(snifferOutFile);

    commandListener = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(commandListener, igCommandReqSubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(commandListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		ret = SYSTEM_COMMAND_RESPONSE_OK;
		
        rc = zmq_recv(commandListener, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SystemCommand deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "commandHandlerThread ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to deserialize!");
			}
		}

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
//			deserialized.PrintDebugString();

			cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
			cout << "                  command: " << deserialized.command() << "\n";
//			cout << "              slot number: " << deserialized.slot_number() << "\n";
//			cout << "              temperature: " << deserialized.temperature() << "\n";
		}

		// Handle command here
		ret = processCommand(deserialized);
		
    }
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
	char messageType[] = { 10, 3, 'C', 'M', 'D' };
	char igCommandReqSubPort[IP_STRING_SIZE];
	int response = 0;
	uint32_t sequenceNumber = 0;
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	uhc::SystemCommandResponses ret = SYSTEM_COMMAND_RESPONSE_OK;
	char commandRespPubPort[IP_STRING_SIZE];

	sprintf(igCommandReqSubPort, "tcp://%s:%d", guiIPAddress2, SYSTEM_COMMAND_PORT_GUI2);
printf("commandHandlerThread2 igCommandReqSubPort = %s\n", igCommandReqSubPort);
fprintf(snifferOutFile, "commandHandlerThread2 igCommandReqSubPort = %s\n", igCommandReqSubPort);
fflush(snifferOutFile);

    commandListener2 = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(commandListener2, igCommandReqSubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(commandListener2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		ret = SYSTEM_COMMAND_RESPONSE_OK;
		
        rc = zmq_recv(commandListener2, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SystemCommand deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "commandHandlerThread ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to deserialize!");
			}
		}

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
//			deserialized.PrintDebugString();

			cout << "        sender IP address: " << deserialized.sender_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
			cout << "                  command: " << deserialized.command() << "\n";
//			cout << "              slot number: " << deserialized.slot_number() << "\n";
//			cout << "              temperature: " << deserialized.temperature() << "\n";
		}

		// Handle command here
		ret = processCommand(deserialized);
		
    }
	pthread_exit(NULL);
}

/*******************************************************************************************/
/*                                                                                         */
/* void *commandResponseListenerThread(void *)                                             */
/*                                                                                         */
/* Receive the SystemCommandResponse message fron the controller                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *commandResponseListenerThread(void *)
{
	char messageType[] = { 10, 3, 'R', 'S', 'P' };
	char igCommandReqSubPort[IP_STRING_SIZE];
	int response = 0;
	uint32_t sequenceNumber = 0;
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	uhc::SystemCommandResponses ret = SYSTEM_COMMAND_RESPONSE_OK;
	char commandRespPubPort[IP_STRING_SIZE];

	sprintf(igCommandReqSubPort, "tcp://%s:%d", controllerIPAddress, SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER);
printf("commandResponseListenerThread igCommandReqSubPort = %s\n", igCommandReqSubPort);
fprintf(snifferOutFile, "commandResponseListenerThread igCommandReqSubPort = %s\n", igCommandReqSubPort);
fflush(snifferOutFile);

    commandResponseListener = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(commandResponseListener, igCommandReqSubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(commandResponseListener, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		ret = SYSTEM_COMMAND_RESPONSE_OK;
		
        rc = zmq_recv(commandResponseListener, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SystemCommandResponse deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "commandHandlerThread ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to deserialize!");
			}
		}
		printf("%s %s received from %s seqNumber = %d\n", getTimeStampString(), SystemCommandResponses_Name(deserialized.response()).c_str(), deserialized.requester_ip_address().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s %s received from %s seqNumber = %d\n", getTimeStampString(), SystemCommandResponses_Name(deserialized.response()).c_str(), deserialized.requester_ip_address().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);

		if (debugPrintf)
		{
			cout << "Deserialization:\n";
//			deserialized.PrintDebugString();

			cout << "        sender IP address: " << deserialized.requester_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
			cout << "                  command: " << deserialized.command() << "\n";
			cout << "                 response: " << deserialized.response() << "\n";
//			cout << "              slot number: " << deserialized.slot_number() << "\n";
//			cout << "              temperature: " << deserialized.temperature() << "\n";
		}

    }
	pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *commandResponseListenerThread2(void *)                                            */
/*                                                                                         */
/* Receive the SystemCommandResponse message fron the controller                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *commandResponseListenerThread2(void *)
{
	char messageType[] = { 10, 3, 'R', 'S', 'P' };
	char igCommandReqSubPort[IP_STRING_SIZE];
	int response = 0;
	uint32_t sequenceNumber = 0;
    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	uhc::SystemCommandResponses ret = SYSTEM_COMMAND_RESPONSE_OK;
	char commandRespPubPort[IP_STRING_SIZE];

	sprintf(igCommandReqSubPort, "tcp://%s:%d", controllerIPAddress, SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER2);
printf("commandResponseListenerThread2 igCommandReqSubPort = %s\n", igCommandReqSubPort);
fprintf(snifferOutFile, "commandResponseListenerThread2 igCommandReqSubPort = %s\n", igCommandReqSubPort);
fflush(snifferOutFile);

    commandResponseListener2 = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(commandResponseListener2, igCommandReqSubPort);
    assert(rc == 0);

    rc = zmq_setsockopt(commandResponseListener2, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    while(1)
    {
		memset(message, 0, sizeof(message));
		ret = SYSTEM_COMMAND_RESPONSE_OK;
		
        rc = zmq_recv(commandResponseListener2, message, sizeof(message), 0);
        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SystemCommandResponse deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			if (debugPrintf)
			{
				cerr << "commandHandlerThread ERROR: Unable to deserialize!\n";
//				syslog(LOG_ERR, "commandHandlerThread ERROR: Unable to deserialize!");
			}
		}
		printf("%s %s received from %s seqNumber = %d\n", getTimeStampString(), SystemCommandResponses_Name(deserialized.response()).c_str(), deserialized.requester_ip_address().c_str(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s %s received from %s seqNumber = %d\n", getTimeStampString(), SystemCommandResponses_Name(deserialized.response()).c_str(), deserialized.requester_ip_address().c_str(), deserialized.sequence_number());
fflush(snifferOutFile);
		if (debugPrintf)
		{
			cout << "Deserialization:\n";
//			deserialized.PrintDebugString();

			cout << "        sender IP address: " << deserialized.requester_ip_address() << "\n";
			cout << "          sequence number: " << deserialized.sequence_number() << "\n";
			cout << "                  command: " << deserialized.command() << "\n";
			cout << "                 response: " << deserialized.response() << "\n";
//			cout << "              slot number: " << deserialized.slot_number() << "\n";
//			cout << "              temperature: " << deserialized.temperature() << "\n";
		}

    }
	pthread_exit(NULL);
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


void *statusListenerThread(void *)
{
	char messageType[] = { 10, 3, 'C', 'S', 'S' };

	char controllerIP[IP_STRING_SIZE];

	memset(controllerIP, 0, sizeof(controllerIP));
	sprintf(controllerIP, "tcp://%s:%d", controllerIPAddress, CURRENT_SYSTEM_STATE_PORT_CONTROLLER);

    subscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(subscriber, controllerIP);
    assert(rc == 0);

    rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	memset(message, 0, sizeof(message));


    while(1)
    {
        rc = zmq_recv(subscriber, message, sizeof(message), 0);
//        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		CurrentSystemState deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			cerr << "ERROR: Unable to deserialize!\n";
		}
printf("%s CurrentSystemStatus received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s CurrentSystemStatus received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fflush(snifferOutFile);

		for (int i = 0; i < deserialized.slot_data_size(); i++)
		{
			SlotData* thisSlot = deserialized.mutable_slot_data(i);
			printf("\tSlot %d Upper SP:%d Temp:%d  Open:%d Shor:%d OT:%d UT%d Ena:%d Lower SP:%d Temp:%d Open:%d Short:%d OT:%d UT%d Ena:%d\n", (i+1),
				thisSlot->mutable_heater_location_upper()->setpoint_temp(),
				thisSlot->mutable_heater_location_upper()->thermistor_temp(),
				thisSlot->mutable_heater_location_upper()->is_open(),
				thisSlot->mutable_heater_location_upper()->is_shorted(),
				thisSlot->mutable_heater_location_upper()->is_overtemp(),
				thisSlot->mutable_heater_location_upper()->is_undertemp(),
				thisSlot->mutable_heater_location_upper()->is_enabled(),
				thisSlot->mutable_heater_location_lower()->setpoint_temp(),
				thisSlot->mutable_heater_location_lower()->thermistor_temp(),
				thisSlot->mutable_heater_location_lower()->is_open(),
				thisSlot->mutable_heater_location_lower()->is_shorted(),
				thisSlot->mutable_heater_location_lower()->is_overtemp(),
				thisSlot->mutable_heater_location_lower()->is_undertemp(),
				thisSlot->mutable_heater_location_lower()->is_enabled()
				);
			fprintf(snifferOutFile, "\tSlot %d Upper SP:%d Temp:%d  Open:%d Shor:%d OT:%d UT%d Ena:%d Lower SP:%d Temp:%d Open:%d Short:%d OT:%d UT%d Ena:%d\n", (i+1),
				thisSlot->mutable_heater_location_upper()->setpoint_temp(),
				thisSlot->mutable_heater_location_upper()->thermistor_temp(),
				thisSlot->mutable_heater_location_upper()->is_open(),
				thisSlot->mutable_heater_location_upper()->is_shorted(),
				thisSlot->mutable_heater_location_upper()->is_overtemp(),
				thisSlot->mutable_heater_location_upper()->is_undertemp(),
				thisSlot->mutable_heater_location_upper()->is_enabled(),
				thisSlot->mutable_heater_location_lower()->setpoint_temp(),
				thisSlot->mutable_heater_location_lower()->thermistor_temp(),
				thisSlot->mutable_heater_location_lower()->is_open(),
				thisSlot->mutable_heater_location_lower()->is_shorted(),
				thisSlot->mutable_heater_location_lower()->is_overtemp(),
				thisSlot->mutable_heater_location_lower()->is_undertemp(),
				thisSlot->mutable_heater_location_lower()->is_enabled()
				);
		}
		fflush(snifferOutFile);
#if (0)
		cout << "Deserialization:\n\n";
		deserialized.PrintDebugString();

		cout << "deserialized current time: " << TimeUtil::ToString(deserialized.system_data().current_time()) << "\n";
		cout << "           system up time: " << TimeUtil::ToString(deserialized.system_data().system_up_time()) << "\n";
		cout << "             ambient temp: " << deserialized.system_data().ambient_temp() << " Degrees\n";
		cout << "    controller IP address: " << deserialized.system_data().controller_ip_address() << "\n";
		cout << "   IntelGlass1 IP address: " << deserialized.system_data().intelligent_glass_1_ip_address() << "\n";
		cout << "      last time IG1 heard: " << TimeUtil::ToString(deserialized.system_data().last_time_intelligent_glass_1_heard()) << "\n";
		cout << "   IntelGlass2 IP address: " << deserialized.system_data().intelligent_glass_2_ip_address() << "\n";
		cout << "      last time IG2 heard: " << TimeUtil::ToString(deserialized.system_data().last_time_intelligent_glass_2_heard()) << "\n";
		cout << "           fan state name: " << FanState_Name(deserialized.system_data().fan_state()) << "\n";
		cout << "                fan state: " << deserialized.system_data().fan_state() << "\n";
		cout << "current power consumption: " << deserialized.system_data().current_power_consumption() << " Amps\n";
		cout << "            system status: " << SystemStatus_Name(deserialized.system_data().system_status()) << "\n";
		cout << "               alarm code: " << AlarmCode_Name(deserialized.system_data().alarm_code()) << "\n";
		cout << "            eco mode temp: " << deserialized.system_data().configured_eco_mode_temp() << " Degrees\n";
		cout << "         eco mode minutes: " << deserialized.system_data().configured_eco_mode_minutes() << " Minutes\n";
		cout << "           eco mode state: " << EcoModeState_Name(deserialized.system_data().eco_mode_state()) << "\n";
		cout << "       shutdown requested: " << deserialized.system_data().shutdown_requested() << "\n";
		SystemCommands sc = static_cast<SystemCommands>(deserialized.system_data().last_command_received());
		cout << "    last command received: " << SystemCommands_Name(sc) << "\n\n";
	
		for (int i = 0; i < deserialized.slot_data_size(); i++)
		{
			SlotData* thisSlot = deserialized.mutable_slot_data(i);
			SlotNumber sn = static_cast<SlotNumber>(i+1);
			cout << "              slot number: " << SlotNumber_Name(sn) << "\n";
			cout << "              lower state: " << HeaterState_Name(thisSlot->mutable_heater_location_lower()->state()) << "\n";
			cout << "           lower location: " << HeaterLocation_Name(thisSlot->mutable_heater_location_lower()->location()) << "\n";
			cout << "    lower thermistor temp: " << thisSlot->mutable_heater_location_lower()->thermistor_temp() << " Degrees\n";
			cout << "      lower setpoint temp: " << thisSlot->mutable_heater_location_lower()->setpoint_temp() << " Degrees\n";
			cout << "                LED state: " << LEDState_Name(thisSlot->mutable_heater_location_lower()->led_state()) << "\n";
			cout << "               start time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_lower()->start_time()) << "\n";
			cout << "                 end time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_lower()->end_time()) << "\n";
		
			cout << "           upper location: " << HeaterLocation_Name(thisSlot->mutable_heater_location_upper()->location()) << "\n";
			cout << "              upper state: " << HeaterState_Name(thisSlot->mutable_heater_location_upper()->state()) << "\n";
			cout << "    upper thermistor temp: " << thisSlot->mutable_heater_location_upper()->thermistor_temp() << " Degrees\n";
			cout << "      upper setpoint temp: " << thisSlot->mutable_heater_location_upper()->setpoint_temp() << " Degrees\n";
			cout << "                LED state: " << LEDState_Name(thisSlot->mutable_heater_location_upper()->led_state()) << "\n";
			cout << "               start time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_upper()->start_time()) << "\n";
			cout << "                 end time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_upper()->end_time()) << "\n";
			cout << "\n";
		}
		if (deserialized.serial_number().length())
		{
			cout << "            serial number: " << deserialized.serial_number() << "\n";
		}
		if (deserialized.model_number().length())
		{
			cout << "             model number: " << deserialized.model_number() << "\n";
		}
		if (deserialized.firmware_version().length())
		{
			cout << "         firmware version: " << deserialized.firmware_version() << "\n";
		}
#endif		
        memset(message, 0, sizeof(message));
    }
	pthread_exit(NULL);
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
    if( pthread_create( &statusListenerThread_ID, NULL, statusListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the statusListenerThread.\n");
        exit(-1);
    }

    if( pthread_create( &commandHandlerThread_ID, NULL, commandHandlerThread, NULL))
    {
        printf("Fail...Cannot spawn the commandHandlerThread.\n");
        exit(-1);
    }

    if( pthread_create( &commandHandlerThread2_ID, NULL, commandHandlerThread2, NULL))
    {
        printf("Fail...Cannot spawn the commandHandlerThread2.\n");
        exit(-1);
    }

    if( pthread_create( &commandResponseListenerThread_ID, NULL, commandResponseListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the commandResponseListenerThread.\n");
        exit(-1);
    }

    if( pthread_create( &commandResponseListenerThread2_ID, NULL, commandResponseListenerThread2, NULL))
    {
        printf("Fail...Cannot spawn the commandResponseListenerThread2.\n");
        exit(-1);
    }

    if( pthread_create( &heartBeatListenerThread_ID, NULL, heartBeatListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the heartBeatListenerThread.\n");
        exit(-1);
    }

    if( pthread_create( &heartBeatListenerThread2_ID, NULL, heartBeatListenerThread2, NULL))
    {
        printf("Fail...Cannot spawn the heartBeatListenerThread2.\n");
        exit(-1);
    }

    if( pthread_create( &timeSyncSubscriberThread_ID, NULL, timeSyncSubscriberThread, NULL))
    {
        printf("Fail...Cannot spawn the timeSyncSubscriberThread.\n");
        exit(-1);
    }

    if( pthread_create( &firmwareUpdateListenerThread_ID, NULL, firmwareUpdateListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the firmwareUpdateListenerThread.\n");
        exit(-1);
    }
    if( pthread_create( &firmwareResponseListenerThread_ID, NULL, firmwareResponseListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the firmwareResponseListenerThread.\n");
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
	// this is really only necessary for testing in order to stop the program
    struct sigaction sig_act;
    memset(&sig_act, 0x00, sizeof(sig_act));
    sig_act.sa_handler = handle_signals;
    sigaction(SIGINT, &sig_act, NULL);
    sigaction(SIGTERM, &sig_act, NULL);
    sigaction(SIGUSR1, &sig_act, NULL);
    sigaction(SIGSEGV, &sig_act, NULL);
    sigaction(SIGABRT, &sig_act, NULL);

    memset(controllerIPAddress, 0, sizeof(controllerIPAddress));
    memset(guiIPAddress1, 0, sizeof(guiIPAddress1));
    memset(guiIPAddress2, 0, sizeof(guiIPAddress2));
    memset(outputFilename, 0, sizeof(outputFilename));

    int i;
    if (5 == argc)
    {
        for (i = 1; i < argc; i++)
        {
            switch (i)
            {
                case 1:
                    strcpy(controllerIPAddress, argv[i]);
                    printf("controllerIPAddress = %s\n", controllerIPAddress);
                    break;
                case 2:
                    strcpy(guiIPAddress1, argv[i]);
                    printf("guiIPAddress1 = %s\n", guiIPAddress1);
                    break;
                case 3:
                    strcpy(guiIPAddress2, argv[i]);
                    printf("guiIPAddress2 = %s\n", guiIPAddress2);
                    break;
                case 4:
                    strcpy(outputFilename, argv[i]);
                    printf("outputFilename = %s\n", outputFilename);
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        printf("Usage ZeroMQSniffer <controllerIPAddress> <GUI1IPAddress> <GUI2IPAddress> <output_filename>\n");
        exit(-1);
    }

	int ret = initializeZeroMQ();
	if (0 == ret)
	{
		printf("ZeroMQ initialization successful\n");
	}
	else
	{
		printf("ZeroMQ initialization failed ret = %d\n", ret);
	}
	snifferOutFile = fopen(outputFilename, "w");
	if (snifferOutFile == NULL)
	{
		printf("ERROR: cannot open %s\n", outputFilename);
		exit(-1);
	}

	// start task threads
	createThreads();
	
	// this loop just spins forever until the power is cut.
	// the task threads do all of the heavy lifting.
	while (1)
	{
		sleep(1);
	}

	exit(0);
}
