
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

using namespace std;

/**
 * Global variable to set the temperaturePath of the electrode
 */
string temperature_Path;

/**
 * @brief mV of the probe
 * @param file_Descriptor
 * @return Voltage
 */
float get_Probe_mV(int ph_Probe_Address){
    //Loading phAdress    
    if(int raw = wiringPiI2CReadReg16(ph_Probe_Address, 0)){
        raw = raw >> 8 | ((raw << 8) &0xffff);
        if(raw > 0){
            return (((float) raw / 4096) * 3.3) * 1000;
        }
        else{
            return -1;
        }
    }
    else{
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }
    
}

/**
 * @brief gets the mean out of a number of measurements
 * @param measurement_size
 * @param ph_probe_Address
 * @return mean > 0 if enough data was collected, else -1 
 */
float getMeanMeasurements(int measurement_size,int ph_Probe_Address){
    float measurements[measurement_size]; 
    float mean_measurements = 0.0;
    float mV = 0.0;
    int valid_data = 0;
    
    for(int i = 0; i < measurement_size; i++){            
        //Gets mV from Probe    
        mV = get_Probe_mV(ph_Probe_Address);
        //if Probe Value is valid
        if(mV > 0){
            //add to measurements array
            measurements[valid_data] = mV;
            valid_data += 1;                
        }                        
    }
    //if one or more values are valid
    if(measurements[0] != 0){
            for(int i = 0; i < valid_data; i++){
                mean_measurements += measurements[i] / valid_data;
            }
            /* Debug: */
             std::cout << "Mean: " << mean_measurements << endl 
                    << "Valid Data Collected:" << valid_data << endl;
        return mean_measurements;
    }    
    else{
        //if no valid data was found, return -1 for mean
        return -1;
    }
}

/**
 * @brief calibrating the ph-Probe and save values of probe in system
 * @param ph_Probe_Address
 * @param ph_Calibration_Val
 * @return 
 */
bool calibratePHProbe(int ph_Probe_Address, int ph_Calibration_Val){
    
    //Assumption: Values are normal distributed or follow a student distribution
    float calibration_Means[5];
    float calibration_Variance = 0.0;
    float calibration_mean = 0.0; 
    float deviation = 0.0;
    
    for(int i = 0; i < 5; i++){
        calibration_Means[i] = getMeanMeasurements(65535, ph_Probe_Address);   
        calibration_mean += calibration_Means[i] / 5;
    }       
    
    for(int i = 0; i < 5; i++){
        //Calculate Variance
        calibration_Variance += (pow((calibration_Means[i] - calibration_mean),2))/5;
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
    std::cout << "Solution:" << ph_Calibration_Val << " Mean: " 
                << calibration_mean << " Variance: " << calibration_Variance 
                << " Deviation: " << deviation << endl;
    if(deviation < 11 & deviation > 0){        
        return true;
    }
    else{
        //Deviation was to high
        return false;
    }
        
}

/**
 * @brief Converts string to integers
 * @param i integer
 * @return string
 */
string intToString(int i){
    ostringstream convert;
    convert << i;
    return convert.str();
}

float getTemperatureCelsius(int i){        
    string line;
    string filename;
    string tmp;    
    
    //Define Filename
    filename = temperature_Path;    
    //Open File
    std::ifstream in(filename);   

    //search for t=
    while(getline(in, line)){
        tmp = "";
        if(strstr(line.c_str(), "t=")){
            tmp = strstr(line.c_str(), "t=");
            tmp.erase(0,2);
            if(tmp.length() > 1){
                in.close();
                return strtof(tmp.c_str(),NULL)/1000;
            }
        }
    }
    in.close();
    
    return -1000;
}

/**
 * @brief Main executeable
 * @param argc should be 2 
 * @param argv Ph-value to calibrate & ph-probe address
 * @return 
 */ 
int main(int argc, char** argv) {

    if(argc != 4){
        std::cout  << "Error: You did not provide required arguments." << endl 
                << "Usage: Aqualight-PhController-Calibrator TemperatureFile PhProbeI2CAddress PhProbeCalibration" 
                << endl
                << "PhProbeI2CAddress has to be provided by integer number of port. E.g. Port:0x4d"
                << endl
                << "PhProbeCalibration is value, that has to be adjusted."
                << endl;        
        return 0;
    }        
    //Initialize variables
    temperature_Path = argv[1];
    float temperature_C = getTemperatureCelsius(0);
    int ph_Probe_Address = atoi(argv[2]);
    int wired_Address; 
    int ph_Value_Integer = atoi(argv[3]);       
        
    //Setting Up Gpio
    wiringPiSetupGpio();
    //Access ph-Probe
    wired_Address = wiringPiI2CSetup(ph_Probe_Address);     

    return calibratePHProbe(wired_Address, ph_Value_Integer);   
}

