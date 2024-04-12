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
#include "uhc.pb.h"
#include "uhc_proto.h"
#include "frontier_uhc.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

void *context;
void *subscriber;
void *publisher;

pthread_t subscriberThread_ID;
pthread_t publisherThread_ID;

char controllerIPAddress[32];
char myIPAddress[32];
char guiNumber[32];


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
                       zmq_close (publisher);
                       zmq_close (subscriber);
                       zmq_ctx_destroy (context);
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



void *subscriberThread(void *)
{
	char messageType[] = { 10, 3, 'C', 'S', 'S' };
	char controllerIPAddressAndPort[32];
	
	memset(controllerIPAddressAndPort, 0, sizeof(controllerIPAddressAndPort));

    subscriber = zmq_socket(context, ZMQ_SUB);
	sprintf(controllerIPAddressAndPort, "tcp://%s:%d", controllerIPAddress, CURRENT_SYSTEM_STATE_PORT_CONTROLLER);
    int rc = zmq_connect(subscriber, controllerIPAddressAndPort);
    assert(rc == 0);
    rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, messageType, sizeof(messageType));
    assert(rc == 0);

    char message[1024];
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
			pthread_exit(NULL);
		}

		cout << "Deserialization:\n\n";
//		deserialized.PrintDebugString();

		cout << "deserialized current time: " << TimeUtil::ToString(deserialized.system_data().current_time()) << "\n";
		cout << "           system up time: " << TimeUtil::ToString(deserialized.system_data().system_up_time()) << "\n";
		cout << "            heatsink temp: " << deserialized.system_data().heatsink_temp() << " Degrees\n";
		cout << "    controller IP address: " << deserialized.system_data().controller_ip_address() << "\n";
		cout << "   IntelGlass1 IP address: " << deserialized.system_data().intelligent_glass_1_ip_address() << "\n";
		cout << "      last time IG1 heard: " << TimeUtil::ToString(deserialized.system_data().last_time_intelligent_glass_1_heard()) << "\n";
		cout << "   IntelGlass2 IP address: " << deserialized.system_data().intelligent_glass_2_ip_address() << "\n";
		cout << "      last time IG2 heard: " << TimeUtil::ToString(deserialized.system_data().last_time_intelligent_glass_2_heard()) << "\n";
		cout << "          fan state1 name: " << FanState_Name(deserialized.system_data().fan_state1()) << "\n";
		cout << "               fan1 state: " << deserialized.system_data().fan_state1() << "\n";
		cout << "          fan state2 name: " << FanState_Name(deserialized.system_data().fan_state2()) << "\n";
		cout << "               fan2 state: " << deserialized.system_data().fan_state2() << "\n";
		cout << "current power consumption: " << deserialized.system_data().current_power_consumption() << " Amps\n";
		cout << "            system status: " << SystemStatus_Name(deserialized.system_data().system_status()) << "\n";
		cout << "               alarm code: " << AlarmCode_Name(deserialized.system_data().alarm_code()) << "\n";
		cout << "            eco mode temp: " << deserialized.system_data().configured_eco_mode_temp() << " Degrees\n";
		cout << "         eco mode minutes: " << deserialized.system_data().configured_eco_mode_minutes() << " Minutes\n";
		cout << "           eco mode state: " << EcoModeState_Name(deserialized.system_data().eco_mode_state()) << "\n";
		cout << "  logging is event driven: " << deserialized.system_data().logging_is_event_driven() << "\n";
		cout << "   logging period seconds: " << deserialized.system_data().logging_period_seconds() << "\n";
		cout << "                 nso mode: " << deserialized.system_data().nso_mode() << "\n";
		cout << "                demo mode: " << deserialized.system_data().demo_mode() << "\n";
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
			cout << "            lower is_open: " << thisSlot->mutable_heater_location_lower()->is_open() << "\n";
			cout << "         lower is_shorted: " << thisSlot->mutable_heater_location_lower()->is_shorted() << "\n";
			cout << "                LED state: " << LEDState_Name(thisSlot->mutable_heater_location_lower()->led_state()) << "\n";
			cout << "               start time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_lower()->start_time()) << "\n";
			cout << "                 end time: " << TimeUtil::ToString(thisSlot->mutable_heater_location_lower()->end_time()) << "\n";
		
			cout << "           upper location: " << HeaterLocation_Name(thisSlot->mutable_heater_location_upper()->location()) << "\n";
			cout << "              upper state: " << HeaterState_Name(thisSlot->mutable_heater_location_upper()->state()) << "\n";
			cout << "    upper thermistor temp: " << thisSlot->mutable_heater_location_upper()->thermistor_temp() << " Degrees\n";
			cout << "      upper setpoint temp: " << thisSlot->mutable_heater_location_upper()->setpoint_temp() << " Degrees\n";
			cout << "            upper is_open: " << thisSlot->mutable_heater_location_upper()->is_open() << "\n";
			cout << "         upper is_shorted: " << thisSlot->mutable_heater_location_upper()->is_shorted() << "\n";
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
		cout << "           board revision: " << deserialized.hardware_revision() << "\n";
		if (deserialized.system_data().hp_error_code().length())
		{
			cout << "               error code: " << deserialized.system_data().hp_error_code() << "\n";
		}

        memset(message, 0, sizeof(message));
    }
	pthread_exit(NULL);
}


