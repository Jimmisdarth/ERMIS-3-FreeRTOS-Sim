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

#define MAX_NUMBER_OF_SESSIONS 5
#define MAX_NUMBER_OF_LINES 5

#define SUBSYSTEM_STATES_RETURN_PARAMETERS 5
#define SESSION_INFORAMTION_RETURN_PARAMETERS 4

#define IMAGE_CAPTURE_TIME  pdMS_TO_TICKS( 1000 )
#define IMAGE_READ_OUT_TIME pdMS_TO_TICKS( 1000 )

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
void PDPU(void);
void Laser(void);

// STRUCT FUNCTIONS
void print_I2C_payload(const I2C_Payload p);
void printCommandID(const char* command_name, int command_id, int color);

// HELPER FUNCTIONS TO PRINT COLORED TEXT USING ANSI COLOR CODES
static void setGreenTextColor()   { printf("\x1b[32m"); }
static void setRedTextColor()     { printf("\x1b[31m"); }
static void setBlueTextColor()    { printf("\x1b[34m"); }
static void setYellowTextColor()  { printf("\x1b[33m"); }
static void setMagentaTextColor() { printf("\x1b[36m"); }
static void setPurpleTextColor()  { printf("\x1b[38;5;128m"); }
static void resetTextColor()      { printf("\033[0m"); }


// OBC COMMANDS
int  cameraOpenSession();
void cameraConfig(int mode);
int  cameraActivateSession(int mode);
void cameraEnableSensor();
void cameraDisableSensor();
void cameraCaptureImage();
void cameraCloseSession();
void cameraSetAndConfirmImagingParameter(int parameter, int value);

void pdpuGetSessionInformation(int session_id);
void pdpuRangeSetup(int start, int stop);
void pdpuGetImageFromCamera(int session_id);
void pdpuAbortReadOut();
void pdpuDeleteSession(int session_id);

void laserReceiveImageFromPDPU();
void laserSendImageToOGS();

// HYPERSPECTRAL CAMERA COMMANDS
// IMAGE CAPTURE COMMANDS
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

// IMAGE READ OUT COMMANDS
void GET_SESSION_INFORMATION(int session_id);
void READ_OUT_RANGE_SET_UP(int start, int stop);
void READ_OUT_SESSION(int session_id);
void ABORT_READ_OUT();
void DELETE_SESSION(int session_id);

// HYPERSPECTRAL CAMERA REQUESTS
void SUBSYSTEM_STATES(int states[], int color);
void SESSION_INFORMATION(int states[], int color);
int  IMAGING_PARAMETER();
int  CURRENT_SESSION_ID();
int  CURRENT_SESSION_SIZE();

// HELPER FOR REQUEST
void printSubSystemStates(const int states[], int color);
void printSessionInforamtion(const int states[], int color);


// TASK HANDLERS
TaskHandle_t HYPERSPECTRAL_CAMERA_TASK = NULL;

// QUEUE HANDLES
xQueueHandle I2C_OBC    = 0;
xQueueHandle I2C_CAMERA = 0;
xQueueHandle I2C_PDPU   = 0;
xQueueHandle I2C_LASER  = 0;

// MAIN FUNCTION
int main(void) {

	// CREATE THE QUEUE OF SIZE 1
	I2C_OBC    = xQueueCreate(5, sizeof(I2C_Payload));
	I2C_CAMERA = xQueueCreate(5, sizeof(I2C_Payload));
	I2C_PDPU   = xQueueCreate(5, sizeof(I2C_Payload));
	I2C_LASER  = xQueueCreate(5, sizeof(I2C_Payload));

	// TASK CREATION
	xTaskCreate(OBC,                 "OBC",    configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL); //tskIDLE_PRIORITY
	xTaskCreate(HyperSpectralCamera, "CAMERA", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, NULL);
	xTaskCreate(PDPU,                "PDPU",   configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, NULL);
	xTaskCreate(Laser,               "LASER",  configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, NULL);

	vTaskStartScheduler();

	for (;;);
	return 0;
}

void print_I2C_payload(const I2C_Payload p) {
	printf("Command ID : 0x%X\n", p.Command_ID);
	for (int i = 0; i < MAX_PARAMETERS; ++i)
		printf("Parameter %d = %d\n", i, p.Parameter[i]);
}

