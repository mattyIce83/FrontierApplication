/***************************************************************************************/
/*                                                                                     */
/* uhc_proto.h - Defines for Henny Penny UHC (Frontier project) system messages to be  */
/* published by either the Controller or the Intelligent Glass modules                 */
/*                                                                                     */
/***************************************************************************************/



/***************************************************************************************/
/*                                                                                     */
/*  The Frontier project uses Google Protocol Buffers to define the messages used      */
/*  by the Controller and Intelligent Glass modules. ZeroMQ is used as the             */
/*  transport mechanism for all of these system messages. Specifically, ZeroMQ's       */
/*  Publish/Subscribe model is used to exchange messages between these devices.        */
/*                                                                                     */
/*  For more information on Google Protocol Buffers, see:                              */
/*  https://protobuf.dev/getting-started/cpptutorial/                                  */
/*                                                                                     */
/*  For more information on ZeroMQ, see:                                               */
/*  https://zguide.zeromq.org/                                                         */
/*                                                                                     */
/*  For this project, the controller will have a static IP address of 192.168.1.200    */
/*  All messages published by the controller will publish its ZeroMQ messages on port  */
/*  numbers that end with a zero, such as 5000, 5010, etc.                             */
/*                                                                                     */
/*  Intelligent Glass 1 (front) will have a static IP address of 192.168.1.201, and    */
/*  will publish its ZeroMQ messages on port numbers that end with a one, such as      */
/*  5001, 5011, etc.                                                                   */
/*                                                                                     */
/*  Intelligent Glass 2 (rear) will have a static IP address of 192.168.1.202, and     */
/*  will publish its ZeroMQ messages on port numbers that end with a one, such as      */
/*  5002, 5012, etc.                                                                   */
/*                                                                                     */
/*  The tens digit of the port number will correspond to the ZeroMQ message that       */
/*  was originally sent. For example, the FirmwareUpdate message is sent on port       */
/*  5041, and the response will be sent on port 5040. This aids in debugging if        */
/*  a tool such as Wireshark is used to capture packet traces.                         */
/*                                                                                     */
/*  One deviation is that port 5023 is used by the controller to respond to system     */
/*  commands from GUI2. ZeroMQ doesn't seem to support multiple transmitters using     */
/*  the same port.                                                                     */
/*                                                                                     */
/***************************************************************************************/


#define CURRENT_SYSTEM_STATE_TOPIC					"CSS"
#define CURRENT_SYSTEM_STATE_PORT_CONTROLLER		5000

#define HEARTBEAT_TOPIC								"HB"
#define HEARTBEAT_PORT_GUI1							5011
#define HEARTBEAT_PORT_GUI2							5012

#define SYSTEM_COMMAND_TOPIC						"CMD"
#define SYSTEM_COMMAND_RESPONSE_TOPIC				"RSP"
#define SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER		5020
#define SYSTEM_COMMAND_PORT_GUI1					5021
#define SYSTEM_COMMAND_PORT_GUI2					5022
#define SYSTEM_COMMAND_RESPONSE_PORT_CONTROLLER2	5023

#define TIME_SYNC_TOPIC								"TIME"
#define TIME_SYNC_PORT_GUI1							5031
#define TIME_SYNC_PORT_GUI2							5032

#define FIRMWARE_UPDATE_TOPIC						"FWUP"
#define FIRMWARE_UPDATE_RESPONSE_TOPIC				"FWRS"
#define FIRMWARE_UPDATE_RESULT_PORT_CONTROLLER		5040
#define FIRMWARE_UPDATE_PORT_GUI1					5041

#define RTD_DATA_PUBLISHER_TOPIC                "RTD"
#define RTD_DATA_PUBLISHER_PORT                 5050

#define SYNC_BACKEND_SUB_PORT                   5111
#define SYNC_BACKEND_PUB_PORT                   5211

#define UI_TOPIC_GUI2_TO_GUI1                   "G2RQ" // gui2->gui1, gui2 request topic
#define UI_TOPIC_GUI1_TO_GUI2                   "G1RS" // gui1->gui2, gui1 response topic

