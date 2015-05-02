#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include "AR1100.h"

/**************************************************************************************
    Example program for calibrating the Microchip AR1100 
    resistive touch screen controller from the Raspberry Pi (Raspbian)
    Requires:
    sudo apt-get install make g++ libusb-dev libgtk2.0-dev
    
    Installation:
    - "cd" to the directory of the source code (this file) and type "make"
    -code will compile
    -sudo chmod +x AR1100
    
    Execution:
    Calibration with 9 points on the screen:
    sudo ./AR1100 -c 9
    You can calibrate with 4 or 25 points, 9 is the default
    sudo ./AR1100 -m
    switch the AR1100 to USB mouse mode
    Combination:
    sudo ./AR1100 -c 9 -m
    Calibrate and switch to mouse mode
    
    Debugging:
    If you want more debugging info you can compile the code with:
    make clean
    make DEBUG=1
    
***************************************************************************************/

static void do_drawing(cairo_t *);

typedef struct {
    int x;
    int y;
}calibration_point;

#define MAX_CALIBRATION_POINTS 25
#define CALIBRATION_POINT_SIZE 10 //size of the + on the screen
#define INSET 64                  //selects the margin from left/top/bottom/right where to start drawing the points default is 64/256/2 = 12.5% margin
#define CONNECT_MAX_RETRY 10

calibration_point calibration_points[MAX_CALIBRATION_POINTS]; //all points that have to be calibrated
int current_calibration_point; //current displayed
int calibration_point_count;   //nr of calibration points

AR1100 AR1100;
int AR1100_configured_mode; //here we keep the current configured mode to restore after calibration

//sends commands to the AR1100 to exit calibration mode
void exit_calibration(){
    AR1100.touch_disable(); //send any command to exit calibration
}

//the window was closed
void destroy (void) {
    printf("The window was forced to close.\n");
    exit_calibration();
    gtk_main_quit();
}

//called if a button was pressed, exit
static gboolean key_pressed(GtkWidget *button, GdkEventKey *event){
    printf ("Button pressed during calibration, exiting!\n");
    exit_calibration();
    gtk_main_quit();
    return FALSE;
}

//called if the window was clicked, if this happens the AR1100 is probably not in calibration mode
static gboolean clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data){
    printf("Received a mouse click, AR1100 not in calibration mode?, exiting %d\n", event->button);
    destroy();
    return TRUE;
}

//this function is called when we need to redraw the +
static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data){
    cairo_t *cr = gdk_cairo_create(widget->window);
    cairo_rectangle(cr, event->area.x, event->area.y, event->area.width, event->area.height);
    cairo_set_source_rgb(cr, 255, 255, 255);
    cairo_fill(cr);
    //cairo_clip(cr);
    do_drawing(cr);
    cairo_destroy(cr);
    return FALSE;
}

//draws a + on the screen where to click for calibration
static void do_drawing(cairo_t *cr){
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);

    calibration_point point = calibration_points[current_calibration_point];
    
    #ifdef DEBUG
        printf("Drawing point %d: (%d,%d)\n",current_calibration_point+1, point.x, point.y);
    #endif
    
    cairo_move_to(cr, point.x - CALIBRATION_POINT_SIZE, point.y);
    cairo_line_to(cr, point.x + CALIBRATION_POINT_SIZE, point.y);

    cairo_move_to(cr, point.x, point.y - CALIBRATION_POINT_SIZE);
    cairo_line_to(cr, point.x, point.y + CALIBRATION_POINT_SIZE);
    
    cairo_stroke(cr);    
}

//timer function runs every 10ms to redraw the screen and move to next calibration point if needed
gboolean update_screen(gpointer data){
    GtkWidget *widget = GTK_WIDGET(data);
    if (!widget->window){
        printf("Error no window.");
        return false;
    }
    if (AR1100.calibrate_next_point()){
        #ifdef DEBUG
            printf("Go to next point.\n");
        #endif
        gtk_widget_queue_draw(widget);
        current_calibration_point++;
        if (current_calibration_point==calibration_point_count){
            printf("End of calibration reached.\n");
            gtk_main_quit();
            return false;
        }
    }
    return true;
}

//starts the GUI with gtk2.0, calculates the calibration points
void setup_calibration_gui (int points){
    GtkWidget *window;
    GtkWidget *darea;
    current_calibration_point=0; //start with point 0
    calibration_point_count=points;

    gtk_init(0, NULL);
    
    //generate a full screen window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "AR1100 Calibration");
    gtk_window_set_decorated (GTK_WINDOW(window), false);
    gint width=gdk_screen_width();
    gint height=gdk_screen_height();
    gtk_widget_set_size_request(window, width, height);
    gtk_window_fullscreen(GTK_WINDOW(window));
    
    //attach a drawing area
    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);
    
    //connect events and signals
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(darea), "expose_event", G_CALLBACK(on_expose_event), NULL); //tells us to update the drawing
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);   //window closed
    gtk_signal_connect(GTK_OBJECT(window), "key-press-event", G_CALLBACK(key_pressed), NULL); //a key was pressed (close window)
    g_signal_connect(window, "button-press-event", G_CALLBACK(clicked), NULL); //button was pressed

    //setup the points
    printf("Screen width is %d, screen height is %d\n", width, height);
    int x_margin = width * INSET / 256; //see AR1100 datasheet 
    int y_margin = height * INSET / 256;
    int x_point_count = sqrt(points);
    int y_point_count = sqrt(points);
    for (int j=0; j < y_point_count; j++){
        for (int i=0; i < x_point_count; i++){
            calibration_points[i+j*x_point_count].x = x_margin/2 + (width - x_margin)* i / (x_point_count-1) ;
            calibration_points[i+j*x_point_count].y = y_margin/2 + (height - y_margin)* j / (y_point_count-1) ;
            printf("Calibration point %d: (%d,%d)\n", i+j * x_point_count+1, calibration_points[i+j*x_point_count].x, calibration_points[i+j*x_point_count].y);
        }
    }
    
    //add a timeout function which will be called every 10ms to check if there is data from the usb and then redraw the + on the screen if needed
    g_timeout_add(250, update_screen, window);
    
    //show window
    gtk_widget_show_all(window);
    
    gtk_main();
}


