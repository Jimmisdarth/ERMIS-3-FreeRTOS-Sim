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
#define MAX_PARAMETERS 6
#define SUBSYSTEM_STATES_RETURN_PARAMETERS 5
#define SESSION_INFORAMTION_RETURN_PARAMETERS 2
#define IMAGE_CAPTURE_TIME pdMS_TO_TICKS( 1000 )
#define MONITOR_IMAGE_CAPTURE_PERIOD pdMS_TO_TICKS ( 500 )
#define MAX_WAIT_TIME_FOR_IMAGE_CAPTURE_COMPLETION pdMS_TO_TICKS( 2000 )

// Struct for I2C transfers of HyperSpectral Camera
typedef struct I2C_Payload {
	int Command_ID;
	int Parameter[MAX_PARAMETERS];
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
int  cameraActivateSession(int mode);
void cameraEnableSensor();
void cameraDisableSensor();
void cameraCaptureImage();
void cameraCloseSession();

void cameraSetAndConfirmImagingParameter(int parameter, int value);

// HYPERSPECTRAL CAMERA COMMANDS
void OPEN_SESSION();
void CONFIGURE(int mode);
void ACTIVATE_SESSION(int mode);
void ENABLE_SENSOR();
void CAPTURE_IMAGE();
void DISABLE_SENSOR();
void CLOSE_SESSION();
void SET_IMAGING_PARAMETER(int param, int val);
void GET_IMAGING_PARAMETER(int param);
void STORE_TIME_SYNC();
void STORE_USER_DATA(int packet_id, int length, int user_data);

// HYPERSPECTRAL CAMERA REQUESTS
void SUBSYSTEM_STATES(int states[]);
void SESSION_INFORMATION(int states[]);
int  IMAGING_PARAMETER();
int  CURRENT_SESSION_ID();
int  CURRENT_SESSION_SIZE();

// HELPER FOR REQUEST
void printSubSystemStates(const int states[]);
void printSessionInforamtion(const int states[]);


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
	for (int i = 0; i < MAX_PARAMETERS; ++i)
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
	int session_size = -100;

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
			printf("\nOBC power commands:");
			printf("\n\tEnter EXIT to close the OBC.");
			printf("\n");
			printf("\nRequired commands for image capturing:");
			printf("\n\tEnter Open_Session to open a camera session.");
			printf("\n\tEnter Configure to configure the currently open session of the camera.");
			printf("\n\tEnter Activate_Session to activate the currently open session of the camera.");
			printf("\n\tEnter Enable_Sensor to enable the sensor of the camera.");
			printf("\n\tEnter Disable_Sensor to disable the sensor of the camera.");
			printf("\n\tEnter Capture_Image to start the image captuting.");
			printf("\n\tEnter Close_Session to close the session of the camera.");
			printf("\n");
			printf("\nOptional commands for image capturing:");
			printf("\n\tEnter Set_Imaging_Parameter to set and confirm an imaging parameter of the camera.");
			printf("\n\tEnter Store_Time_Sync to store time sync.");
			printf("\n\tEnter Store_User_Data to store the user data.");
			printf("\n");
			resetTextColor();
		}
		// CAMERA REQUIRED COMMANDS FOR IMAGE CAPTURE
		if (strcmp(command_name, "Open_Session\n") == 0) {
			hyperspectral_camera_session_id = cameraOpenSession();
			setBlueTextColor();
			printf("Received Camera Session ID : %d\n", hyperspectral_camera_session_id);
			resetTextColor();
		}
		if (strcmp(command_name, "Configure\n") == 0) {
			cameraConfig(1);	// Configure camera for line scan
		}
		if (strcmp(command_name, "Activate_Session\n") == 0) {
			session_size = cameraActivateSession(1);	// Activate the currently open session with automatic mode
		}
		if (strcmp(command_name, "Enable_Sensor\n") == 0) {
			cameraEnableSensor();
		}
		if (strcmp(command_name, "Disable_Sensor\n") == 0) {
			cameraDisableSensor();
		}
		if (strcmp(command_name, "Capture_Image\n") == 0) {
			cameraCaptureImage();
		}
		if (strcmp(command_name, "Close_Session\n") == 0) {
			cameraCloseSession();
		}
		// CAMERA OPTIONAL COMMANDS FOR IMAGE CAPTURE
		if (strcmp(command_name, "Set_Imaging_Parameter\n") == 0) {
			cameraSetAndConfirmImagingParameter(0, 10);
		}
		if (strcmp(command_name, "Store_Time_Sync\n") == 0) {
			STORE_TIME_SYNC();
		}
		if (strcmp(command_name, "Store_User_Data\n") == 0) {
			STORE_USER_DATA(2, 10, 324);
		}
	}
}

