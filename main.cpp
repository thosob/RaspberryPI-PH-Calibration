
/* 
 * File:   main.cpp
 * Author: tsobieroy
 *
 * Created on 2. Januar 2017, 07:53
 */

#include <cstdlib>
#include <iostream>
#include <cstdlib>
#include <string>
#include <string.h>
#include <cmath>
#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <inttypes.h> 
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <cstdint>

using namespace std;

/**
 * Global variable to set the temperaturePath of the electrode
 */
string temperature_Path;
uint wired_Address;

/**
 * @brief reads from ADS1015
 * @param fd aka i2c address
 * @param channel for the multiplexer
 * @return integer with milivoltage
 */
int readADC_SingleEnded(int fd, int channel) {

    int ADS_address = fd; // Address of our device on the I2C bus
    int I2CFile;

    uint8_t writeBuf[3]; // Buffer to store the 3 bytes that we write to the I2C device
    uint8_t readBuf[2]; // 2 byte buffer to store the data read from the I2C device

    int16_t val; // Stores the 16 bit value of our ADC conversion

    
    I2CFile = open("/dev/i2c-1", O_RDWR); // Open the I2C device

    ioctl(I2CFile, I2C_SLAVE, ADS_address); // Specify the address of the I2C Slave to communicate with

    // These three bytes are written to the ADS1115 to set the config register and start a conversion 
    // There are 3 registers  one is the config register which is accessed by writing one to the buffer    
    writeBuf[0] = 1; // This sets the pointer register so that the following two bytes write to the config register
    // Modifying adressing part
    // 100 : AIN P = AIN0 and AIN N = GND => Results in 0xC3 11000011 CHANNEL 0
    // 101 : AIN P = AIN1 and AIN N = GND => Results in 0xD3 11010011 CHANNEL 1
    // 110 : AIN P = AIN2 and AIN N = GND => Results in 0xE3 11100011 CHANNEL 2
    // 111 : AIN P = AIN3 and AIN N = GND => Results in 0xF3 11110011 CHANNEL 3
    switch(channel){
        //set up 0
        case 0: writeBuf[1] = 0xC3; break;
        //set up 1
        case 1: writeBuf[1] = 0xD3; break;
        //set up 2
        case 2: writeBuf[1] = 0xE3; break;
        //set up channel 3
        case 3: writeBuf[1] = 0xF3; break;
        //set up default is channel 0
        default: writeBuf[1] = 0xC3; break;
    }
    
    //writeBuf[1] = 0xC3; // This sets the 8 MSBs of the config register (bits 15-8) to 11000011
    writeBuf[2] = 0x03; // This sets the 8 LSBs of the config register (bits 7-0) to 00000011

    // Initialize the buffer used to read data from the ADS1115 to 0
    readBuf[0] = 0;
    readBuf[1] = 0;

    // Write writeBuf to the ADS1115, the 3 specifies the number of bytes we are writing,
    // this begins a single conversion
    write(I2CFile, writeBuf, 3);

    // Wait for the conversion to complete, this requires bit 15 to change from 0->1
    while ((readBuf[0] & 0x80) == 0) // readBuf[0] contains 8 MSBs of config register, AND with 10000000 to select bit 15
    {
        read(I2CFile, readBuf, 2); // Read the config register into readBuf
    }

    writeBuf[0] = 0; // Set pointer register to 0 to read from the conversion register
    write(I2CFile, writeBuf, 1);

    read(I2CFile, readBuf, 2); // Read the contents of the conversion register into readBuf

    val = readBuf[0] << 8 | readBuf[1]; // Combine the two bytes of readBuf into a single 16 bit result 

    printf("Voltage Reading %f (V) \n", (float) val * 4.096 / 32767.0); // Print the result to terminal, first convert from binary value to mV

    close(I2CFile);
    return val;
}


/**
 * @brief mV of the probe
 * @param file_Descriptor
 * @return Voltage
 */
float get_Probe_mV(int i2c_Address, int i2c_Port) {
    //Sparkfun i2c is built in -> special case
    if (i2c_Address == 77) {
        //we have to set it to zero, regardless whats read in console param
        i2c_Port = 0;
        //and we have to use the special configuration of wiringpi
        
        //Loading phAdress - using wiringPi, so using special address range  
        int raw = wiringPiI2CReadReg16(wired_Address, i2c_Port);
        raw = raw >> 8 | ((raw << 8) &0xffff);
        //std::cout << raw << endl;
        //3.3 equals the voltage
        //Design Decision: 3.3V implementation   
        //4096 - 12bit in total 
        if (raw > 0) {
            return (((float) raw / 4096) * 3.3) * 1000;
        } else {
            return -1;
        }
    } else {
        //normal case
        return readADC_SingleEnded(i2c_Address, i2c_Port) * 4.096 / 32767.0;

    }

}

/**
 * @brief gets the mean out of a number of measurements
 * @param measurement_size
 * @param ph_probe_Address
 * @return mean > 0 if enough data was collected, else -1 
 */
