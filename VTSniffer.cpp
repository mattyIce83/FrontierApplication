/******************************************************************************/
/*                                                                            */
/* FILE:        VTSniffer.cpp                                                 */
/*                                                                            */
/* DESCRIPTION: Routines to implement a ZeroMQ sniffer to display all Vantron */
/*            intra HMI messages in the HennyPenny Frontier UHC for McDonalds */
/*                                                                            */
/* AUTHOR(S):   USA Firmware, LLC                                             */
/*                                                                            */
/* DATE:        Nov 16, 2023                                                  */
/*                                                                            */
/* This is an unpublished work subject to Trade Secret and Copyright          */
/* protection by HennyPenny and USA Firmware, LLC                             */
/*                                                                            */
/* USA Firmware, LLC                                                          */
/* 10060 Brecksville Road Brecksville, OH 44141                               */
/*                                                                            */
/* EDIT HISTORY:                                                              */
/* Nov 16, 2023 - Initial release                                             */
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

#include "frontier_uhc.h"
//#include "uhc.pb.h"
#include "sync.pb.h"
#include "uhc_proto.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

// global variables
void *context;
void *syncSubSubscriber;
void *syncPubSubscriber;

char guiIPAddress1[IP_STRING_SIZE];
char guiIPAddress2[IP_STRING_SIZE];
char outputFilename[MAX_FILE_PATH];
char timeString[128];

// thread IDs
pthread_t syncSubListenerThread_ID;
pthread_t syncPubListenerThread_ID;

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
					   zmq_close (syncSubSubscriber);
					   zmq_close (syncPubSubscriber);
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
/* void *syncSubListenerThread(void *)                                                     */
/*                                                                                         */
/* Receive the SyncDataMessage message from GUI1 and display it.                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *syncSubListenerThread(void *)
{
	char messageType[] = { 10, 4, 'G', '2', 'R', 'Q' };

	char guiIP[IP_STRING_SIZE];

	memset(guiIP, 0, sizeof(guiIP));
	sprintf(guiIP, "tcp://%s:%d", guiIPAddress2, SYNC_BACKEND_SUB_PORT);

    syncSubSubscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(syncSubSubscriber, guiIP);
    assert(rc == 0);

    rc = zmq_setsockopt(syncSubSubscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	memset(message, 0, sizeof(message));


    while(1)
    {
        rc = zmq_recv(syncSubSubscriber, message, sizeof(message), 0);
//        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SyncDataMessage deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			cerr << "ERROR: Unable to deserialize!\n";
		}
printf("%s G2RQ received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s G2RQ received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fflush(snifferOutFile);

        printf("UUID:      %s\n", deserialized.uuid().c_str());
        printf("CMD:       %s\n", deserialized.cmd().c_str());
        printf("PARAMETER: %s\n", deserialized.parameter().c_str());
        printf("BODY:      %s\n\n", deserialized.body().c_str());
		
        fprintf(snifferOutFile, "UUID:      %s\n", deserialized.uuid().c_str());
        fprintf(snifferOutFile, "CMD:       %s\n", deserialized.cmd().c_str());
        fprintf(snifferOutFile, "PARAMETER: %s\n", deserialized.parameter().c_str());
        fprintf(snifferOutFile, "BODY:      %s\n\n", deserialized.body().c_str());
		fflush(snifferOutFile);

        memset(message, 0, sizeof(message));
    }
	pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* void *syncPubListenerThread(void *)                                                     */
/*                                                                                         */
/* Receive the SyncDataMessage message from GUI2 and display it.                           */
/*                                                                                         */
/* Returns: pthread_exit(NULL)                                                             */
/*                                                                                         */
/*******************************************************************************************/
void *syncPubListenerThread(void *)
{
	char messageType[] = { 10, 4, 'G', '1', 'R', 'S' };

	char guiIP[IP_STRING_SIZE];

	memset(guiIP, 0, sizeof(guiIP));
	sprintf(guiIP, "tcp://%s:%d", guiIPAddress1, SYNC_BACKEND_PUB_PORT);

    syncPubSubscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(syncPubSubscriber, guiIP);
    assert(rc == 0);

    rc = zmq_setsockopt(syncPubSubscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    char message[DEFAULT_MESSAGE_BUFFER_SIZE];
	memset(message, 0, sizeof(message));


    while(1)
    {
        rc = zmq_recv(syncPubSubscriber, message, sizeof(message), 0);
//        assert(rc != -1);

		// Deserialization
		string s(message, rc);

		SyncDataMessage deserialized;
		if ( !deserialized.ParseFromString( s ) )
		{
			cerr << "ERROR: Unable to deserialize!\n";
		}
printf("%s G1RS received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fprintf(snifferOutFile, "%s G1RS received seqNumber = %d\n", getTimeStampString(), deserialized.sequence_number());
fflush(snifferOutFile);

        printf("UUID:      %s\n", deserialized.uuid().c_str());
        printf("CMD:       %s\n", deserialized.cmd().c_str());
        printf("PARAMETER: %s\n", deserialized.parameter().c_str());
        printf("BODY:      %s\n\n", deserialized.body().c_str());
		
        fprintf(snifferOutFile, "UUID:      %s\n", deserialized.uuid().c_str());
        fprintf(snifferOutFile, "CMD:       %s\n", deserialized.cmd().c_str());
        fprintf(snifferOutFile, "PARAMETER: %s\n", deserialized.parameter().c_str());
        fprintf(snifferOutFile, "BODY:      %s\n\n", deserialized.body().c_str());
		fflush(snifferOutFile);
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
    if( pthread_create( &syncSubListenerThread_ID, NULL, syncSubListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the syncSubListenerThread.\n");
        exit(-1);
    }
    if( pthread_create( &syncPubListenerThread_ID, NULL, syncPubListenerThread, NULL))
    {
        printf("Fail...Cannot spawn the syncPubListenerThread.\n");
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

    memset(guiIPAddress1, 0, sizeof(guiIPAddress1));
    memset(guiIPAddress2, 0, sizeof(guiIPAddress2));
    memset(outputFilename, 0, sizeof(outputFilename));

    int i;
    if (4 == argc)
    {
        for (i = 1; i < argc; i++)
        {
            switch (i)
            {
                case 1:
                    strcpy(guiIPAddress1, argv[i]);
                    printf("guiIPAddress1 = %s\n", guiIPAddress1);
                    break;
                case 2:
                    strcpy(guiIPAddress2, argv[i]);
                    printf("guiIPAddress2 = %s\n", guiIPAddress2);
                    break;
                case 3:
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
        printf("Usage VTSniffer <GUI1IPAddress> <GUI2IPAddress> <output_filename>\n");
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
