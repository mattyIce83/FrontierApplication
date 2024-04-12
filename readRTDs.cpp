#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <ctime>
#include <google/protobuf/util/time_util.h>
#include "uhc_proto.h"
#include "uhc.pb.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

void *context;
void *subscriber;

pthread_t subscriberThread_ID;
char controllerIPAddress[32];
bool sigTermReceived = false;



/*******************************************************************************************/
/*                                                                                         */
/* static void handle_signals(int signum)                                                  */
/*                                                                                         */
/* SIGINT & SIGTERM handler: set doQuit to true for graceful termination                   */
/*                                                                                         */
/*******************************************************************************************/
static void handle_signals(int signum)
{
    switch (signum)
    {
        case SIGUSR1:
        case SIGSEGV:
        case SIGABRT:
        case SIGINT:
           sigTermReceived = true;
           zmq_close (subscriber);
           zmq_ctx_destroy (context);
           exit(0);
           break;

        default:
            break;
    }
}   // handle_signals()



void *subscriberThread(void *)
{
   char messageType[] = { 10, 3, 'R', 'T', 'D' };
   char controllerIPAddressAndPort[64];
   
   memset(controllerIPAddressAndPort, 0, sizeof(controllerIPAddressAndPort));

    subscriber = zmq_socket(context, ZMQ_SUB);
   sprintf(controllerIPAddressAndPort, "tcp://%s:%d", controllerIPAddress, RTD_DATA_PUBLISHER_PORT);
    int rc = zmq_connect(subscriber, controllerIPAddressAndPort);
    assert(rc == 0);
    rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

   char message[2048];
  	char str[16];
   memset(message, 0, sizeof(message));

    while(!sigTermReceived)
    {
        rc = zmq_recv(subscriber, message, sizeof(message), 0);
        if (rc == -1)
        {
           break;
        }

        // Deserialization
        string s(message, rc);
 

        ReadRTDs deserialized;
        if ( !deserialized.ParseFromString( s ) )
        {
           cerr << "ERROR: Unable to deserialize!\n";
           pthread_exit(NULL);
           exit(0);
        }

//		  cout << "Deserialization:\n\n";
//		  deserialized.PrintDebugString();

        cout << "Controller IP Address: " << deserialized.controller_ip_address() << "\n";
        cout << "Board Hardware Revision: " << deserialized.hardware_revision() << "\n";

        printf(" NUMBER        LOCATION              VOLTAGE   ADC COUNTS  TEMPERATURE  SHORTED/OPEN\n");
        printf("======================================================================================\n");
   
        for (int i = 0; i < deserialized.rtd_data_size(); i++)
        {
           RTDData* thisRTD = deserialized.mutable_rtd_data(i);
           memset(str, 0, sizeof(str));
           if (thisRTD->is_open())
           {
              strcpy(str, "Open");
           }
           if (thisRTD->is_shorted())
           {
              strcpy(str, "Shorted");
           }
           printf("RTD [%2d] %23s     %0.5fV     %4d      %5d F        %s\n", thisRTD->rtd_number() , thisRTD->location().c_str(), thisRTD->voltage(),  thisRTD->raw_counts(), thisRTD->temperature(), str);

        }
        printf("\n\n\n");
        memset(message, 0, sizeof(message));
    }
    pthread_exit(NULL);
    exit(0);
}


/*******************************************************************************************/
/*                                                                                         */
/* int main()                                                                              */
/*                                                                                         */
/* Simple example to show the use of ZeroMQ's subscribe model while reading RTD data       */
/*                                                                                         */
/*******************************************************************************************/
int main(int argc, char *argv[])
{
    struct sigaction sig_act;
    memset(&sig_act, 0x00, sizeof(sig_act));
    sig_act.sa_handler = handle_signals;
    sigaction(SIGINT, &sig_act, NULL);
    sigaction(SIGTERM, &sig_act, NULL);
    sigaction(SIGUSR1, &sig_act, NULL);
    sigaction(SIGSEGV, &sig_act, NULL);
    sigaction(SIGABRT, &sig_act, NULL);

    memset(controllerIPAddress, 0, sizeof(controllerIPAddress));
    strcpy(controllerIPAddress, "192.168.1.200");
    context = zmq_ctx_new();

    if( pthread_create( &subscriberThread_ID, NULL, subscriberThread, NULL))
    {
        printf("Fail...Cannot spawn the subscriberThread.\n");
        exit(-1);
    }
   
   while (1)
   {
      sleep(1);
   }

    return 0;
}