/*
* 
* Camera Required Image Capture Commands
* 
*/

int cameraOpenSession() {
	int session_id = -1;

	int states[5];

	OPEN_SESSION();

	session_id = CURRENT_SESSION_ID();

	SUBSYSTEM_STATES(states);

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

	SUBSYSTEM_STATES(states);

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

int cameraActivateSession(int mode) {
	int states[5];
	int session_size = -100;

	ACTIVATE_SESSION(mode);

	SUBSYSTEM_STATES(states);

	session_size = CURRENT_SESSION_SIZE();

	if (states[0] == 2) {
		setBlueTextColor();
		printf("Camera session activated, current session size : %d\n", session_size);
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't activate camera's session\n");
		resetTextColor();
	}

	return session_size;
}

void cameraEnableSensor() {
	int states[5];

	ENABLE_SENSOR();

	SUBSYSTEM_STATES(states);

	if (states[2] == 1) {
		setBlueTextColor();
		printf("Camera's sensor enabled successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't enable the sensor of the camera\n");
		resetTextColor();
	}
}

void cameraDisableSensor() {
	int states[5];

	DISABLE_SENSOR();

	SUBSYSTEM_STATES(states);

	if (states[2] == 0) {
		setBlueTextColor();
		printf("Camera's sensor disabled successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't disable the sensor of the camera\n");
		resetTextColor();
	}
}

void cameraCaptureImage() {
	int states[5];

	TickType_t starting_tick_time;
	TickType_t current_tick_time;
	TickType_t time_passed;

	CAPTURE_IMAGE();
	starting_tick_time = xTaskGetTickCount();

	SUBSYSTEM_STATES(states);

	while (states[3] == 2) {
		current_tick_time = xTaskGetTickCount();
		time_passed = current_tick_time - starting_tick_time;

		if (time_passed % MONITOR_IMAGE_CAPTURE_PERIOD == 0)
			SUBSYSTEM_STATES(states);
		if (time_passed == MAX_WAIT_TIME_FOR_IMAGE_CAPTURE_COMPLETION)
			break;
	}

	if (states[3] == 0) {
		setBlueTextColor();
		printf("Camera captured image successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't capture the image\n");
		resetTextColor();
	}
}

void cameraCloseSession() {
	int states[3];

	CLOSE_SESSION();

	SESSION_INFORMATION(states);

	if (states[0] == 0) {
		setBlueTextColor();
		printf("Camera's session closed successfully\n");
		resetTextColor();
	}
	else {
		setRedTextColor();
		printf("Couldn't close the session of the camera\n");
		resetTextColor();
	}
}

/*
*
* Camera Optional Image Capture Commands
*
*/

void cameraSetAndConfirmImagingParameter(int parameter, int value) {
	int imaging_parameter = -1;

	SET_IMAGING_PARAMETER(parameter, value);

	GET_IMAGING_PARAMETER(parameter);

	imaging_parameter = IMAGING_PARAMETER();

	setBlueTextColor();
	printf("Imaging parameter %d of camera has value : %d\n", parameter, imaging_parameter);
	resetTextColor();
}

/*
* 
* HYPERSPECTRAL CAMERA TASK
* 
*/

void HyperSpectralCamera(void) {

	// IDENTIFIERS AND MODES OF THE CAMERA
	int session_id = -1;

	int scan_mode = -1;	// 1 -> line scan, 3 -> line scan test pattern, 5 -> line scan high accuracy mode. 

	int storage_mode = -1; // 0 -> Manual mode (should never be used), 1 -> Automatic mode.
	int session_size = 0; // In MegaBytes.

	int time_sync = 0;

	// IMAGING PARAMETERS
	int imaging_index = 0;
	int imaging_parameters[2] = { 2, 4 }; // lines, frame interval

	// USER DATA
	int packet_id = -1;
	int length = -1;
	int user_data = -1;

	// SUBSYSTEM STATES RESPONSE 
	int session_state = 0; // 2 Bits
	int config_state = 0; // 1 Bit
	int sensor_state = 0; // 1 Bit
	int capture_state = 0; // 2 Bits
	int read_out_state = 0; // 1 Bit

	// SESSION INFORAMTION RESPONSE 
	int session_close_error = 0; // 1 Bits
	int storage_error = 0;       // 1 Bit

	// COMMAND AND REQUESTS INFORMATION
	I2C_Payload rx_payload;
	I2C_Payload tx_payload;

	int command_id;
	int received_command;

	// REPRESENTATION OF THE STORED IMAGE DATA
	int stored_image_data[5] = {0, 0, 0, 0, 0};

	// TIME INFORMATION FOR IMAGE CAPTURE SIMULATION
	TickType_t image_capture_start_tick_time;
	TickType_t current_tick_time; 
	TickType_t time_passed;

	for (;;) {
		received_command = xQueueReceive(I2C_CAMERA, &rx_payload, portMAX_DELAY);
		if (received_command) {
			command_id = rx_payload.Command_ID;

			setGreenTextColor();

			// COMMANDS
			if (command_id == 0) {		// 0x00 OPEN SESSION
				++session_id;
				session_state = 1;
				printf("HyperSpectral Camera Opening Session %d ...\n", session_id);
			}
			if (command_id == 1 && session_state == 1 && config_state == 1) {		// 0x01 ACTIVATE SESSION
				storage_mode = rx_payload.Parameter[0];
				session_size = 1024;
				session_state = 2;
				printf("HyperSpectral Camera activating current open Session with ID: %d, storage mode : %d ", session_id, storage_mode);
				printf("and reserving %d MB in Flash Memory\n", session_size);
			}
			if (command_id == 2) {		// 0x02 CLOSE SESSION
				session_state  = 0;
				config_state   = 0;
				sensor_state   = 0;
				capture_state  = 0;
				read_out_state = 0;

				session_close_error = 0;
				printf("HyperSpectral Camera closing the session with ID: %d\n", session_id);
			}
			if (command_id == 5) {		// 0x05 STORE TIME SYNC
				time_sync = 1;
				printf("\nHyperSpectral Camera set time sync as true\n");
			}
			if (command_id == 6) {		// 0x06 STORE USER DATA
				packet_id = rx_payload.Parameter[0];
				length = rx_payload.Parameter[1];
				user_data = rx_payload.Parameter[2];

				printf("\nHyperSpectral Camera storing user data:");
				printf("\nPacket ID : %d", packet_id);
				printf("\nLength    : %d", length);
				printf("\nUser Data : %d", user_data);
				printf("\n");
			}
			if (command_id == 32) {		// 0x20 ENALBE SENSOR
				sensor_state = 1;
				printf("HyperSpectral Camera Enabling Sensor\n");
			}
			if (command_id == 33) {		// 0x21 DISABLE SENSOR
				sensor_state = 0;
				printf("HyperSpectral Camera Disabling the Sensor\n");
			}
			if (command_id == 34) {		// 0x22 SET IMAGING PARAMETER
				int index = rx_payload.Parameter[0];
				int value = rx_payload.Parameter[1];

				config_state = 0;

				imaging_parameters[index] = value;

				printf("HyperSpectral Camera setting imaging parameter %d, with value : %d\n", index, value);
			}
			if (command_id == 36) {		// 0x24 GET IMAGING PARAMETER
				imaging_index = rx_payload.Parameter[0];
				printf("HyperSpectral Camera has imaging index : %d\n", imaging_index);
			}
			if (command_id == 38 && session_state == 1) {		// 0x26 CONFIGURE
				scan_mode = rx_payload.Parameter[0];
				config_state = 1;
				printf("HyperSpectral Camera using scan mode : %d\n", scan_mode);
			}
			if (command_id == 39 && session_state == 2 && config_state == 1 && sensor_state == 1) {		// 0x27 CAPTURE IMAGE
				capture_state = 2;
				image_capture_start_tick_time = xTaskGetTickCount();
				printf("HyperSpectral Camera starting image capture\n");
			}
			// REQUESTS
			if (command_id == 129) {	// 0x81 SUBSYSTEMS STATES
				tx_payload.Command_ID = 129;
				tx_payload.Parameter[0] = session_state;
				tx_payload.Parameter[1] = config_state;
				tx_payload.Parameter[2] = sensor_state;
				tx_payload.Parameter[3] = capture_state;
				tx_payload.Parameter[4] = read_out_state;

				printf("About to send SubSystem States response to OBC\n");
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 133) {	// 0x85 SESSION INFORMATION
				tx_payload.Command_ID = 133;
				tx_payload.Parameter[0] = session_close_error;
				tx_payload.Parameter[1] = storage_error;

				printf("About to send Session Information response to OBC\n");
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 134) {	// 0x86 CURRENT SESSION ID
				tx_payload.Command_ID = 134;
				tx_payload.Parameter[0] = session_id;
				printf("About to send Session_ID : %d to OBC\n", session_id);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 135) {	// 0x87 CURRENT SESSION SIZE
				tx_payload.Command_ID = 135;
				tx_payload.Parameter[0] = session_size;
				printf("About to send  current Session Size : %d to OBC\n", session_size);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 137) {	// 0x89 IMAGING PARAMETER
				tx_payload.Command_ID = 137;
				tx_payload.Parameter[0] = imaging_parameters[imaging_index];
				printf("About to send imaging parameter value : %d to OBC\n", imaging_parameters[imaging_index]);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
		}

		// Simulate image capture. 
		// The imager will count for a predifined number of ticks after which we consider that the image is captured.
		if (capture_state == 2) {
			setGreenTextColor();

			current_tick_time = xTaskGetTickCount();
			time_passed = current_tick_time - image_capture_start_tick_time;

			if (time_passed == IMAGE_CAPTURE_TIME) {
				capture_state = 0;
				stored_image_data[session_id] = 2930;
				printf("Image Capture complete for session with ID : %d, stored image data : %d\n", session_id, stored_image_data[session_id]);
			}
		}

		resetTextColor();
	}
}

/*
* 
* HYPESPECTRAL CAMERA COMMAND TRANSACTIONS
* 
*/

void OPEN_SESSION() {
	I2C_Payload OPEN_SESSION;
	OPEN_SESSION.Command_ID = 0;

	printCommandID("OPEN SESSION", OPEN_SESSION.Command_ID);

	xQueueSend(I2C_CAMERA, &OPEN_SESSION, portMAX_DELAY);
}

void CLOSE_SESSION() {
	I2C_Payload payload;
	payload.Command_ID = 2;

	printCommandID("CLOSE SESSION", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void CONFIGURE(int mode){
	I2C_Payload payload;
	payload.Command_ID = 38;
	payload.Parameter[0] = mode;

	printCommandID("CONFIGURE", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void ACTIVATE_SESSION(int mode) {
	I2C_Payload payload;
	payload.Command_ID = 1;
	payload.Parameter[0] = mode;

	printCommandID("ACTIVATE SESSION", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void ENABLE_SENSOR() {
	I2C_Payload payload;
	payload.Command_ID = 32;

	printCommandID("ENABLE SENSOR", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void DISABLE_SENSOR() {
	I2C_Payload payload;
	payload.Command_ID = 33;

	printCommandID("DISABLE SENSOR", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void CAPTURE_IMAGE() {
	I2C_Payload payload;
	payload.Command_ID = 39;

	printCommandID("CAPTURE IMAGE", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void STORE_TIME_SYNC() {
	I2C_Payload payload;
	payload.Command_ID = 5;

	printCommandID("STORE TIME SYNC", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void STORE_USER_DATA(int packet_id, int length, int user_data) {
	I2C_Payload payload;
	payload.Command_ID = 6;
	payload.Parameter[0] = packet_id;
	payload.Parameter[1] = length;
	payload.Parameter[2] = user_data;

	printCommandID("STORE USER DATA", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

int CURRENT_SESSION_ID() {
	int session_id = -100;

	I2C_Payload CURRENT_SESSION_ID;
	CURRENT_SESSION_ID.Command_ID = 134;
	printCommandID("CURRENT SESSION ID", CURRENT_SESSION_ID.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &CURRENT_SESSION_ID, portMAX_DELAY)) {
		printf("\nOBC FAILED TO SEND COMMAND x%x TO THE HYPERSPECTRAL CAMERA\n", CURRENT_SESSION_ID.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &CURRENT_SESSION_ID, portMAX_DELAY))
			session_id = CURRENT_SESSION_ID.Parameter[0];
	}

	return session_id;
}

int CURRENT_SESSION_SIZE() {
	int session_size = -100;

	I2C_Payload payload;
	payload.Command_ID = 135;
	printCommandID("CURRENT SESSION SIZE", payload.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("\nOBC FAILED TO SEND COMMAND x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY))
			session_size = payload.Parameter[0];
	}

	return session_size;
}

void SET_IMAGING_PARAMETER(int param, int val) {
	I2C_Payload payload;
	payload.Command_ID = 34;
	payload.Parameter[0] = param;
	payload.Parameter[1] = val;

	printCommandID("SET IMAGING PARAMETER", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void GET_IMAGING_PARAMETER(int param) {
	I2C_Payload payload;
	payload.Command_ID = 36;
	payload.Parameter[0] = param;

	printCommandID("GET IMAGING PARAMETER", payload.Command_ID);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

int  IMAGING_PARAMETER() {
	int imaging_parameter = -1;

	I2C_Payload payload;
	payload.Command_ID = 137;	// 0x89
	printCommandID("IMAGING PARAMETER", payload.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the value of the imaging parameter that the hyperspectral camera returned.
			imaging_parameter = payload.Parameter[0];
		}
	}

	return imaging_parameter;
}

void SUBSYSTEM_STATES(int states[]) {

	I2C_Payload payload;
	payload.Command_ID = 129;	// 0x81
	printCommandID("SUBSYSTEMS STATES" ,payload.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the states that the hyperspectral camera returned.
			for(int i = 0; i < SUBSYSTEM_STATES_RETURN_PARAMETERS; ++i)
				states[i] = payload.Parameter[i];

			printSubSystemStates(states);
		}
	}
}

void SESSION_INFORMATION(int states[]) {

	I2C_Payload payload;
	payload.Command_ID = 133;	// 0x85
	printCommandID("SESSION INFORMATION", payload.Command_ID);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the states that the hyperspectral camera returned.
			for (int i = 0; i < SESSION_INFORAMTION_RETURN_PARAMETERS; ++i)
				states[i] = payload.Parameter[i];

			printSessionInforamtion(states);
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

void printSessionInforamtion(const int states[]) {
	char* session_close_error = "";
	char* storage_error = "";

	// SESSION CLOSE ERROR
	if (states[0] == 0)
		session_close_error = _strdup("No Error.");
	else if (states[0] == 1)
		session_close_error = _strdup("Error. Session was not explicitly closed, and session size may be incorrect.");
	else
		session_close_error = _strdup("Unkwown");

	// STORAGE ERROR
	if (states[1] == 0)
		storage_error = _strdup("No Error.");
	else if (states[1] == 1)
		storage_error = _strdup(" Error. Flash ECC errors were detected during read out. A flash block may be bad.");
	else
		storage_error = _strdup("Unkwown");

	setYellowTextColor();
	printf("Received response from the camera, printing the states:\n");
	printf("Closed Session State : %s\n", session_close_error);
	printf("Storage State        : %s\n", storage_error);
	resetTextColor();
}


