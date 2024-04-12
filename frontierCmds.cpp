#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <ctime>
#include <google/protobuf/util/time_util.h>
#include <zmq.h>
#include <dirent.h>

#include "frontier_uhc.h"
#include "uhc.pb.h"
#include "uhc_proto.h"

using namespace uhc;
using namespace std;
using namespace google::protobuf::util;

void *context;
void *publisher;
void *publisherTS;
void *subscriber;

pthread_t publisherThread_ID;
pthread_t subscriberThread_ID;

char controllerIPAddress[32];
char myIPAddress[32];
char guiNumber[32];
bool quit = false;
bool responseReceived = false;
uint32_t sequenceNumber = 0;

SystemCommand sc;

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
   else
   {
      char publisherIPAddressAndPort[32];
      char tsPublisherIPAddressAndPort[32];

      memset(publisherIPAddressAndPort, 0, sizeof(publisherIPAddressAndPort));
      memset(tsPublisherIPAddressAndPort, 0, sizeof(tsPublisherIPAddressAndPort));
      if (!strcmp(guiNumber, "1"))
      {
         sprintf(publisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, SYSTEM_COMMAND_PORT_GUI1);
         sprintf(tsPublisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, TIME_SYNC_PORT_GUI1);

      }
      else
      {
         sprintf(publisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, SYSTEM_COMMAND_PORT_GUI2);
         sprintf(tsPublisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, TIME_SYNC_PORT_GUI1);
      }
//		sprintf(publisherIPAddressAndPort, "tcp://%s:%d", myIPAddress, SYSTEM_COMMAND_PORT_GUI1);
      printf("publisherIPAddressAndPort = %s\n", publisherIPAddressAndPort);
      printf("tsPublisherIPAddressAndPort = %s\n", tsPublisherIPAddressAndPort);

      publisher = zmq_socket(context, ZMQ_PUB);
      int rc = zmq_bind(publisher, publisherIPAddressAndPort);
      assert(rc == 0);

      publisherTS = zmq_socket(context, ZMQ_PUB);
      rc = zmq_bind(publisherTS, tsPublisherIPAddressAndPort);
      assert(rc == 0);
   }
   return ret;
}

void exitProgram()
{
    zmq_close (publisher);
    zmq_close (subscriber);
   zmq_close (publisherTS);
    zmq_ctx_destroy (context);
}

void sendCommand(SystemCommand s)
{
   printf("sendCommand called\n");
   s.set_topic(SYSTEM_COMMAND_TOPIC);
   s.set_sender_ip_address(myIPAddress);
   s.set_sequence_number(sequenceNumber++);
   string serialized;
   if ( !s.SerializeToString( &serialized ) )
   {
      cerr << "ERROR: Unable to serialize!\n";
   }
   char bytes[serialized.length()];
   memcpy(bytes, serialized.data(), serialized.length());

//	cout << "Serialized length: " << serialized.length() << "\n";
   int rc = zmq_send(publisher, bytes, serialized.length(), 0);
   assert(rc == serialized.length());
   cout << "Bytes sent: " << rc << "\n";
}


void sendTimeSync(TimeSync s)
{
   printf("sendTimeSync called\n");
   struct timeval now;
   
   s.set_topic(TIME_SYNC_TOPIC);
   s.set_sender_ip_address(myIPAddress);
   s.set_sequence_number(sequenceNumber++);
   ::google::protobuf::Timestamp* currentTime = new ::google::protobuf::Timestamp();
   gettimeofday(&now, NULL);
   currentTime->set_seconds(now.tv_sec);
   currentTime->set_nanos(0);
   s.set_allocated_current_time(currentTime);
   string serialized;
   if ( !s.SerializeToString( &serialized ) )
   {
      cerr << "ERROR: Unable to serialize!\n";
   }
   char bytes[serialized.length()];
   memcpy(bytes, serialized.data(), serialized.length());

//		cout << "Serialized length: " << serialized.length() << "\n";
   int rc = zmq_send(publisherTS, bytes, serialized.length(), 0);
   assert(rc == serialized.length());
   cout << "Bytes sent: " << rc << "\n";
}

