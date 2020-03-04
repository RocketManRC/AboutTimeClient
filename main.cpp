/*
    AboutTimeClient - A command line program to adjust the system clock. This can be
    used standalone to modify the clock by an offset or it can synchronize to
    an external time source connected via a serial port. The time source could be a
    real time clock (RTC) module and/or GPS.
 
    This program can be built and run on a Windows PC, a Linux box (including the
    Raspberry Pi) and on MacOS. The only requirment for building is a C++11 compiler.
 
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
 
    In order to set the clock this application has to be run with administor priviledges, this means
    run under a command prompt that has been opened to run as administrator on Windows and use sudo on MacOS or Linux.
 
    AboutTimeClient is Copyright (c) 2020 Rick MacDonald - MIT License:

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <iostream>
#include <time.h>
#include <string>
#include "ceSerial.h"
#include <stdio.h>

#ifdef ceWINDOWS

//#include <Windows.h>
//#include "stdafx.h"
#undef min
#undef max
#include "cxxopts.hpp"
//#include "SerialPort.h"
#include "ceSerial.h"

const __int64 UNIX_TIME_START = 0x019DB1DED53E8000; //January 1, 1970 (start of Unix epoch) in "ticks"
const double TICKS_PER_SECOND = 10000000.0; //a tick is 100ns

double GetSystemTimeAsUnixTimeDouble()
{
    //Get the number of seconds since January 1, 1970 12:00am UTC
    //Code released into public domain; no attribution required.

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft); //returns ticks in UTC

    //Copy the low and high parts of FILETIME into a LARGE_INTEGER
    //This is so we can access the full 64-bits as an Int64 without causing an alignment fault
    LARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    //Convert ticks since 1/1/1970 into seconds
    return (li.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

void SetSystemTimeFromUnixTime(double t)
{
    FILETIME ft;

    LARGE_INTEGER li;
    li.QuadPart = t * TICKS_PER_SECOND + UNIX_TIME_START;

    ft.dwLowDateTime = li.LowPart;
    ft.dwHighDateTime = li.HighPart;

    SYSTEMTIME st;

    FileTimeToSystemTime(&ft, &st);

    if(!SetSystemTime(&st))
        printf("SetSystemTime() failed, you need to run this as Administrator!\n");
}

#else // Linux or MacOS

#include <sys/time.h>
#include "cxxopts.hpp"

struct timeval tv;

double GetSystemTimeAsUnixTimeDouble()
{
    gettimeofday(&tv, NULL);
    
    double secs = tv.tv_sec + (tv.tv_usec / 1000000.0);

    return secs;
}

void SetSystemTimeFromUnixTime(double secs)
{
    tv.tv_sec = (int)secs;
    tv.tv_usec = (secs - (int)secs) * 1000000;
    
    if( settimeofday(&tv, NULL) == -1 )
    {
        perror("settimeofday");
        printf("You have to run this as root (use sudo)!\n");
    }
}

#endif

using namespace std;
using namespace ce;

int main(int argc, char *argv[])
{
    double offset = 0.0;
    bool init = false;
    string port = "";
    
    try
    {
        cxxopts::Options options(argv[0], "Timetool command line options\n");

        options.add_options()
            ("i,init", "False", cxxopts::value<bool>()->default_value("false"))
            ("p,port", "Port", cxxopts::value<vector<string>>(), "PORT")
            ("h,help", "Print help")
            ("o,offset", "Offset", cxxopts::value<float>());

        auto result = options.parse(argc, argv);

        if(result.count("help"))
        {
            cout << options.help({"", "Group"}) << endl;
            exit(0);
        }

        if(result.count("i"))
        {
          init = result["i"].as<bool>();
        }

        if(result.count("p"))
        {
            auto& ff = result["p"].as<vector<string>>();
            //cout << "Port " << endl;
            for (const auto& f : ff)
            {
                //cout << f << endl;
                port =  f;
            }
        }

        if(result.count("o"))
        {
            offset =  result["o"].as<float>();
        }
    }
    catch(const cxxopts::OptionException& e)
    {
        cout << "error parsing options: " << e.what() << endl;
        exit(1);
    }
    
    cout << "init: " <<  init << endl;
    cout << "port: " <<  (port != "" ? port : "n/a") << endl;
    cout << "offset: " <<  offset << endl;
    
#ifdef ceWINDOWS
    
    if( port != "" )
        port = "\\\\.\\" + port;
    
    ceSerial com(port,115200,8,'N',1); // Windows

#else // Linux or MacOS
    
    ceSerial com(port,115200,8,'N',1); // MacOS or Linux
    
#endif
    
    uint32_t t = 0;
    
    // If a serial port is specified then we are going to get reference time from there.
    // We will read the time fron the port for 5 seconds then set the local time from that,
    // including any offset specified. After that we will display the time
    // for 5 more seconds to see that it worked...
    
    if(port != "")
    {
        printf("Opening port %s.\n",com.GetPort().c_str());
        
        if(com.Open() == 0)
        {
            printf("OK.\n");
        }
        else
        {
            printf("Error.\n");
            return 1;
        }
        
        bool successFlag = true;
        int i = 1;
        char c = '\0';
        
        com.SetRTS(true); // ******** These two lines are critical on Windows for getting USB Serial
        com.SetDTR(true); // ******** to work with the Teensy
        
        // flush the serial buffer by trying to read for 2 seconds (Linux seems to need this this)
        double tStart = GetSystemTimeAsUnixTimeDouble();
        while(GetSystemTimeAsUnixTimeDouble() - tStart < 2.0)
            c = com.ReadChar(successFlag);
        
        // discard chars up to the first nl
        while(c != '\n')
            c = com.ReadChar(successFlag);

        string s = "";

        while(i < 11)
        {
            double secs;
            
            c = com.ReadChar(successFlag);
            
            if(successFlag)
            {
                if(c == 'S')
                {
                    if(i == 5 && init)
                    {
                        if(offset != 0.0)
                        {
                            secs = t * 1.0 + offset;
                        }
                        else
                        {
                            secs = t * 1.0;
                        }
                    
                        printf("Setting time to: %lf\n", secs);
                        
                        SetSystemTimeFromUnixTime(secs);
                    }
                
                    secs = GetSystemTimeAsUnixTimeDouble();
                    printf("Current unix time in secs: %lf, i: %d\n", secs, i);
                }
                
                s.append(1, c);
        
                if(c == '\n')
                {
                    if(s.find ("SQW ") != string::npos)
                    {
                        string ss = s.substr(4, 10);
                    
                        int n = stoi(ss);
                    
                        t = n + 1; // set the clock 1 second later than now next time (to sync with the time source)
                    
                        i++;
                    
                        cout << n << endl << flush;
                    }
                    else
                        cout << s << flush;
                
                    s = "";
                }
            }
        }
    
        printf("Closing port %s.\n",com.GetPort().c_str());
        com.Close();
    }
    else if(offset !=  0.0 &&  init)
    {
        double d = GetSystemTimeAsUnixTimeDouble();

        printf("Current time: %lf\n", d);

        if(offset !=  0.0 && init)
        {
            d += offset;
            
            printf("Adjusting time with offset %lf to %lf\n", offset, d);
            
            SetSystemTimeFromUnixTime(d);
        }

        d = GetSystemTimeAsUnixTimeDouble();

        printf("%lf\n", d);
    }
    
    return 0;
}
    

