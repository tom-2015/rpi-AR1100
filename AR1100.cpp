#include "AR1100.h"
#include <stdio.h>


static const int MAX_INTERRUPT_IN_TRANSFER_SIZE = 64;
static const int MAX_INTERRUPT_OUT_TRANSFER_SIZE = 64;
static const int INTERFACE_NUMBER = 0;
static const int TIMEOUT_MS = AR1100_TIMEOUT;
static const int INTERRUPT_IN_ENDPOINT = 0x81;
static const int INTERRUPT_OUT_ENDPOINT = 0x01;

typedef struct {
    unsigned char sync;
    unsigned char size;
    unsigned char cmd;
    unsigned char data[MAX_INTERRUPT_OUT_TRANSFER_SIZE-3];
}AR1100_cmd_packet;

typedef struct {
    unsigned char sync;
    unsigned char size;
    unsigned char status;
    unsigned char cmd;
    unsigned char data[MAX_INTERRUPT_IN_TRANSFER_SIZE-4];
}AR1100_response_packet;


AR1100::AR1100(){
    device_ready=0;
    devh=NULL;
}

AR1100::~AR1100(){
    if (devh!=NULL) libusb_close(devh);
}

int AR1100::connect(int vid, int pid){
	int result;
   
	result = libusb_init(NULL);
	if (result >= 0){
		devh = libusb_open_device_with_vid_pid(NULL, vid, pid);

		if (devh != NULL){
			// The HID has been detected.
			// Detach the hidusb driver from the HID to enable using libusb.
			libusb_detach_kernel_driver(devh, INTERFACE_NUMBER);
            result = libusb_claim_interface(devh, INTERFACE_NUMBER);
            if (result >= 0){
                device_ready = 1;
                return 0;
            }else{
                #ifdef DEBUG
                    fprintf(stderr, "libusb_claim_interface error %d\n", result);
                #endif
            }
		}else{
            #ifdef DEBUG
                fprintf(stderr, "Unable to find the device.\n");
            #endif
            result=1;
		}
	}else{
        #ifdef DEBUG
            fprintf(stderr, "Unable to initialize libusb.\n");
        #endif
	}

    return result;
	/*if (device_ready){
		// Send and receive data.
		exchange_input_and_output_reports_via_interrupt_transfers(devh);

		libusb_release_interface(devh, 0);
	}
	libusb_close(devh);
	libusb_exit(NULL);*/
}

int AR1100::write_data(unsigned char * data, int data_count, int * bytes_written, int time_out){
    #ifdef DEBUG
        printf("USB tx %d:\n", data_count);
        for (int i=0; i< data_count; i++){
            printf("%02x ", data[i]);
            if (((i+1)%16)==0) printf("\n");
        }
        printf("\n");
    #endif
    int result = libusb_interrupt_transfer(devh,INTERRUPT_OUT_ENDPOINT,data,data_count,bytes_written,time_out);
    #ifdef DEBUG
        printf("Result %d\n", result);
    #endif
    return result;
}

int AR1100::read_data(unsigned char * data, int data_count, int * bytes_read, int time_out){

    int result= libusb_interrupt_transfer(devh,INTERRUPT_IN_ENDPOINT,data,data_count,bytes_read,time_out);
    #ifdef DEBUG
        if (result==0){
            printf("USB rx %d:\n", data_count);
            for (int i=0; i<data_count;i++){
                printf("%02x ", data[i]);
                if (((i+1)%16)==0) printf("\n");
            }
            printf("\n");
        }
    #endif   
    return result;
}

//clears all waiting in USB packets
void AR1100::clear_in_buffer(){
    int bytes_transferred;
	int result = 0;
 	unsigned char data_in[MAX_INTERRUPT_IN_TRANSFER_SIZE];
    if (!device_ready) return;
    while (result==0){
        result = read_data(data_in, MAX_INTERRUPT_IN_TRANSFER_SIZE, &bytes_transferred,10); //libusb_interrupt_transfer(devh,INTERRUPT_IN_ENDPOINT,data_in,MAX_INTERRUPT_IN_TRANSFER_SIZE,&bytes_transferred,10);
        if (bytes_transferred==0) return;
    }
}

//sends a command to AR1100 and waits TIMEOUT_MS for a return packet (if wait_response is set to true)
int AR1100::send_command(unsigned char cmd, unsigned char * data, int data_count, bool wait_response){
    int bytes_transferred;
	int result = 0;
 	AR1100_cmd_packet command={0};
    AR1100_response_packet response={0};
    #ifdef DEBUG
        printf("Sending cmd %#x\n", cmd);
    #endif
    if (data_count > sizeof(command.data)) data_count = sizeof(command.data);
	
    command.sync = AR1100_SYNC;
    command.size = data_count+1;
    command.cmd = cmd;
    for (int i=0; i<data_count;i++){
        command.data[i]=data[i];
    }
    
    if (device_ready){
        // Write data to the device.
        result = write_data((unsigned char *)&command,sizeof(command),&bytes_transferred);
        if (result==0){
            if (wait_response){
                //read the result of the command
                result = read_data((unsigned char *) & response,sizeof(response),&bytes_transferred);
                if (result==0){
                    #ifdef DEBUG
                        printf("COMMAND response: %d\n", response.status);
                    #endif                    
                    if (response.sync==AR1100_SYNC && response.cmd==cmd){
                        return response.status; //return status if OK
                    }
                }
            }else{
                return AR1100_CMD_RESULT_OK;
            }
        }
    }else{
        #ifdef DEBUG
            printf("Device not connected %d\n", device_ready);
        #endif
    }
    return AR1100_CMD_TRANSFER_ERROR;

}

bool AR1100::touch_disable(){
    return send_command(AR1100_CMD_TOUCH_DISABLE, NULL, 0, true)==AR1100_CMD_RESULT_OK;
}

bool AR1100::touch_enable(){
    return send_command(AR1100_CMD_TOUCH_ENABLE, NULL, 0, true)==AR1100_CMD_RESULT_OK;
}

bool AR1100::calibrate(unsigned char type){
    return send_command(AR1100_CMD_CALIBRATE, &type, 1, true)==AR1100_CMD_RESULT_OK;
}

bool AR1100::calibrate_next_point(){
    AR1100_response_packet response;
    int bytes_read;
    int result = read_data((unsigned char *) & response, sizeof(response), & bytes_read, 5);
    if (result==0 && bytes_read>0){
        #ifdef DEBUG
            printf("Next point: %d\n", response.status);
        #endif
        return response.sync==AR1100_SYNC && response.cmd==AR1100_CMD_CALIBRATE && response.status==AR1100_CMD_RESULT_OK;
    }
    return false;
}

bool AR1100::switch_hid_generic(){ //switch to hid generic
    return send_command(AR1100_CMD_HID_GENERIC, NULL, 0, false)==AR1100_CMD_RESULT_OK;
}

bool AR1100::switch_usb_mouse(){ //switch to usb mouse
    return send_command(AR1100_CMD_USB_MOUSE, NULL, 0, false)==AR1100_CMD_RESULT_OK;
}

bool AR1100::switch_digitizer(){ //switch to digitizer
    return send_command(AR1100_CMD_DIGITIZE, NULL, 0, false)==AR1100_CMD_RESULT_OK;
}
