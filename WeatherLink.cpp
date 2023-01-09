//
//  WeatherLink.cpp
//  CWeatherLink
//
//  Created by Rodolphe Pineau on 2021-04-13
//  WeatherLink X2 plugin

#include "WeatherLink.h"

void threaded_poller(std::future<void> futureObj, CWeatherLink *WeatherLinkControllerObj)
{
    while (futureObj.wait_for(std::chrono::milliseconds(5000)) == std::future_status::timeout) {
        if(WeatherLinkControllerObj->m_DevAccessMutex.try_lock()) {
            WeatherLinkControllerObj->getData();
            WeatherLinkControllerObj->m_DevAccessMutex.unlock();
        }
        else {
            std::this_thread::yield();
        }
    }
}

CWeatherLink::CWeatherLink()
{
    // set some sane values
    m_pSerx = NULL;
    m_bIsConnected = false;
    m_ThreadsAreRunning = false;
    m_sIpAddress.clear();
    m_nTcpPort = 0;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_WeatherLink.txt";
    m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_WeatherLink.txt";
    m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_WeatherLink.txt";
    m_sPlatform = "macOS";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CWeatherLink] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CWeatherLink] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

    curl_global_init(CURL_GLOBAL_ALL);
    m_Curl = nullptr;

}

CWeatherLink::~CWeatherLink()
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [~CWeatherLink] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_bIsConnected) {
        Disconnect();
    }

    curl_global_cleanup();

#ifdef    PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

int CWeatherLink::Connect()
{
    int nErr = SB_OK;
    std::string sDummy;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_sIpAddress.empty())
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Base url = " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

    m_Curl = curl_easy_init();

    if(!m_Curl) {
        m_Curl = nullptr;
        return ERR_CMDFAILED;
    }

    m_bIsConnected = true;

    
    nErr = getData();
    if (nErr) {
        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;
        return nErr;
    }
    if(!m_ThreadsAreRunning) {
        m_exitSignal = new std::promise<void>();
        m_futureObj = m_exitSignal->get_future();
        m_th = std::thread(&threaded_poller, std::move(m_futureObj), this);
        m_ThreadsAreRunning = true;
    }


    return nErr;
}


void CWeatherLink::Disconnect()
{
    const std::lock_guard<std::mutex> lock(m_DevAccessMutex);

    if(m_bIsConnected) {
        if(m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Waiting for threads to exit." << std::endl;
            m_sLogFile.flush();
#endif
            m_exitSignal->set_value();
            m_th.join();
            delete m_exitSignal;
            m_exitSignal = nullptr;
            m_ThreadsAreRunning = false;
        }

        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;

#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Disconnected." << std::endl;
        m_sLogFile.flush();
#endif
    }
}


void CWeatherLink::getFirmware(std::string &sFirmware)
{
    sFirmware.assign(m_sFirmware);
}


int CWeatherLink::getWindSpeedUnit(int &nUnit)
{
    int nErr = PLUGIN_OK;
    nUnit = KPH;
    return nErr;
}


int CWeatherLink::isSafe(bool &bSafe)
{
    int nErr = PLUGIN_OK;
    nErr = getData();
    bSafe = m_bSafe;
    return nErr;
}