int main(int argc, char *argv[]){
    int i=0;
    int cal_type=0;
    int cal_points=9;
    
    if (argc==1 || (argc==2 && strcmp(argv[0], "-?")==0)){
        printf("Command line options:\n");
        printf("    -?       show this message.\n");
        printf("    -m       switch AR11000 to mouse mode.\n");
        printf("    -h       switch AR11000 to HID mode.\n");
        printf("    -d       switch AR11000 to digitizer mode.\n");
        printf("    -c [p]   calibrate using p number of points.\n");
        printf("             valid values for p: 4, 9 and 25 default is 9.\n");
        return 0;
    }
    
    if (AR1100.connect(AR1100_VENDOR_ID,AR1100_PID_GENERIC)==0){
        printf("Connected to AR1100 as generic HID.\n");
        AR1100_configured_mode = AR1100_MODE_GENERIC;
    }else if (AR1100.connect(AR1100_VENDOR_ID,AR1100_PID_MOUSE)==0){
        printf("Connected to AR1100 as USB mouse.\nSwitching to generic HID mode.\n");
        AR1100_configured_mode = AR1100_MODE_MOUSE;
    }else if (AR1100.connect(AR1100_VENDOR_ID,AR1100_PID_DIGITIZER)==0){
        printf("Connected to AR1100 as digitizer.\nSwitching to generic HID mode.\n");
        AR1100_configured_mode = AR1100_MODE_DIGITIZER;
    }else{
        printf("Could not connect to the AR1100!\n");
        return 1;
    }
    
    if (AR1100_configured_mode!=AR1100_MODE_GENERIC){
        printf("The AR1100 is not in generic HID mode, switching mode.\n");
        if (!AR1100.switch_hid_generic()){
            printf("Failed to switch to generic HID device.\n");
            return 1;
        }
        sleep(1);
        
        i=0;
        while (AR1100.connect(AR1100_VENDOR_ID,AR1100_PID_GENERIC)!=0 && i < CONNECT_MAX_RETRY){
            printf(".");
            sleep(1);
        }
        if (i==CONNECT_MAX_RETRY){
            printf("Error connecting to AR1100 after mode switch!");
            return 1;
        }
    }
    
    i=1;
    while (i<argc){
        if (strcmp(argv[i], "-c")==0){ //enable calibration
            if (argc>i+1){
                if(strcmp(argv[i+1], "4")==0){
                    cal_type=AR1100_CAL_TYPE_4POINT;
                    cal_points=4;
                    i++;
                }else if(strcmp(argv[i+1], "9")==0){
                    cal_type=AR1100_CAL_TYPE_9POINT;
                    cal_points=9;
                    i++;
                }else if(strcmp(argv[i+1], "25")==0){
                    cal_type=AR1100_CAL_TYPE_25POINT;
                    cal_points=25;
                    i++;
                }else{
                    cal_type=AR1100_CAL_TYPE_9POINT;
                    cal_points=9;
                }
            }
        }else if (strcmp(argv[i], "-m")==0){
            AR1100_configured_mode = AR1100_MODE_MOUSE;
        }else if (strcmp(argv[i], "-h")==0){
           AR1100_configured_mode = AR1100_MODE_GENERIC;
        }else if (strcmp(argv[i], "-d")==0){
           AR1100_configured_mode = AR1100_MODE_DIGITIZER;
        }else{
            printf("Uknown option %s.\n", argv[i]);
        }
        i++;
    }
    
    if (!AR1100.touch_disable()){
        printf("Error disabeling touch!\n");
    }
    
    AR1100.clear_in_buffer();
    
    if (cal_type!=0){
        printf("Calibrating %d points.\n", cal_points);
        if (AR1100.calibrate(cal_type)){
            setup_calibration_gui(cal_points);
        }
    }
    
    switch (AR1100_configured_mode){
        case AR1100_MODE_MOUSE:
            printf("Setting AR1100 to mouse mode.\n");
            AR1100.switch_usb_mouse();
            break;
        case AR1100_MODE_DIGITIZER:
            printf("Setting AR1100 digitize mode.\n");
            AR1100.switch_digitizer();
            break;
        default:
            printf("Setting AR1100 to generic HID.\n");
            AR1100.switch_hid_generic(); //reset
            break;
    }
    return 0;
}