void *subscriberThread(void *)
{
   char messageType[] = { 10, 3, 'R', 'S', 'P' };
   char subscriberIPAddressAndPort[32];

   memset(subscriberIPAddressAndPort, 0, sizeof(subscriberIPAddressAndPort));
   sprintf(subscriberIPAddressAndPort, "tcp://%s:%d", controllerIPAddress, SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER);
printf("subscriberIPAddressAndPort = %s\n", subscriberIPAddressAndPort);

    subscriber = zmq_socket(context, ZMQ_SUB);
    int rc = zmq_connect(subscriber, subscriberIPAddressAndPort);
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

      SystemCommandResponse deserialized;
      if ( !deserialized.ParseFromString( s ) )
      {
         cerr << "ERROR: Unable to deserialize!\n";
         pthread_exit(NULL);
      }

//		cout << "Deserialization:\n\n";
//		deserialized.PrintDebugString();

      cout << "          sequence number: " << deserialized.sequence_number() << "\n";
      SystemCommandResponses r = static_cast<SystemCommandResponses>(deserialized.response());
      cout << "                 response: " << SystemCommandResponses_Name(r) << "\n";
      SlotNumber sn = static_cast<SlotNumber>(deserialized.slot_number());
      cout << "              slot number: " << SlotNumber_Name(sn) << "\n";
      if (deserialized.response() == SYSTEM_COMMAND_RESPONSE_LINK_ESTABLISHED)
      {
         printf("response = SYSTEM_COMMAND_RESPONSE_LINK_ESTABLISHED\n");
         responseReceived = 1;
//			break;
      }
      if (deserialized.response() == SYSTEM_COMMAND_RESPONSE_OK)
      {
         printf("response = SYSTEM_COMMAND_RESPONSE_OK\n");
         quit = 1;
         break;
      }
   
        memset(message, 0, sizeof(message));
    }
   quit = 1;
   pthread_exit(NULL);
}

void timeSyncCommandMenu()
{
   char option = 0;
   TimeSync ts;
   struct timeval now;
   

   while (option != 'x')
   {
      system("clear");
        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("TIMESYNC COMMAND MENU\n");
      printf("======================================================\n");
        printf("[1]Send TimeSync commmand\n");
        printf("[x]Previous menu\n");

        option = getchar();

        //Selects the course of action specified by the option
        switch(option)
        {
         case '1':
            ts.set_is_master(true);
            sendTimeSync(ts);
            break;
            case 'x':
                     //Return to main menu
                     return;
            default :
                     //User enters wrong input
                     //TO DO CODE
                     break;
        }
   }
}


void nsoModeCommandMenu()
{
   char option = 0;
   SystemCommand sc;

   while (option != 'x')
   {
      system("clear");
      //Clears the keyboard buffer
      fflush(stdin);
      //Outputs the options to console
      printf("NSO MODE COMMAND MENU\n");
      printf("======================================================\n");
      printf("[1]Send NSO Mode ON commmand\n");
      printf("[x]Previous menu\n");

      option = getchar();

      //Selects the course of action specified by the option
      switch(option)
      {
         case '1':
            sc.set_command(SYSTEM_COMMAND_NSO_MODE_ON);
            sendCommand(sc);
            break;
         case 'x':
            //Return to main menu
            return;
         default :
            //User enters wrong input
            //TO DO CODE
            break;
      }
   }
}