void printCommandID(const char* command_name, int command_id, int color) {
	if (color == 0)	// OBC
		setYellowTextColor();
	else
		setMagentaTextColor();

	printf("Sending %s Command (0x%x) to camera\n", command_name, command_id);

	resetTextColor();
}

/*
* 
* OBC TASK
* 
*/

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
			printf("\n\tEnter open_session to open a camera session.");
			printf("\n\tEnter configure to configure the currently open session of the camera.");
			printf("\n\tEnter activate_session to activate the currently open session of the camera.");
			printf("\n\tEnter enable_sensor to enable the sensor of the camera.");
			printf("\n\tEnter disable_sensor to disable the sensor of the camera.");
			printf("\n\tEnter capture_image to start the image captuting.");
			printf("\n\tEnter close_session to close the session of the camera.");
			printf("\n");
			printf("\nOptional commands for image capturing:");
			printf("\n\tEnter set_imaging_parameter to set and confirm an imaging parameter of the camera.");
			printf("\n\tEnter store_time_tync to store time sync.");
			printf("\n\tEnter store_user_data to store the user data.");
			printf("\n");
			printf("\nRequired commands for image read out:");
			printf("\n\tEnter pdpu_get_session_information to get the session size and status of session 0.");
			printf("\n\tEnter pdpu_range_set_up to set up the read out range of the next image read out.");
			printf("\n\tEnter pdpu_read_out_session to read out the data of session 0.");
			printf("\n\tEnter pdpu_abort_read_out to read out the data of session 0.");
			printf("\n\tEnter pdpu_delete_session to delete the stored data inside the camera of session 0.");
			printf("\n");
			printf("\nRequired commands for image transmission to OGS:");
			printf("\n\tEnter laser_receive_image to receive the stored image from the PDPU.");
			printf("\n\tEnter laser_send_image to transmit an image to Optical Ground Station.");
			printf("\n");

			resetTextColor();
		}
		// CAMERA REQUIRED COMMANDS FOR IMAGE CAPTURE
		if (strcmp(command_name, "open_session\n") == 0) {
			hyperspectral_camera_session_id = cameraOpenSession();
			setBlueTextColor();
			printf("Received Camera Session ID : %d\n", hyperspectral_camera_session_id);
			resetTextColor();
		}
		if (strcmp(command_name, "configure\n") == 0) {
			cameraConfig(1);	// Configure camera for line scan
		}
		if (strcmp(command_name, "activate_session\n") == 0) {
			session_size = cameraActivateSession(1);	// Activate the currently open session with automatic mode
		}
		if (strcmp(command_name, "enable_sensor\n") == 0) {
			cameraEnableSensor();
		}
		if (strcmp(command_name, "disable_sensor\n") == 0) {
			cameraDisableSensor();
		}
		if (strcmp(command_name, "capture_image\n") == 0) {
			cameraCaptureImage();
		}
		if (strcmp(command_name, "close_session\n") == 0) {
			cameraCloseSession();
		}
		// CAMERA OPTIONAL COMMANDS FOR IMAGE CAPTURE
		if (strcmp(command_name, "set_imaging_parameter\n") == 0) {
			cameraSetAndConfirmImagingParameter(0, 10);
		}
		if (strcmp(command_name, "store_time_tync\n") == 0) {
			STORE_TIME_SYNC();
		}
		if (strcmp(command_name, "store_user_data\n") == 0) {
			STORE_USER_DATA(2, 10, 324);
		}
		if (strcmp(command_name, "bug\n") == 0) {
			setPurpleTextColor();
			printf("PURPLE\n");
			resetTextColor();
		}
		// PDPU COMMANDS
		if (strcmp(command_name, "pdpu_get_session_information\n") == 0) {
			pdpuGetSessionInformation(0);
		}
		if (strcmp(command_name, "pdpu_range_set_up\n") == 0) {
			pdpuRangeSetup(1, 3);
		}
		if (strcmp(command_name, "pdpu_read_out_session\n") == 0) {
			pdpuGetImageFromCamera(0);
		}
		if (strcmp(command_name, "pdpu_abort_read_out\n") == 0) {
			pdpuAbortReadOut();
		}
		if (strcmp(command_name, "pdpu_delete_session\n") == 0) {
			pdpuDeleteSession(0);
		}
		// LASER COMMANDS
		if (strcmp(command_name, "laser_receive_image\n") == 0) {
			laserReceiveImageFromPDPU();
		}
		if (strcmp(command_name, "laser_send_image\n") == 0) {
			laserSendImageToOGS();
		}
	}
}

