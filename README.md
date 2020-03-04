# AboutTimeClient
A command line program to adjust the system clock on MacOS, Linux and Windows. This can be used standalone to modify the clock by an offset or it can synchronize to an external time source connected via a serial port. The time source could be a real time clock (RTC) module and/or GPS.

# Background

# Building the Application
The only requirment for building is a C++11 compiler.
 
I use the following two platform independent libraries:

    serial - https://github.com/yan9a/serial (serial port library)
    cxxopts - https://github.com/jarro2783/cxxopts (command line parsing library)
 
Both of those libraries are licensed under the MIT license and are included in the repository.
 
Building on Windows with MS tools (from the developer command prompt):

    cl /EHsc main.cpp ceSerial.cpp /link /OUT:AboutTimeClient.exe
    
Building on Windows with mingw-x64 (from the command prompt after path is updated):

    g++ main.cpp ceSerial.cpp -o AboutTimeClient.exe -std=c++11
    
Building on MacOS or Linux (including Raspian):

    g++ main.cpp ceSerial.cpp -o AboutTimeClient -std=c++11
        
# Running the Application
In order to set the clock this application has to be run with administor priviledges, this means run under a command prompt that has been opened to run as administrator on Windows and use sudo on MacOS or Linux.