void demoModeCommandMenu()
{
   char option = 0;
   SystemCommand sc;

   while (option != 'x')
   {
      system("clear");
      //Clears the keyboard buffer
      fflush(stdin);
      //Outputs the options to console
      printf("DEMO MODE COMMAND MENU\n");
      printf("======================================================\n");
      printf("[1]Send Demo Mode ON commmand\n");
      printf("[2]Send Demo Mode OFF commmand\n");
      printf("[x]Previous menu\n");

      option = getchar();

      //Selects the course of action specified by the option
      switch(option)
      {
         case '1':
            sc.set_command(SYSTEM_COMMAND_DEMO_MODE_ON);
            sendCommand(sc);
            break;
         case '2':
            sc.set_command(SYSTEM_COMMAND_DEMO_MODE_OFF);
            sendCommand(sc);
            break;
         case 'x':
            //Return to main menu
            return;
         default :
            //User enters wrong input
            //TO DO CODE
            break;
      }
   }
}


void configureLoggingMenu()
{
   char option = 0;
   char slot = 'a';
   char stringEntered[16];
   uint32_t loggingPeriodSeconds = 3;
   bool loggingIsEventDriven = false;
   SystemCommand sc;
   uint32_t sequenceNumber = 0;

   sc.set_command(SYSTEM_COMMAND_CONFIGURE_LOGGING);

   while (option != 'x')
   {
      system("clear");
      memset(stringEntered, 0, sizeof(stringEntered));
      //Clears the keyboard buffer
      fflush(stdin);
      //Outputs the options to console
      printf("CONFIGURE LOGGING MENU\n");
      printf("======================================================\n");
      printf("[1]Configure event driven false\n");
      printf("[2]Configure event driven true\n");
      printf("[3]Enter logging perios in seconds value\n");
      printf("[4]Send configure logging message\n");
      printf("[x]Previous menu\n");
      //Reads the user's option
//		scanf("%s", &option)
      option = getchar();
      //Selects the course of action specified by the option
      switch(option)
      {
         case '1':
            loggingIsEventDriven = false;
            sc.set_logging_is_event_driven(loggingIsEventDriven);
            break;
         case '2':
            loggingIsEventDriven = true;
            sc.set_logging_is_event_driven(loggingIsEventDriven);
            break;
         case '3':
            printf("Enter new logging period in seconds (currently %d): ", loggingPeriodSeconds);
            scanf("%s", stringEntered);
            loggingPeriodSeconds = atoi(stringEntered);
            printf("New logging period = %d seconds\n", loggingPeriodSeconds);
            sc.set_logging_period_seconds(loggingPeriodSeconds);
            break;
         case '4':
            sendCommand(sc);
            break;
         case 'x':
//					 printf("Return to main menu\n");
            return;
         default :
            //User enters wrong input
            //TO DO CODE
            break;
        }
   }
}

void fanCommandMenu()
{
   char option = 0;
   SystemCommand sc;
   struct timeval now;
   char slot = 'a';
   char stringEntered[16];
   int index = 0;
   bool allSelected = false;
   FanNumber fan_number = FAN_NUMBER_BOTH;
   

   while (option != 'x')
   {
      system("clear");
      //Clears the keyboard buffer
      fflush(stdin);
      //Outputs the options to console
      printf("FAN COMMAND MENU\n");
      printf("======================================================\n");
      printf("[1]Select Fan Number\n");
      printf("[2]Send FAN_ON commmand\n");
      printf("[3]Send FAN_OFF commmand\n");
      printf("[x]Previous menu\n");

      option = getchar();

      //Selects the course of action specified by the option
      switch(option)
      {
         case '1':
            printf("Enter Fan Number (1, 2, b for both): ");
            scanf("%s", stringEntered);
            if ((stringEntered[0] == 'b') || (stringEntered[0] == 'B'))
            {
               printf("Fan selected = BOTH\n");
               sc.set_fan_number(FAN_NUMBER_BOTH);
            }
            else if (stringEntered[0] == '1')
            {
               printf("Fan selected = %s\n", stringEntered);
               sc.set_fan_number(FAN_NUMBER_1);
            }
            else if (stringEntered[0] == '2')
            {
               printf("Fan selected = %s\n", stringEntered);
               sc.set_fan_number(FAN_NUMBER_2);
            }
            else
            {
               printf("Wrong Fan selected = %s\n", stringEntered);
               sc.set_fan_number(FAN_NUMBER_BOTH);
            }
            break;
         case '2':
            sc.set_command(SYSTEM_COMMAND_FAN_ON);
            sendCommand(sc);
            break;
         case '3':
            sc.set_command(SYSTEM_COMMAND_FAN_OFF);
            sendCommand(sc);
            break;
         case 'x':
            //Return to main menu
            return;
         default :
            //User enters wrong input
            //TO DO CODE
            break;
      }
   }
}