float getMeanMeasurements(int measurement_size, int i2c_address, int i2c_address_port) {
    float measurements[measurement_size];
    float mean_measurements = 0.0;
    float mV = 0.0;
    int valid_data = 0;

    for (int i = 0; i < measurement_size; i++) {
        //Gets mV from Probe    
        mV = get_Probe_mV(i2c_address, i2c_address_port);
        //if Probe Value is valid
        if (mV > 0) {
            //add to measurements array
            measurements[valid_data] = mV;
            valid_data += 1;
        }
    }
    //if one or more values are valid
    if (measurements[0] != 0) {
        for (int i = 0; i < valid_data; i++) {
            mean_measurements += measurements[i] / valid_data;
        }
        /* Debug: */
       // std::cout << "Mean: " << mean_measurements << endl << "Valid Data Collected:" << valid_data << endl;
        return mean_measurements;
    } else {
        //if no valid data was found, return -1 for mean
        return -1;
    }
}

/**
 * @brief calibrating the ph-Probe and save values of probe in system
 * @param i2c_address Address of probe
 * @param ph_Calibration_Val Calibration value
 * @param i2c_address_port port of the address
 * @return true if function was successful
 */
bool calibratePHProbe(int i2c_address, int ph_Calibration_Val, int i2c_address_port) {

    //Assumption: Values are normal distributed or follow a student distribution
    float calibration_Means[5];
    float calibration_Variance = 0.0;
    float calibration_mean = 0.0;
    float deviation = 0.0;

    for (int i = 0; i < 5; i++) {
        calibration_Means[i] = getMeanMeasurements(1000, i2c_address, i2c_address_port);
        calibration_mean += calibration_Means[i] / 5;
    }

    for (int i = 0; i < 5; i++) {
        //Calculate Variance
        calibration_Variance += (pow((calibration_Means[i] - calibration_mean), 2)) / 5;
    }
    //Calculate Deviation
    deviation = sqrt(calibration_Variance);

    //Debug: Cancel effect of deviation
    //deviation *= 0; 
    //Check deviation if higher than 15 means, that the variance of mV is
    //bigger than 200. This is not acceptable - error sources could be, 
    //defective probe or problems with the solution.     
    //Maybe it should be smaller thou or different value for different calibration
    //solutions
    
    if (deviation < 11 && deviation > 0.0) {
        //gives calibration value and calibrated solution
        std::cout << "<calibration><value>"<< calibration_mean << "</value><phSolution>" << ph_Calibration_Val << "</phSolution></calibration>" << endl;
        return true;
    } else {
        //Deviation was to high
        return false;
    }

}

/**
 * @brief Converts string to integers
 * @param i integer
 * @return string
 */
string intToString(int i) {
    ostringstream convert;
    convert << i;
    return convert.str();
}
/**
 * @brief getting the temperature in degree celsius
 * @return tempperature, that was measured by the assigned temp_probe
 */
float getTemperatureCelsius() {
    string line;
    string filename;
    string tmp;

    //Define Filename
    filename = temperature_Path;
    //Open File
    std::ifstream in(filename);

    //search for t=
    while (getline(in, line)) {
        tmp = "";
        if (strstr(line.c_str(), "t=")) {
            tmp = strstr(line.c_str(), "t=");
            tmp.erase(0, 2);
            if (tmp.length() > 1) {
                in.close();
                return strtof(tmp.c_str(), NULL) / 1000;
            }
        }
    }
    in.close();

    return -1000;
}

/**
 * @brief Measures data and returns milivolt for pre-defined ph solution
 * @param argc should be 4
 * @param argv Ph-value to calibrate & ph-probe address
 * @return 
 */
int main(int argc, char** argv) {

    if (argc != 4) {
        std::cout << "Error: You did not provide required arguments." << endl
                << "Usage: Aqualight-PhController-Calibrator TemperatureFile PhProbeI2CAddress PhProbeCalibration"
                << endl
                << "PhProbeI2CAddress has to be provided by decimal:decimal as address:port e.g.: 77:0. (0x4d:1)"
                << endl
                << "PhProbeCalibration is value, that has to be adjusted."
                << endl;
        return 0;
    }
    //Initialize variables
    //Temperature
    temperature_Path = argv[1];
    float temperature_C = getTemperatureCelsius();
    //I²C
    string str = argv[2];
    int pos = str.find_first_of(':');
    string i2c_FD = str.substr(0, pos);
    uint i2c_address = stoi(i2c_FD); //0x4d;
    string ph_Address = str.substr(pos + 1);
    uint i2c_port = stoi(ph_Address);                            
    //Knowing which ph-value we are measuring
    int ph_Value_Integer = atoi(argv[3]);

    //Setting Up Gpio
    wiringPiSetupGpio();
    //make wired_Address ready to be read from, just in case it's going to be needed
    wired_Address = wiringPiI2CSetup(i2c_address);
    return calibratePHProbe(i2c_address, ph_Value_Integer,i2c_port);
}