int CWeatherLink::doGET(std::string sCmd, std::string &sResp)
{
    int nErr = PLUGIN_OK;
    CURLcode res;
    std::string response_string;
    std::string header_string;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Called." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Doing get on " << sCmd << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Full get url " << (m_sBaseUrl+sCmd) << std::endl;
    m_sLogFile.flush();
#endif

    res = curl_easy_setopt(m_Curl, CURLOPT_URL, (m_sBaseUrl+sCmd).c_str());
    if(res != CURLE_OK) { // if this fails no need to keep going
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] curl_easy_setopt Error = " << res << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    curl_easy_setopt(m_Curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_POST, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(m_Curl, CURLOPT_HEADERDATA, &header_string);
    curl_easy_setopt(m_Curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(m_Curl, CURLOPT_CONNECTTIMEOUT, 3); // 3 seconds timeout on connect

    // Perform the request, res will get the return code
    res = curl_easy_perform(m_Curl);
    // Check for errors
    if(res != CURLE_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Error = " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] response = " << response_string << std::endl;
    m_sLogFile.flush();
#endif

    sResp.assign(cleanupResponse(response_string,'\n'));

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] sResp = " << sResp << std::endl;
    m_sLogFile.flush();
#endif
    return nErr;
}

size_t CWeatherLink::writeFunction(void* ptr, size_t size, size_t nmemb, void* data)
{
    ((std::string*)data)->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

std::string CWeatherLink::cleanupResponse(const std::string InString, char cSeparator)
{
    std::string sSegment;
    std::vector<std::string> svFields;

    if(!InString.size()) {
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [cleanupResponse] InString is empty." << std::endl;
        m_sLogFile.flush();
#endif
        return InString;
    }


    std::stringstream ssTmp(InString);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        if(sSegment.find("<!-") == -1)
            svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        return std::string("");
    }

    sSegment.clear();
    for( std::string s : svFields)
        sSegment.append(trim(s,"\n\r "));
    return sSegment;
}


#pragma mark - Getter / Setter
double CWeatherLink::getAmbianTemp()
{
    return m_dTemp;
}

double CWeatherLink::getWindSpeed()
{
    return m_dWindSpeed;
}

double CWeatherLink::getHumidity()
{
    return m_dPercentHumdity;
}

double CWeatherLink::getDewPointTemp()
{
    return m_dDewPointTemp;
}

double CWeatherLink::getRainFlag()
{
    return m_dRainFlag;
}

double CWeatherLink::getBarometricPressure()
{
    return m_dBarometricPressure;
}

double CWeatherLink::getWindCondition()
{
    return m_dWindCondition;
}

double CWeatherLink::getRainCondition()
{
    return m_dRainCondition;
}


int CWeatherLink::getData()
{
    int nErr = PLUGIN_OK;
    json jResp;
    std::string response_string;
    std::string weatherLinkError;

    if(!m_bIsConnected || !m_Curl)
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] Called." << std::endl;
    m_sLogFile.flush();
#endif

    // do http GET request to PLC got get current Az or Ticks .. TBD
    nErr = doGET("/v1/current_conditions", response_string);
    if(nErr) {
        return ERR_CMDFAILED;
    }

    // process response_string
    try {
        jResp = json::parse(response_string);
        if(jResp.at("error").is_null()) {
            for (auto& jElement : jResp.at("data").at("conditions").items()) {
                if(jElement.value().at("data_structure_type").get<int>() == 1) {
                    parseType1(jElement.value());
                }
                if(jElement.value().at("data_structure_type").get<int>() == 2) {
                    parseType2(jElement.value());
                }
                if(jElement.value().at("data_structure_type").get<int>() == 3) {
                    parseType3(jElement.value());
                }
                if(jElement.value().at("data_structure_type").get<int>() == 4) {
                    parseType4(jElement.value());
                }
            }
            m_sFirmware = "WeatherLink Live "+jResp.at("data").at("did").get<std::string>();
        }
        else {
            weatherLinkError = jResp.at("error").get<std::string>();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] Weatherlink error : " << weatherLinkError << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception response : " << response_string << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dTemp                : " << m_dTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dWindSpeed           : " << m_dWindSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dPercentHumdity      : " << m_dPercentHumdity << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dDewPointTemp        : " << m_dDewPointTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dRainFlag            : " << m_dRainFlag << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dBarometricPressure  : " << m_dBarometricPressure << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dWindCondition       : " << m_dWindCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dRainCondition       : " << m_dRainCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_sFirmware            : " << m_sFirmware << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


int CWeatherLink::parseType1(json jData)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseType1] json data : " << jData << std::endl;
    m_sLogFile.flush();
#endif
    m_dTemp =  (jData.at("temp").get<double>() -32)/1.8; // converted to Celsius
    m_dWindSpeed = jData.at("wind_speed_avg_last_2_min").get<double>()*1.60934; // Converted to kph
    m_dPercentHumdity = jData.at("hum").get<double>();
    m_dDewPointTemp = (jData.at("dew_point").get<double>() -32)/1.8;  // converted to Celsius
    m_dRainFlag =  jData.at("rain_rate_hi").get<double>() * 2.54; // convert to cm/h -> we might need a /100 based on test done and weather link web site data
    m_dWindCondition = jData.at("wind_speed_hi_last_10_min").get<double>()*1.60934; // Converted to kph
    m_dRainCondition = jData.at("rainfall_last_15_min").get<double>() * 2.54; // convert to cm -> we might need a /100 based on test done and weather link web site data
    return nErr;
}

int CWeatherLink::parseType2(json jData)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseType2] json data : " << jData << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CWeatherLink::parseType3(json jData)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseType3] json data : " << jData << std::endl;
    m_sLogFile.flush();
#endif

    m_dBarometricPressure = jData.at("bar_sea_level").get<double>() * inHg_to_mBar;
    
    return nErr;
}

int CWeatherLink::parseType4(json jData)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseType4] json data : " << jData << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


#pragma mark - Getter / Setter


void CWeatherLink::getIpAddress(std::string &IpAddress)
{
    IpAddress = m_sIpAddress;
}

void CWeatherLink::setIpAddress(std::string IpAddress)
{
    m_sIpAddress = IpAddress;
    if(m_nTcpPort!=80 && m_nTcpPort!=443) {
        m_sBaseUrl = "http://"+m_sIpAddress+":"+std::to_string(m_nTcpPort);
    }
    else if (m_nTcpPort==443) {
        m_sBaseUrl = "https://"+m_sIpAddress;
    }
    else {
        m_sBaseUrl = "http://"+m_sIpAddress;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setIpAddress] New base url : " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

}

void CWeatherLink::getTcpPort(int &nTcpPort)
{
    nTcpPort = m_nTcpPort;
}

void CWeatherLink::setTcpPort(int nTcpPort)
{
    m_nTcpPort = nTcpPort;
    if(m_nTcpPort!=80) {
        m_sBaseUrl = "http://"+m_sIpAddress+":"+std::to_string(m_nTcpPort);
    }
    else {
        m_sBaseUrl = "http://"+m_sIpAddress;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setTcpPort] New base url : " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif
}

std::string& CWeatherLink::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CWeatherLink::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CWeatherLink::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

std::string CWeatherLink::findField(std::vector<std::string> &svFields, const std::string& token)
{
    for(int i=0; i<svFields.size(); i++){
        if(svFields[i].find(token)!= -1) {
            return svFields[i];
        }
    }
    return std::string();
}


#ifdef PLUGIN_DEBUG
void CWeatherLink::log(const std::string sLogLine)
{
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [log] " << sLogLine << std::endl;
    m_sLogFile.flush();

}

const std::string CWeatherLink::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif

