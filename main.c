// HEADER FILES
#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

// DEFINITIONS
#define MAXPARAMETERS 6
#define SUBSYSTEMSTATESRETURNPARAMETERS 5

// Struct for I2C transfers of HyperSpectral Camera
typedef struct I2C_Payload {
	int Command_ID;
	int Parameter[MAXPARAMETERS];
} I2C_Payload;

// TASK FUNCTIONS
void OBC(void);
void HyperSpectralCamera(void);

// STRUCT FUNCTIONS
void print_I2C_payload(const I2C_Payload p);
void printCommandID(const char* command_name, int command_id);

// HELPER FUNCTIONS
void setGreenTextColor()  { printf("\033[0;32m"); }
void setRedTextColor()    { printf("\033[0;31m"); }
void setBlueTextColor()   { printf("\033[0;34m"); }
void setYellowTextColor() { printf("\033[1;33m"); }
void resetTextColor()     { printf("\033[0m"); }


// OBC COMMANDS
int  cameraOpenSession();
void cameraConfig(int mode);

// HYPERSPECTRAL CAMERA COMMANDS
void OPEN_SESSION_COMMAND();
int  CURRENT_SESSION_ID_COMMAND();
void CONFIGURE(int mode);

// HYPERSPECTRAL CAMERA REQUESTS
void SUBSYSTEM_STATES_COMMAND(int states[]);
void printSubSystemStates(const int states[]);


// TASK HANDLERS
TaskHandle_t TASK_1 = NULL;
TaskHandle_t TASK_2 = NULL;

// QUEUE HANDLES
xQueueHandle I2C_OBC    = 0;
xQueueHandle I2C_CAMERA = 0;

// MAIN FUNCTION
int main(void) {

	// CREATE THE QUEUE OF SIZE 1
	I2C_OBC    = xQueueCreate(5, sizeof(I2C_Payload));
	I2C_CAMERA = xQueueCreate(5, sizeof(I2C_Payload));

	// TASK CREATION
	xTaskCreate(OBC, "Tx_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL); //tskIDLE_PRIORITY
	xTaskCreate(HyperSpectralCamera, "Rx_Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL);

	vTaskStartScheduler();

	for (;;);
	return 0;
}

void print_I2C_payload(const I2C_Payload p) {
	printf("Command ID : 0x%X\n", p.Command_ID);
	for (int i = 0; i < MAXPARAMETERS; ++i)
		printf("Parameter %d = %d\n", i, p.Parameter[i]);
}

void printCommandID(const char* command_name, int command_id) {
	setYellowTextColor();
	printf("Sending %s Command (0x%x) to camera\n", command_name, command_id);
	resetTextColor();
}

// TASKS
void OBC(void) {
	int hyperspectral_camera_session_id = -100;

	char command_name[64];

	printf("On Board Computer (OBC) STARTING...\n");
	printf("Type help to see the available commands.\n");

	for (;;) {
		printf("Type a command for OBC to execute : ");
		fgets(command_name, sizeof(command_name), stdin);


		if (strcmp(command_name, "EXIT\n") == 0) {
			setBlueTextColor();
			printf("OBC TURING OFF...\n");
			resetTextColor();
			exit(0);
		}
		if (strcmp(command_name, "help\n") == 0) {
			setBlueTextColor();
			printf("Enter EXIT to close the OBC.");
			printf("\nEnter Open_Session to open a camera session.");
			printf("\nEnter Configure to configure the currently open session of the camera.\n");
			resetTextColor();
		}
		if (strcmp(command_name, "Open_Session\n") == 0) {
			hyperspectral_camera_session_id = cameraOpenSession();
			setBlueTextColor();
			printf("Received Camera Session ID : %d\n", hyperspectral_camera_session_id);
			resetTextColor();
		}
		if (strcmp(command_name, "Configure\n") == 0) {
			cameraConfig(1);	// Configure camera for line scan
		}
	}
}

int cameraOpenSession() {
	int session_id = -1;

	int states[5];

	OPEN_SESSION_COMMAND();

	session_id = CURRENT_SESSION_ID_COMMAND();

	SUBSYSTEM_STATES_COMMAND(states);

	if (states[0] == 1) {
		setBlueTextColor();
		printf("Camera session opened successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Camera couldn't open the session\n");
		resetTextColor();
	}

	return session_id;
}

void cameraConfig(int mode) {
	int states[5];

	CONFIGURE(mode);

	SUBSYSTEM_STATES_COMMAND(states);

	if (states[1] == 1) {
		setBlueTextColor();
		printf("Camera configured successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't configure camera\n");
		resetTextColor();
	}
}

/*
* 
* HYPERSPECTRAL CAMERA TASK
* 
*/

void HyperSpectralCamera(void) {

	int session_id = -1;

	int scan_mode = -1;	// 1 -> line scan, 3 -> line scan test pattern, 5 -> line scan high accuracy mode 

	// SUBSYSTEM STATES RESPONSE 
	int Session_State = 0; // 2 Bits
	int Config_State = 0; // 1 Bit
	int Sensor_State = 0; // 1 Bit
	int Capture_State = 0; // 2 Bits
	int Read_Out_State = 0; // 1 Bit

	I2C_Payload rx_payload;
	I2C_Payload tx_payload;

	int command_id;
	int received_command;

	for (;;) {
		received_command = xQueueReceive(I2C_CAMERA, &rx_payload, portMAX_DELAY);
		if (received_command) {
			command_id = rx_payload.Command_ID;

			setGreenTextColor();

			// COMMANDS
			if (command_id == 0) {		// 0x00 OPEN SESSION
				++session_id;
				Session_State = 1;
				printf("HyperSpectral Camera Opening Session %d ...\n", session_id);
			}
			if (command_id == 38 && Session_State == 1) {		// 0x26 CONFIGURE
				scan_mode = rx_payload.Parameter[0];
				Config_State = 1;
				printf("HyperSpectral Camera using scan mode : %d\n", scan_mode);
			}
			// REQUESTS
			if (command_id == 129) {	// 0x81 SUBSYSTEMS STATES
				tx_payload.Command_ID = 129;
				tx_payload.Parameter[0] = Session_State;
				tx_payload.Parameter[1] = Config_State;
				tx_payload.Parameter[2] = Sensor_State;
				tx_payload.Parameter[3] = Capture_State;
				tx_payload.Parameter[4] = Read_Out_State;

				printf("About to send SubSystem States response to OBC\n");
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 134) {	// 0x86 CURRENT SESSION ID
				tx_payload.Command_ID = 134;
				tx_payload.Parameter[0] = session_id;
				printf("About to send Session_ID : %d to OBC\n", session_id);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}

			resetTextColor();
		}
	}
}

/*
* 
* HYPESPECTRAL CAMERA COMMAND TRANSACTIONS
* 
*/

void OPEN_SESSION_COMMAND() {
	I2C_Payload OPEN_SESSION;
	OPEN_SESSION.Command_ID = 0;

	printCommandID("OPEN SESSION", OPEN_SESSION.Command_ID);

	xQueueSend(I2C_CAMERA, &OPEN_SESSION, portMAX_DELAY);
}

int CURRENT_SESSION_ID_COMMAND() {
	int session_id = -100;

	I2C_Payload CURRENT_SESSION_ID;
	CURRENT_SESSION_ID.Command_ID = 134;
	printCommandID("CURRENT SESSION ID", CURRENT_SESSION_ID.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &CURRENT_SESSION_ID, portMAX_DELAY)) {
		printf("\nOBC FAILED TO SEND COMMAND x%x TO THE HYPERSPECTRAL CAMERA\n", CURRENT_SESSION_ID.Command_ID);
		return NULL;
	}
	else {
		if (xQueueReceive(I2C_OBC, &CURRENT_SESSION_ID, portMAX_DELAY))
			session_id = CURRENT_SESSION_ID.Parameter[0];
	}

	return session_id;
}

void CONFIGURE(int mode){
	I2C_Payload payload;
	payload.Command_ID = 38;
	payload.Parameter[0] = mode;

	printCommandID("CONFIGURE", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void SUBSYSTEM_STATES_COMMAND(int states[]) {

	I2C_Payload payload;
	payload.Command_ID = 129;	// 0x81
	printCommandID("SUBSYSTEMS STATES" ,payload.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
		return NULL;
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the states that the hyperspectral camera returned.
			for(int i = 0; i < SUBSYSTEMSTATESRETURNPARAMETERS; ++i)
				states[i] = payload.Parameter[i];

			printSubSystemStates(states);
		}
	}
}

void printSubSystemStates(const int states[]) {
	char* session = "";
	char* config  = "";
	char* sensor  = "";
	char* capture = "";
	char* readout = "";

	// SESSION STATE
	if (states[0] == 0)
		session = _strdup("Closed");
	else if (states[0] == 1)
		session = _strdup("Open");
	else if (states[0] == 2)
		session = _strdup("Active (Ready)");
	else if (states[0] == 3)
		session = _strdup("Reserved");
	else
		session = _strdup("Unkwown");

	// CONFIG STATE
	if (states[1] == 0)
		config = _strdup("Unconfigured");
	else if (states[1] == 1)
		config = _strdup("Successfully Configured");
	else
		config = _strdup("Unkwown");

	// SENSOR STATE
	if (states[2] == 0)
		sensor = _strdup("Disabled (Powered Off)");
	else if (states[2] == 1)
		sensor = _strdup("Enabled (Powered On)");
	else
		sensor = _strdup("Unkwown");

	// CAPTURE STATE
	if (states[3] == 0)
		capture = _strdup("Idle");
	else if (states[3] == 1)
		capture = _strdup("Waiting (For PPS)");
	else if (states[3] == 2)
		capture = _strdup("Busy");
	else if (states[3] == 3)
		capture = _strdup("Reserved");
	else
		capture = _strdup("Unkwown");

	// READOUT STATE
	if (states[4] == 0)
		readout = _strdup("Idle");
	else if (states[4] == 1)
		readout = _strdup("Busy");
	else
		readout = _strdup("Unkwown");

	setYellowTextColor();
	printf("Received response from the camera, printing the states:\n");
	printf("Session  State : %s\n", session);
	printf("Config   State : %s\n", config);
	printf("Sensor   State : %s\n", sensor);
	printf("Capture  State : %s\n", capture);
	printf("Read Out State : %s\n", readout);
	resetTextColor();
}