void cleaningModeCommandMenu()
{
   char option = 0;
   SystemCommand sc;
   struct timeval now;
   char slot = 'a';
   char stringEntered[16];
   int heaterIndex = 0;
   bool allSelected = false;
   

   while (option != 'x')
   {
      system("clear");
        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("CLEANING MODE COMMAND MENU\n");
      printf("======================================================\n");
        printf("[1]Send CLEANING_MODE_ON commmand\n");
        printf("[2]Send CLEANING_MODE_OFF commmand\n");
        printf("[x]Previous menu\n");

        option = getchar();

        //Selects the course of action specified by the option
        switch(option)
        {
         case '1':
            sc.set_command(SYSTEM_COMMAND_CLEANING_MODE_ON);
            sendCommand(sc);
            break;
         case '2':
            sc.set_command(SYSTEM_COMMAND_CLEANING_MODE_OFF);
            sendCommand(sc);
            break;
            case 'x':
                     //Return to main menu
                     return;
            default :
                     //User enters wrong input
                     //TO DO CODE
                     break;
        }
   }
}


void heaterCommandMenu()
{
   char option = 0;
   SystemCommand sc;
   struct timeval now;
   char slot = 'a';
   char stringEntered[16];
   int heaterIndex = 0;
   bool allSelected = false;
   

   while (option != 'x')
   {
      system("clear");
        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("HEATERS COMMAND MENU\n");
      printf("======================================================\n");
        printf("[1]Enter Heater Index\n");
        printf("[2]Setup HEATER_ON commmand\n");
        printf("[3]Setup HEATER_OFF commmand\n");
        printf("[4]Send heater commmand\n");
        printf("[x]Previous menu\n");

        option = getchar();

        //Selects the course of action specified by the option
        switch(option)
        {
            case '1':
                printf("Enter Heater Index (1 - 12, or a for all): ");
                scanf("%s", stringEntered);
                if ((stringEntered[0] == 'a') || (stringEntered[0] == 'A'))
                {
                  printf("heaterIndex = ALL\n");
                  allSelected = true;
                }
                else
                {
                  heaterIndex = atoi(stringEntered);
                  printf("heaterIndex = %d\n", heaterIndex);
                  sc.set_heater_index(heaterIndex-1);
                  allSelected = false;
                }
                     break;
         case '2':
            sc.set_command(SYSTEM_COMMAND_HEATER_ON);
            break;
         case '3':
            sc.set_command(SYSTEM_COMMAND_HEATER_OFF);
            break;
            case '4':
               {
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  if (allSelected)
                  {
                     for (int i = 0; i < NUM_HEATERS; i++)
                     {
                        sc.set_heater_index(i);
                        sendCommand(sc);
                     }
                  }
                  else
                  {
                     sendCommand(sc);
                  }
               }
                     break;
            case 'x':
                     //Return to main menu
                     return;
            default :
                     //User enters wrong input
                     //TO DO CODE
                     break;
        }
   }
}


