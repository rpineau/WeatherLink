//
//  WeatherLink.h
//  CWeatherLink
//
//  Created by Rodolphe Pineau on 2021-04-13
//  WeatherLink X2 plugin

#ifndef __WeatherLink__
#define __WeatherLink__
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#ifdef SB_WIN_BUILD
#include <time.h>
#endif


#ifndef SB_WIN_BUILD
#include <curl/curl.h>
#else
#include "win_includes/curl.h"
#endif

#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cmath>
#include <future>
#include <mutex>


#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"

#include "json.hpp"
using json = nlohmann::json;

#define PLUGIN_VERSION      1.0

//  #define PLUGIN_DEBUG 3

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 500
#define MAX_READ_WAIT_TIMEOUT 25
#define NB_RX_WAIT 10

#define inHg_to_mBar  33.86389

// error codes
enum WeatherLinkErrors {PLUGIN_OK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT, PARSE_FAILED};

enum WeatherLinkWindUnits {KPH=0, MPS, MPH};

class CWeatherLink
{
public:
    CWeatherLink();
    ~CWeatherLink();

    int         Connect();
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }
    void        getFirmware(std::string &sFirmware);

    int         getWindSpeedUnit(int &nUnit);
    int         getRain();
    double      getSkyIr();
    double      getAmbientTemp();
    int         isSafe(bool &bSafe);

    std::mutex  m_DevAccessMutex;
    int         getData();

    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, void* data);

    void getIpAddress(std::string &IpAddress);
    void setIpAddress(std::string IpAddress);

    void getTcpPort(int &nTcpPort);
    void setTcpPort(int nTcpPort);

    double getAmbianTemp();
    double getWindSpeed();
    double getHumidity();
    double getDewPointTemp();
    double getRainFlag();
    double getBarometricPressure();
    double getWindCondition();
    double getRainCondition();

#ifdef PLUGIN_DEBUG
    void  log(const std::string sLogLine);
#endif

protected:

    bool            m_bIsConnected;
    SerXInterface   *m_pSerx;
    std::string     m_sFirmware;
    std::string     m_sModel;
    double          m_dFirmwareVersion;

    CURL            *m_Curl;
    std::string     m_sBaseUrl;

    std::string     m_sIpAddress;
    int             m_nTcpPort;

    bool                m_ThreadsAreRunning;
    std::promise<void> *m_exitSignal;
    std::future<void>   m_futureObj;
    std::thread         m_th;

    // weatherlink variables
    std::atomic<double> m_dTemp;
    std::atomic<double> m_dWindSpeed;
    std::atomic<double> m_dPercentHumdity;
    std::atomic<double> m_dDewPointTemp;
    std::atomic<double> m_dRainFlag;
    // std::atomic<int>    m_nWetFlag;
    std::atomic<double> m_dBarometricPressure;
    std::atomic<double> m_dWindCondition;
    std::atomic<double> m_dRainCondition;
    // daylightCondition
    
    bool            m_bSafe;
    int             doGET(std::string sCmd, std::string &sResp);
    std::string     cleanupResponse(const std::string InString, char cSeparator);
    int             getModelName();
    int             getFirmwareVersion();
    
    int             parseType1(json jResp);
    int             parseType2(json jResp);
    int             parseType3(json jResp);
    int             parseType4(json jResp);

    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);
    std::string     findField(std::vector<std::string> &svFields, const std::string& token);


#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
    std::string m_sPlatform;
#endif

};

#endif
