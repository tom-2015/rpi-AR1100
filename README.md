# rpi-AR1100
    Example program for calibrating the Microchip AR1100 
    resistive touch screen controller from the Raspberry Pi (Raspbian)
    Requires:
    sudo apt-get install git make g++ libusb-1.0-0-dev libgtk2.0-dev
    
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