void startupCommandMenu()
{
   char option = 0;
   SystemCommand sc;
   struct timeval now;
   

   while (option != 'x')
   {
      system("clear");
        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("STARTUP / ESTOP / SHUTDOWN / IDLE COMMAND MENU\n");
      printf("======================================================\n");
        printf("[1]Send STARTUP commmand\n");
        printf("[2]Send EMERGENCY_STOP commmand\n");
        printf("[3]Send SHUTDOWN commmand\n");
        printf("[4]Send IDLE commmand\n");
        printf("[x]Previous menu\n");

        option = getchar();

        //Selects the course of action specified by the option
        switch(option)
        {
            case '1':
               {
                  sc.set_command(SYSTEM_COMMAND_STARTUP);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  sendCommand(sc);
               }
               break;
            case '2':
               {
                  sc.set_command(SYSTEM_COMMAND_EMERGENCY_STOP);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  sendCommand(sc);
               }
               break;
            case '3':
               {
                  sc.set_command(SYSTEM_COMMAND_SHUTDOWN_REQUESTED);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  sendCommand(sc);
               }
               break;
            case '4':
               {
                  sc.set_command(SYSTEM_COMMAND_IDLE);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  sendCommand(sc);
               }
               break;
            case 'x':
                     //Return to main menu
                     return;
            default :
                     //User enters wrong input
                     //TO DO CODE
                     break;
        }
   }
}

void changeSetpointMenu()
{
   char option = 0;
   char slot = 'a';
   char stringEntered[16];
   int setpoint = 165;
   int upperSetpoint = 165;
   int lowerSetpoint = 165;
   int slotNumber = 0;
   SystemCommand sc;
   struct timeval now;
   uint32_t sequenceNumber = 0;
   bool allSelected = false;
   

   while (option != 'x')
   {
      system("clear");
      memset(stringEntered, 0, sizeof(stringEntered));
        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("CHANGE SETPOINT MENU\n");
      printf("======================================================\n");
        printf("[1]Enter setpoint value\n");
        printf("[2]Enter slot number\n");
        printf("[3]Send set setpoint slot message\n");
        printf("[4]Enter upper setpoint value\n");
        printf("[5]Enter lower setpoint value\n");
        printf("[6]Send set setpoint heater message\n");
        printf("[x]Previous menu\n");
        //Reads the user's option
//		scanf("%s", &option)
        option = getchar();
        //Selects the course of action specified by the option
        switch(option)
        {
            case '1':
                printf("Enter new setpoint (165 - 200) (currently %d): ", setpoint);
                scanf("%s", stringEntered);
                setpoint = atoi(stringEntered);
                printf("New Setpoint = %d\n", setpoint);
                sc.set_temperature(setpoint);
                     break;
            case '2':
                printf("Enter slot number (1 - 6, or a for all): ");
                scanf("%s", stringEntered);
                if ((stringEntered[0] == 'a') || (stringEntered[0] == 'A'))
                {
                  printf("slotNumber = ALL\n");
                  allSelected = true;
                }
                else
                {
                  slotNumber = atoi(stringEntered);
                  printf("slotNumber = %d\n", slotNumber);
                  SlotNumber sn = static_cast<SlotNumber>(slotNumber);
                  sc.set_slot_number(sn);
                  allSelected = false;
                }
                     break;
            case '3':
               {
                  sc.set_command(SYSTEM_COMMAND_UPDATE_SLOT_TEMP_SETPOINT);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  if (allSelected)
                  {
                     for (int i = 0; i < TOTAL_SLOTS; i++)
                     {
                        SlotNumber sn = static_cast<SlotNumber>(i+1);
                        sc.set_slot_number(sn);
                        sendCommand(sc);
                     }
                  }
                  else
                  {
                     sendCommand(sc);
                  }
               }
                    break;
            case '4':
                printf("Enter new Upper setpoint (165 - 200) (currently %d): ", upperSetpoint);
                scanf("%s", stringEntered);
                upperSetpoint = atoi(stringEntered);
                printf("New Upper Setpoint = %d\n", upperSetpoint);
                sc.set_heater_location_upper_setpoint_temperature(upperSetpoint);
                     break;
            case '5':
                printf("Enter new Lower setpoint (165 - 200) (currently %d): ", lowerSetpoint);
                scanf("%s", stringEntered);
                lowerSetpoint = atoi(stringEntered);
                printf("New Lower Setpoint = %d\n", lowerSetpoint);
                sc.set_heater_location_lower_setpoint_temperature(lowerSetpoint);
                     break;
            case '6':
               {
                  sc.set_command(SYSTEM_COMMAND_SET_HEATER_TEMP_SETPOINT);
                  ::google::protobuf::Timestamp* startTime = new ::google::protobuf::Timestamp();
                  ::google::protobuf::Timestamp* endTime = new ::google::protobuf::Timestamp();
                  gettimeofday(&now, NULL);
                  startTime->set_seconds(now.tv_sec);
                  startTime->set_nanos(0);
                  sc.set_allocated_start_time(startTime);
                  endTime->set_seconds(now.tv_sec + 1800);
                  endTime->set_nanos(0);
                  sc.set_allocated_end_time(endTime);
                  if (allSelected)
                  {
                     for (int i = 0; i < TOTAL_SLOTS; i++)
                     {
                        SlotNumber sn = static_cast<SlotNumber>(i+1);
                        sc.set_slot_number(sn);
                        sendCommand(sc);
                     }
                  }
                  else
                  {
                     sendCommand(sc);
                  }
               }
                    break;
            case 'x':
//					 printf("Return to main menu\n");
                     return;
            default :
                     //User enters wrong input
                     //TO DO CODE
                     break;
        }
   }
}

