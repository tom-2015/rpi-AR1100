#ifndef AR1100_CLASS_H
	#define AR1100_CLASS_H
    
	#include <libusb-1.0/libusb.h>
    
    #define AR1100_MODE_GENERIC 1
    #define AR1100_MODE_MOUSE 2
    #define AR1100_MODE_DIGITIZER 3
    
    //see data sheet
    #define AR1100_SYNC 0x55
    
    #define AR1100_CMD_TOUCH_ENABLE 0x12
    #define AR1100_CMD_TOUCH_DISABLE 0x13
    #define AR1100_CMD_CALIBRATE 0x14
    #define AR1100_CMD_REG_READ 0x20
    #define AR1100_CMD_REG_WRITE 0x21
    #define AR1100_CMD_EE_READ 0x28
    #define AR1100_CMD_EE_WRITE 0x29
    #define AR1100_CMD_EE_READ_PARAMS 0x2B
    #define AR1100_CMD_EE_WRITE_PARAMS 0x23
    
    //no response packet for these commands
    #define AR1100_CMD_HID_GENERIC 0x70
    #define AR1100_CMD_USB_MOUSE 0x71
    #define AR1100_CMD_DIGITIZE 0x72
    
    #define AR1100_CMD_RESULT_OK 0x00
    #define AR1100_CMD_RESULT_UNKNOWN 0x01
    #define AR1100_CMD_RESULT_TIMEOUT 0x04
    #define AR1100_CMD_RESULT_EEPARAMS_ERR 0x05
    #define AR1100_CMD_RESULT_CAL_CANCEL 0xFC
    #define AR1100_CMD_TRANSFER_ERROR 0xFF //if problem with libusb
    
    #define AR1100_VENDOR_ID 0x04D8
    #define AR1100_PID_GENERIC 0x0C01 //generic HID
    #define AR1100_PID_MOUSE  0x0C02 //mouse
    #define AR1100_PID_DIGITIZER 0x0C03 //digitizer
    
    #define AR1100_CAL_TYPE_4POINT 0x01
    #define AR1100_CAL_TYPE_9POINT 0x02
    #define AR1100_CAL_TYPE_25POINT 0x03
    #define AR1100_CAL_TYPE_AR1000  0x04
    
    #define AR1100_TIMEOUT 1000
    
	class AR1100 {
        private:
            struct libusb_device_handle *devh;
            int device_ready;
        public:
            AR1100();
            ~AR1100();
            int connect(int vid, int pid); //connects to the AR1100, returns 0 if success
            void clear_in_buffer(); //clears waiting IN packets on USB interface
            int send_command(unsigned char cmd, unsigned char * data, int data_count, bool wait_response=true); //sends a command to AR1100 and waits TIMEOUT_MS for a return packet (if wait_response is set to true)
            int write_data(unsigned char * data, int data_count, int * bytes_written, int time_out=AR1100_TIMEOUT);
            int read_data(unsigned char * data, int data_count, int * bytes_read, int time_out=AR1100_TIMEOUT);
            bool touch_disable(); //disable touch (call this first before sending any commands, then call clear_in_buffer)
            bool touch_enable(); //enable touch 
            bool calibrate(unsigned char type); //start calibration (need hid generic first)
            bool calibrate_next_point(); //returns true if we can calibrate next point
            bool switch_hid_generic(); //switch to hid generic, return true if success 
            bool switch_usb_mouse(); //switch to usb mouse, return true if success 
            bool switch_digitizer(); //switch to digitizer, return true if success
    };

#endif