/*
* 
* Camera Required Image Capture Commands, this are executed by the OBC
* 
*/

int cameraOpenSession() {
	int session_id = -1;

	int states[5];

	OPEN_SESSION();

	session_id = CURRENT_SESSION_ID();

	SUBSYSTEM_STATES(states, 0);

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

	SUBSYSTEM_STATES(states, 0);

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

	SUBSYSTEM_STATES(states, 0);

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

	SUBSYSTEM_STATES(states, 0);

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

	SUBSYSTEM_STATES(states, 0);

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

	SUBSYSTEM_STATES(states, 0);

	while (states[3] == 2) {
		current_tick_time = xTaskGetTickCount();
		time_passed = current_tick_time - starting_tick_time;

		if (time_passed % MONITOR_IMAGE_CAPTURE_PERIOD == 0)
			SUBSYSTEM_STATES(states, 0);
		if (time_passed == MAX_WAIT_TIME_FOR_IMAGE_CAPTURE_COMPLETION)
			break;
	}

	// To be sure that there was issued an image capture the camera 
	// must have an active session, be configured and have the sensor enabled.
	if (states[0] == 2 && states[1] == 1 && states[2] == 1 && states[3] == 0) {
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
	int states[SESSION_INFORAMTION_RETURN_PARAMETERS];

	CLOSE_SESSION();

	SESSION_INFORMATION(states, 0);

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
* Camera Optional Image Capture Commands, this are executed by the OBC
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

	// IDENTIFIERS OF THE CAMERA
	int session_id = -1;
	int read_out_session_id = -1;

	// IMAGE SCANING MODE
	int scan_mode = -1;	// 1 -> line scan, 3 -> line scan test pattern, 5 -> line scan high accuracy mode. 

	// STORAGE PARAMETER AND INFO
	int storage_mode = -1; // 0 -> Manual mode (should never be used), 1 -> Automatic mode.
	int session_size = 0;  // In MegaBytes.

	// TIME SYNC
	int time_sync = 0;

	// IMAGING PARAMETERS
	int imaging_index = 0;
	int imaging_parameters[2] = { 2, 4 }; // lines, frame interval

	// USER DATA
	int packet_id = -1;
	int length = -1;
	int user_data = -1;

	// SUBSYSTEM STATES RESPONSE 
	int session_state  = 0; // 2 Bits
	int config_state   = 0; // 1 Bit
	int sensor_state   = 0; // 1 Bit
	int capture_state  = 0; // 2 Bits
	int read_out_state = 0; // 1 Bit

	// SESSION INFORAMTION RESPONSE 
	int session_close_error[MAX_NUMBER_OF_SESSIONS] = { 0, 0, 0, 0, 0 };	// 1 Bits
	int storage_error[MAX_NUMBER_OF_SESSIONS]       = { 0, 0, 0, 0, 0 };	// 1 Bit
	int total_bytes[MAX_NUMBER_OF_SESSIONS]         = { 0, 0, 0, 0, 0 };	// int64
	int used_bytes[MAX_NUMBER_OF_SESSIONS]          = { 0, 0, 0, 0, 0 };	// int64

	// COMMAND AND REQUESTS INFORMATION
	I2C_Payload rx_payload;
	I2C_Payload tx_payload;

	// RECEIVED COMMAND FROM I2C
	int command_id;
	int received_command;

	// REPRESENTATION OF THE STORED IMAGE DATA
	int stored_image_data[MAX_NUMBER_OF_SESSIONS][MAX_NUMBER_OF_LINES] = { {0, 0, 0, 0, 0},
																		   {0, 0, 0, 0, 0} ,
																		   {0, 0, 0, 0, 0} ,
																		   {0, 0, 0, 0, 0} ,
																		   {0, 0, 0, 0, 0} };

	// START AND STOP RANGE FOR IMAGE READ OUT
	int start_range = 0;
	int stop_range  = MAX_NUMBER_OF_LINES;

	// TIME INFORMATION FOR IMAGE CAPTURE SIMULATION
	TickType_t starting_tick_time = xTaskGetTickCount();;
	TickType_t current_tick_time; 
	TickType_t time_passed;

	for (;;) {
		received_command = xQueueReceive(I2C_CAMERA, &rx_payload, portMAX_DELAY);
		if (received_command) {
			command_id = rx_payload.Command_ID;

			setGreenTextColor();

			// COMMANDS
			if (command_id == 0) {		// 0x00 OPEN SESSION
				if (session_id < MAX_NUMBER_OF_SESSIONS)
					++session_id;
				else
					session_id = 0;

				session_state  = 1; 
				config_state   = 0; 
				sensor_state   = 0; 
				capture_state  = 0; 
				read_out_state = 0;

				session_close_error[session_id] = 1;
				storage_error[session_id]       = 0;	
				total_bytes[session_id]		    = 0;	
				used_bytes[session_id]		    = 0;	

				printf("HyperSpectral Camera Opening Session %d ...\n", session_id);
			}
			if (command_id == 1 && session_state == 1 && config_state == 1) {		// 0x01 ACTIVATE SESSION
				storage_mode  = rx_payload.Parameter[0];
				session_size  = 1024 * (session_id + 1);
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

				session_close_error[session_id] = 0;
				printf("HyperSpectral Camera closing the session with ID: %d\n", session_id);
			}
			if (command_id == 3) {		// 0x03 READ OUT SESSION
				read_out_state = 1;
				read_out_session_id = rx_payload.Parameter[0];
				starting_tick_time = xTaskGetTickCount();
				printf("HyperSpectral Camera iniating image read out\n");
			}
			if (command_id == 4) {		// 0x04 DELETE SESSION
				read_out_session_id = rx_payload.Parameter[0];

				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i) {
					stored_image_data[read_out_session_id][i] = 0;
				}

				printf("HyperSpectral Camera deleted session with ID : % d\n", read_out_session_id);
				printf("Storage released: \n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[read_out_session_id][i]);
			}
			if (command_id == 5) {		// 0x05 STORE TIME SYNC
				time_sync = 1;
				printf("HyperSpectral Camera set time sync as true\n");
			}
			if (command_id == 6) {		// 0x06 STORE USER DATA
				packet_id = rx_payload.Parameter[0];
				length    = rx_payload.Parameter[1];
				user_data = rx_payload.Parameter[2];

				printf("HyperSpectral Camera storing user data:");
				printf("\nPacket ID : %d", packet_id);
				printf("\nLength    : %d", length);
				printf("\nUser Data : %d", user_data);
				printf("\n");
			}
			if (command_id == 7) {		// 0x07 GET SESSION INFORMATION
				read_out_session_id = rx_payload.Parameter[0];
				printf("HyperSpectral Camera has read out session ID : %d\n", read_out_session_id);
			}
			if (command_id == 10) {		// 0x0A ABORT READ OUT
				read_out_state = 0;
				printf("HyperSpectral Camera aborting read out\n");
			}
			if (command_id == 18) {		// 0x12 READ OUT RANGE SET UP
				start_range = rx_payload.Parameter[0];
				stop_range  = rx_payload.Parameter[1];
				printf("HyperSpectral Camera has start range : %d, stop range : %d\n", start_range, stop_range);
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
			}						// Active			   // Configured	    // Enabled
			if (command_id == 39 && session_state == 2 && config_state == 1 && sensor_state == 1) {		// 0x27 CAPTURE IMAGE
				capture_state = 2;
				starting_tick_time = xTaskGetTickCount();
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

				printf("Sending SubSystem States response\n");
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 133) {	// 0x85 SESSION INFORMATION
				tx_payload.Command_ID = 133;
				tx_payload.Parameter[0] = session_close_error[session_id];
				tx_payload.Parameter[1] = storage_error[session_id];
				tx_payload.Parameter[2] = total_bytes[session_id];
				tx_payload.Parameter[3] = used_bytes[session_id];

				printf("Sending Session Information response\n");
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 134) {	// 0x86 CURRENT SESSION ID
				tx_payload.Command_ID = 134;
				tx_payload.Parameter[0] = session_id;
				printf("Sending Session_ID : %d to OBC\n", session_id);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 135) {	// 0x87 CURRENT SESSION SIZE
				tx_payload.Command_ID = 135;
				tx_payload.Parameter[0] = session_size;
				printf("Sending  current Session Size : %d to OBC\n", session_size);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 137) {	// 0x89 IMAGING PARAMETER
				tx_payload.Command_ID = 137;
				tx_payload.Parameter[0] = imaging_parameters[imaging_index];
				printf("Sending imaging parameter value : %d to OBC\n", imaging_parameters[imaging_index]);
				xQueueSend(I2C_OBC, &tx_payload, portMAX_DELAY);
			}
		}

		// Simulate image capture. 
		// The imager will count for a predifined number of ticks after which we consider that the image is captured.
		if (capture_state == 2) {
			setGreenTextColor();

			current_tick_time = xTaskGetTickCount();
			time_passed = current_tick_time - starting_tick_time;

			if (time_passed > IMAGE_CAPTURE_TIME) {
				capture_state = 0;

				// STORE DATA FOR EACH LINE
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					stored_image_data[session_id][i] = i * session_id + 546;

				total_bytes[session_id] = session_size;
				used_bytes[session_id]  = session_size / (MAX_NUMBER_OF_SESSIONS - session_id);

				printf("Image Capture completed for session with ID : %d\n", session_id);
				printf("Stored image data :\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i+1, stored_image_data[session_id][i]);
			}
		}

		// Simulate image download. 
		// The imager will count for a predifined number of ticks after which we consider that the image is captured.
		if (read_out_state == 1) {
			setGreenTextColor();

			current_tick_time = xTaskGetTickCount();
			time_passed = current_tick_time - starting_tick_time;

			if (time_passed > IMAGE_CAPTURE_TIME) {

				read_out_state = 0;

				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i) {
					if (i >= start_range && i < stop_range)
						tx_payload.Parameter[i] = stored_image_data[read_out_session_id][i];
					else
						tx_payload.Parameter[i] = 0;
				}

				printf("HyperSpectral Camera sending read out data :\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[read_out_session_id][i]);

				tx_payload.Command_ID = 20;
				xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
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

	printCommandID("OPEN SESSION", OPEN_SESSION.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &OPEN_SESSION, portMAX_DELAY);
}

void CLOSE_SESSION() {
	I2C_Payload payload;
	payload.Command_ID = 2;

	printCommandID("CLOSE SESSION", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void CONFIGURE(int mode){
	I2C_Payload payload;
	payload.Command_ID = 38;
	payload.Parameter[0] = mode;

	printCommandID("CONFIGURE", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void ACTIVATE_SESSION(int mode) {
	I2C_Payload payload;
	payload.Command_ID = 1;
	payload.Parameter[0] = mode;

	printCommandID("ACTIVATE SESSION", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void ENABLE_SENSOR() {
	I2C_Payload payload;
	payload.Command_ID = 32;

	printCommandID("ENABLE SENSOR", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void DISABLE_SENSOR() {
	I2C_Payload payload;
	payload.Command_ID = 33;

	printCommandID("DISABLE SENSOR", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void CAPTURE_IMAGE() {
	I2C_Payload payload;
	payload.Command_ID = 39;

	printCommandID("CAPTURE IMAGE", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void STORE_TIME_SYNC() {
	I2C_Payload payload;
	payload.Command_ID = 5;

	printCommandID("STORE TIME SYNC", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void STORE_USER_DATA(int packet_id, int length, int user_data) {
	I2C_Payload payload;
	payload.Command_ID = 6;
	payload.Parameter[0] = packet_id;
	payload.Parameter[1] = length;
	payload.Parameter[2] = user_data;

	printCommandID("STORE USER DATA", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void GET_SESSION_INFORMATION(int session_id) {
	I2C_Payload payload;
	payload.Command_ID = 7;
	payload.Parameter[0] = session_id;

	printCommandID("GET SESSION INFORMATION", payload.Command_ID, 1);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void READ_OUT_RANGE_SET_UP(int start, int stop) {
	I2C_Payload payload;
	payload.Command_ID = 18;
	payload.Parameter[0] = start;
	payload.Parameter[1] = stop;

	printCommandID("READ OUT RANGE SET UP", payload.Command_ID, 1);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void READ_OUT_SESSION(int session_id) {
	I2C_Payload payload;
	payload.Command_ID = 3;
	payload.Parameter[0] = session_id;

	printCommandID("READ OUT SESSION", payload.Command_ID, 1);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void ABORT_READ_OUT() {
	I2C_Payload payload;
	payload.Command_ID = 10;

	printCommandID("ABORT READ OUT", payload.Command_ID, 1);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void DELETE_SESSION(int session_id) {
	I2C_Payload payload;
	payload.Command_ID = 4;
	payload.Parameter[0] = session_id;

	printCommandID("DELETE SESSION", payload.Command_ID, 1);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

int CURRENT_SESSION_ID() {
	int session_id = -100;

	I2C_Payload CURRENT_SESSION_ID;
	CURRENT_SESSION_ID.Command_ID = 134;
	printCommandID("CURRENT SESSION ID", CURRENT_SESSION_ID.Command_ID, 0);

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
	printCommandID("CURRENT SESSION SIZE", payload.Command_ID, 0);

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

	printCommandID("SET IMAGING PARAMETER", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

void GET_IMAGING_PARAMETER(int param) {
	I2C_Payload payload;
	payload.Command_ID = 36;
	payload.Parameter[0] = param;

	printCommandID("GET IMAGING PARAMETER", payload.Command_ID, 0);

	xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY);
}

int  IMAGING_PARAMETER() {
	int imaging_parameter = -1;

	I2C_Payload payload;
	payload.Command_ID = 137;	// 0x89
	printCommandID("IMAGING PARAMETER", payload.Command_ID, 0);

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

void SUBSYSTEM_STATES(int states[], int color) {

	I2C_Payload payload;
	payload.Command_ID = 129;	// 0x81
	printCommandID("SUBSYSTEMS STATES" ,payload.Command_ID, color);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the states that the hyperspectral camera returned.
			for(int i = 0; i < SUBSYSTEM_STATES_RETURN_PARAMETERS; ++i)
				states[i] = payload.Parameter[i];

			printSubSystemStates(states, color);
		}
	}
}

void SESSION_INFORMATION(int states[], int color) {

	I2C_Payload payload;
	payload.Command_ID = 133;	// 0x85
	printCommandID("SESSION INFORMATION", payload.Command_ID, color);

	if (!xQueueSend(I2C_CAMERA, &payload, portMAX_DELAY)) {
		printf("OBC FAILED TO SEND COMMAND 0x%x TO THE HYPERSPECTRAL CAMERA\n", payload.Command_ID);
	}
	else {
		if (xQueueReceive(I2C_OBC, &payload, portMAX_DELAY)) {
			// Read the states that the hyperspectral camera returned.
			for (int i = 0; i < SESSION_INFORAMTION_RETURN_PARAMETERS; ++i)
				states[i] = payload.Parameter[i];

			printSessionInforamtion(states, color);
		}
	}
}

void printSubSystemStates(const int states[], int color) {
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

	if (color == 0)	// OBC
		setYellowTextColor();
	else            // PDPU
		setMagentaTextColor();

	printf("Received response from the camera, printing the states:\n");
	printf("Session  State : %s\n", session);
	printf("Config   State : %s\n", config);
	printf("Sensor   State : %s\n", sensor);
	printf("Capture  State : %s\n", capture);
	printf("Read Out State : %s\n", readout);

	resetTextColor();
}

void printSessionInforamtion(const int states[], int color) {
	char* session_close_error = "";
	char* storage_error = "";

	int total_bytes = 0;
	int used_bytes = 0;

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

	total_bytes = states[2];
	used_bytes = states[3];

	if (color == 0)	// OBC
		setYellowTextColor();
	else            // PDPU
		setMagentaTextColor();

	printf("Received response from the camera, printing the states:\n");
	printf("Closed Session State : %s\n", session_close_error);
	printf("Storage State        : %s\n", storage_error);
	printf("Total Bytes          : %d\n", total_bytes);
	printf("Used Bytes           : %d\n", used_bytes);

	resetTextColor();
}

/*
* 
* PDPU TASK
* 
*/

void PDPU(void) {

	// IDENTIFIERS AND MODES OF THE CAMERA
	int session_id = -1;
	int session_size = 0; // In MegaBytes.

	// SUBSYSTEM STATES RESPONSE 
	int session_state  = 0; // 2 Bits
	int config_state   = 0; // 1 Bit
	int sensor_state   = 0; // 1 Bit
	int capture_state  = 0; // 2 Bits
	int read_out_state = 0; // 1 Bit

	// SESSION INFORAMTION RESPONSE 
	int session_close_error = 0; // 1 Bits
	int storage_error = 0;       // 1 Bit
	int total_bytes  = 0;		 // int64
	int used_bytes   = 0;		 // int64

	// AUXILARY PARAMETER FOR SESSION INFO
	int states[SESSION_INFORAMTION_RETURN_PARAMETERS];
	int sub_states[SUBSYSTEM_STATES_RETURN_PARAMETERS];

	// COMMAND AND REQUESTS INFORMATION
	I2C_Payload rx_payload;
	I2C_Payload tx_payload;

	// RECEIVED COMMAND FROM I2C
	int command_id;
	int received_command;

	// START AND STOP RANGE OF IMAGE DATA
	int start = 0;
	int stop  = MAX_NUMBER_OF_LINES;

	// REPRESENTATION OF THE STORED IMAGE DATA
	int stored_image_data[MAX_NUMBER_OF_LINES] = { 0, 0, 0, 0, 0 };

	// TIME INFORMATION FOR IMAGE CAPTURE SIMULATION
	TickType_t start_tick_time;
	TickType_t current_tick_time;
	TickType_t time_passed;

	for (;;) {
		received_command = xQueueReceive(I2C_PDPU, &rx_payload, portMAX_DELAY);
		if (received_command) {
			command_id = rx_payload.Command_ID;

			setMagentaTextColor();

			// COMMANDS
			if (command_id == 0) {		// 0x00 GET SESSION SIZE
				session_id = rx_payload.Parameter[0];

				GET_SESSION_INFORMATION(session_id);

				SESSION_INFORMATION(states, 1);

				session_close_error = states[0]; // 1 Bits
				storage_error       = states[1]; // 1 Bit
				total_bytes         = states[2]; // int64
				used_bytes          = states[3]; // int65
			}
			if (command_id == 3) {		// 0x03 READ OUT SESSION
				session_id = rx_payload.Parameter[0];

				READ_OUT_SESSION(session_id);
				start_tick_time = xTaskGetTickCount();

				SUBSYSTEM_STATES(sub_states, 1);

				while (sub_states[4] == 1) {
					current_tick_time = xTaskGetTickCount();
					time_passed = current_tick_time - start_tick_time;

					if (time_passed % MONITOR_IMAGE_CAPTURE_PERIOD == 0)
						SUBSYSTEM_STATES(sub_states, 1);
					if (time_passed == MAX_WAIT_TIME_FOR_IMAGE_CAPTURE_COMPLETION)
						break;
				}
			}
			if (command_id == 4) {		// 0x04 DELETE SESSION
				session_id = rx_payload.Parameter[0];

				DELETE_SESSION(session_id);
			}
			if (command_id == 10) {		// 0x0A ABORT READ OUT
				ABORT_READ_OUT();
			}
			if (command_id == 18) {		// 0x12 RANGE SET UP
				start = rx_payload.Parameter[0];
				stop  = rx_payload.Parameter[1];
				READ_OUT_RANGE_SET_UP(start, stop);
			}
			if (command_id == 20) {		// 0x12 RECEIVE IMAGE DATA
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					stored_image_data[i] = rx_payload.Parameter[i];

				printf("PDPU received read out data :\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[i]);

				// To be sure that there was issued an image capture the camera 
				// must have an active session, be configured and have the sensor enabled.
				if (sub_states[4] == 0) {
					setMagentaTextColor();
					printf("Downloaded image from camera successfully\n");
					resetTextColor();
				}
				else {
					setRedTextColor();
					printf("Couldn't download the image from the camera\n");
					resetTextColor();
				}
			}
			if (command_id == 100) {		// 0x64 SEND IMAGE TO LASER
				tx_payload.Command_ID = 2;

				tx_payload.Parameter[0] = session_id;

				for (int i = 1; i < MAX_PARAMETERS; ++i)
					tx_payload.Parameter[i] = stored_image_data[i-1];

				printf("Sending stored image to laser\n");
				printf("Session ID : %d\n", session_id);
				printf("Stored Image Data:\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[i]);

				xQueueSend(I2C_LASER, &tx_payload, portMAX_DELAY);
			}
		}

		resetTextColor();
	}
}

/*
*
* PAYLOAD DATA PROCESSING UNIT (PDPU) COMMAND TRANSACTIONS, THIS COMMANDS ARE EXEUTED FROM THE OBC
*
*/

void pdpuGetSessionInformation(int session_id) {
	I2C_Payload tx_payload;

	tx_payload.Command_ID = 0;
	tx_payload.Parameter[0] = session_id;

	xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
}

void pdpuRangeSetup(int start, int stop) {
	I2C_Payload tx_payload;

	tx_payload.Command_ID = 18;
	tx_payload.Parameter[0] = start;
	tx_payload.Parameter[1] = stop;

	xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
}

void pdpuGetImageFromCamera(int session_id) {
	I2C_Payload tx_payload;

	tx_payload.Command_ID = 3;
	tx_payload.Parameter[0] = session_id;

	xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
}

void pdpuAbortReadOut() {
	I2C_Payload tx_payload;

	tx_payload.Command_ID = 10;

	xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
}

void pdpuDeleteSession(int session_id) {
	I2C_Payload tx_payload;

	tx_payload.Command_ID = 4;
	tx_payload.Parameter[0] = session_id;

	xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
}

/*
*
* LASER TASK
*
*/

void Laser(void) {

	// STORED SESSION
	int session_id = -1;

	// COMMAND AND REQUESTS INFORMATION
	I2C_Payload rx_payload;
	I2C_Payload tx_payload;

	// RECEIVED COMMAND FROM I2C
	int command_id;
	int received_command;

	// REPRESENTATION OF THE STORED IMAGE DATA
	int stored_image_data[MAX_NUMBER_OF_LINES] = { 0, 0, 0, 0, 0 };

	for (;;) {
		received_command = xQueueReceive(I2C_LASER, &rx_payload, portMAX_DELAY);
		if (received_command) {
			command_id = rx_payload.Command_ID;

			setPurpleTextColor();

			// COMMANDS
			if (command_id == 0) {		// 0x00 SEND IMAGE TO OGS
				printf("Laser sending stored image to Optical Ground Station...\n");
				printf("Session ID : %d\n", session_id);
				printf("Stored Image Data:\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[i]);

			}
			if (command_id == 1) {		// 0x01 READ OUT IMAGE FROM PDPU
				tx_payload.Command_ID = 100;

				xQueueSend(I2C_PDPU, &tx_payload, portMAX_DELAY);
			}
			if (command_id == 2) {		// 0x02 RECEIVE IMAGE DATA FROM PDPU
				session_id = rx_payload.Parameter[0];

				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					stored_image_data[i] = rx_payload.Parameter[i+1];

				printf("Laser received image from PDPU\n");
				printf("Session ID : %d\n", session_id);
				printf("Stored Image Data:\n");
				for (int i = 0; i < MAX_NUMBER_OF_LINES; ++i)
					printf("line %d : %d\n", i + 1, stored_image_data[i]);
			}
		}

		resetTextColor();
	}
}

/*
*
* LASER COMMAND TRANSACTIONS, THIS COMMANDS ARE EXEUTED FROM THE OBC
*
*/

void laserSendImageToOGS() {
	I2C_Payload payload;

	payload.Command_ID = 0;

	xQueueSend(I2C_LASER, &payload, portMAX_DELAY);
}

void laserReceiveImageFromPDPU() {
	I2C_Payload payload;

	payload.Command_ID = 1;

	xQueueSend(I2C_LASER, &payload, portMAX_DELAY);
}