void *mainMenuThread(void*)
{
   while(1)
   {
   }
   pthread_exit(NULL);
}





/*******************************************************************************************/
/*                                                                                         */
/* int main(int argc, char *argv[])                                                        */
/*                                                                                         */
/* Call the initialization functions and start the main menu          .                    */
/* Exits when the user selects x from the main menu.                                       */
/*                                                                                         */
/*******************************************************************************************/
int main(int argc, char *argv[])
{
    //Variable used for reading the user input
    char option;
    //Variable used for controlling the while loop
    bool isRunning = true;
//	strcpy(controllerIPAddress, "192.168.1.200");
//	strcpy(myIPAddress, "192.168.1.123");
    memset(controllerIPAddress, 0, sizeof(controllerIPAddress));
    memset(myIPAddress, 0, sizeof(myIPAddress));
   memset(guiNumber, 0, sizeof(guiNumber));

    int i;
    if (4 == argc)
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
        printf("Usage frontierCmds <controllerIPAddress> <ThisPCsIPAddress> <GUI 1 or 2>\n");
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

   // start task threads
//	createThreads();
   

    while(isRunning==true)
    {
        //Clears the screen
        system("clear");        //For UNIX-based OSes

        //Clears the keyboard buffer
        fflush(stdin);
        //Outputs the options to console
      printf("MAIN MENU\n");
      printf("======================================================\n");
      printf("[1]STARTUP / ESTOP / SHUTDOWN / IDLE command menu\n");
      printf("[2]Change setpoint menu\n");
      printf("[3]Heater command menu\n");
      printf("[4]Fan command menu\n");
      printf("[5]TimeSync command menu\n");
      printf("[6]Cleaning mode command menu\n");
      printf("[7]NSO mode command menu\n");
      printf("[8]Demo mode command menu\n");
      printf("[9]Configure logging menu\n");
      printf("[x]Exit\n");
        //Reads the user's option
//		scanf("%s", &option)
        option = getchar();
        //Selects the course of action specified by the option
        switch(option)
        {
            case '1':
               startupCommandMenu();
               break;
            case '2':
               changeSetpointMenu();
               break;
            case '3':
               heaterCommandMenu();
               break;
            case '4':
               fanCommandMenu();
               break;
            case '5':
               timeSyncCommandMenu();
               break;
            case '6':
               cleaningModeCommandMenu();
               break;
            case '7':
               nsoModeCommandMenu();
               break;
            case '8':
               demoModeCommandMenu();
               break;
            case '9':
               configureLoggingMenu();
               break;
            case 'x':
               //Exits the system
               isRunning = false;
               return 0;
            default :
               //User enters wrong input
               //TO DO CODE
               break;
        }
    }
   exitProgram();
    return 0;
}