void *publisherThread(void *)
{
	struct timeval tv;
	char publisherIPAddressAndPort[32];
	
	memset(publisherIPAddressAndPort, 0, sizeof(publisherIPAddressAndPort));
	if (!strcmp(guiNumber, "1"))
	{
		sprintf(publisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, HEARTBEAT_PORT_GUI1);
	}
	else
	{
		sprintf(publisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, HEARTBEAT_PORT_GUI2);
	}
printf("publisherIPAddressAndPort = %s\n", publisherIPAddressAndPort);
    publisher = zmq_socket(context, ZMQ_PUB);
    int rc = zmq_bind(publisher, publisherIPAddressAndPort);
    assert(rc == 0);
	uint32_t sequenceNumber = 0;

    while(1)
    {
		HeartBeat s;
		::google::protobuf::Timestamp* timestamp = new ::google::protobuf::Timestamp();
		gettimeofday(&tv, NULL);
		timestamp->set_seconds(tv.tv_sec);
		timestamp->set_nanos(tv.tv_usec * 1000);
	
		s.set_topic("HB");
		s.set_sender_ip_address(myIPAddress);
		s.set_sequence_number(sequenceNumber++);

//		cout << "Serialization:\n\n" << s.DebugString() << "\n\n";
//		s.PrintDebugString();

		string serialized;
		if ( !s.SerializeToString( &serialized ) )
		{
			cerr << "ERROR: Unable to serialize!\n";
		}
		char bytes[serialized.length()];
		memcpy(bytes, serialized.data(), serialized.length());

//		cout << "Serialized length: " << serialized.length() << "\n";
        rc = zmq_send(publisher, bytes, serialized.length(), 0);
        assert(rc == serialized.length());
		cout << "Bytes sent: " << rc << "\n";
		sleep(1);
    }
	pthread_exit(NULL);
}


/*******************************************************************************************/
/*                                                                                         */
/* int main()                                                                              */
/*                                                                                         */
/* Simple example to show the use of ZeroMQ's subscribe model                              */
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
	memset(myIPAddress, 0, sizeof(myIPAddress));
	
	int i;
	if (argc == 4)
	{
		for (i = 1; i < argc; i++)
		{
		    switch(i)
			{
			    case 1:
				   strcpy(controllerIPAddress, argv[i]);
				   printf("controllerIPAddress = %s\n", controllerIPAddress);
				   break;
				case 2:
				   strcpy(myIPAddress, argv[i]);
				   printf("myIPAddress = %s\n", myIPAddress);
				   break;
				case 3:
				   strcpy(guiNumber, argv[i]);
				   printf("guiNumber = %s\n", guiNumber);
				   break;
				default:
				   break;
			}
		}
	}
	else
	{
		printf("Usage: sub <controllerIPAddress> <GUIIPAddress> <1 or 2 for GUI1 or GUI2>\n");
		exit(-1);
	}
    context = zmq_ctx_new();

    if( pthread_create( &publisherThread_ID, NULL, publisherThread, NULL))
    {
        printf("Fail...Cannot spawn the publisherThread.\n");
        exit(-1);
    }
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
