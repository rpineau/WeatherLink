
#include "x2weatherstation.h"

X2WeatherStation::X2WeatherStation(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;		
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;
    m_nPrivateISIndex               = nInstanceIndex;

	m_bLinked = false;
    if (m_pIniUtil) {
        char szIpAddress[128];
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_IP, "192.168.0.10", szIpAddress, 128);
        m_WeatherLink.setIpAddress(std::string(szIpAddress));
        m_WeatherLink.setTcpPort(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_PORT, 80));
    }
}

X2WeatherStation::~X2WeatherStation()
{
	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
}

int	X2WeatherStation::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;

	if (!strcmp(pszName, LinkInterface_Name))
		*ppVal = (LinkInterface*)this;
	else if (!strcmp(pszName, WeatherStationDataInterface_Name))
        *ppVal = dynamic_cast<WeatherStationDataInterface*>(this);
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);


	return SB_OK;
}

int X2WeatherStation::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    char szTmpBuf[LOG_BUFFER_SIZE];
    std::string sIpAddress;
    int nTcpPort;

    if (NULL == ui)
        return ERR_POINTER;

    m_WeatherLink.log("[execModalSettingsDialog] trying to load WeatherLink.ui");

    if ((nErr = ui->loadUserInterface("WeatherLink.ui", deviceType(), m_nPrivateISIndex))) {
        return nErr;
    }

    m_WeatherLink.log("[execModalSettingsDialog] WeatherLink.ui loaded");

    if (NULL == (dx = uiutil.X2DX())) {
        return ERR_POINTER;
    }
    X2MutexLocker ml(GetMutex());

    m_WeatherLink.log("[execModalSettingsDialog] GetMutex called");

    m_WeatherLink.getIpAddress(sIpAddress);
    dx->setPropertyString("IPAddress", "text", sIpAddress.c_str());
    m_WeatherLink.getTcpPort(nTcpPort);
    snprintf(szTmpBuf, LOG_BUFFER_SIZE, "%d", nTcpPort );
    dx->setPropertyString("tcpPort", "text", szTmpBuf);

    if(m_bLinked) { // we can't change the value for the ip and port if we're connected
        dx->setEnabled("IPAddress", false);
        dx->setEnabled("tcpPort", false);
        dx->setEnabled("pushButton", true);
    }
    else {
        dx->setEnabled("IPAddress", true);
        dx->setEnabled("tcpPort", true);
        dx->setEnabled("pushButton", false);
    }


    m_WeatherLink.log("[execModalSettingsDialog] calling ui->exec");

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        if(!m_bLinked) {
            // save the values to persistent storage
            dx->propertyString("IPAddress", "text", szTmpBuf, 128);
            nErr |= m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_IP, szTmpBuf);
            m_WeatherLink.setIpAddress(std::string(szTmpBuf));

            dx->propertyString("tcpPort", "text", szTmpBuf, 128);
            nErr |= m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_PORT, atoi(szTmpBuf));
            m_WeatherLink.setTcpPort( atoi(szTmpBuf));
        }
    }
    return nErr;
}

void X2WeatherStation::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
}

void X2WeatherStation::driverInfoDetailedInfo(BasicStringInterface& str) const
{
}
double X2WeatherStation::driverInfoVersion(void) const							
{
	return PLUGIN_VERSION;
}

void X2WeatherStation::deviceInfoNameShort(BasicStringInterface& str) const
{
    str = "WeatherLink";
}

void X2WeatherStation::deviceInfoNameLong(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(m_bLinked) {
        str = "N/A";
        std::string sFirmware;
        X2MutexLocker ml(GetMutex());
        m_WeatherLink.getFirmware(sFirmware);
        str = sFirmware.c_str();
    }
    else
        str = "N/A";

}

void X2WeatherStation::deviceInfoModel(BasicStringInterface& str)
{
    deviceInfoNameShort(str);
}

int	X2WeatherStation::establishLink(void)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    nErr = m_WeatherLink.Connect();
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

	return SB_OK;
}
int	X2WeatherStation::terminateLink(void)
{
    m_WeatherLink.Disconnect();

	m_bLinked = false;
	return SB_OK;
}


bool X2WeatherStation::isLinked(void) const
{
	return m_bLinked;
}


int X2WeatherStation::weatherStationData(double& dSkyTemp,
                                         double& dAmbTemp,
                                         double& dSenT,
                                         double& dWind,
                                         int& nPercentHumdity,
                                         double& dDewPointTemp,
                                         int& nRainHeaterPercentPower,
                                         int& nRainFlag,
                                         int& nWetFlag,
                                         int& nSecondsSinceGoodData,
                                         double& dVBNow,
                                         double& dBarometricPressure,
                                         WeatherStationDataInterface::x2CloudCond& cloudCondition,
                                         WeatherStationDataInterface::x2WindCond& windCondition,
                                         WeatherStationDataInterface::x2RainCond& rainCondition,
                                         WeatherStationDataInterface::x2DayCond& daylightCondition,
                                         int& nRoofCloseThisCycle //The weather station hardware determined close or not (boltwood hardware says cloudy is not close)
)
{
    int nErr = SB_OK;
    double dWindCond;
    std::stringstream tmp;

    if(!m_bLinked)
        return ERR_NOLINK;


    X2MutexLocker ml(GetMutex());

    nSecondsSinceGoodData = 1; // was 900 , aka 15 minutes ?
    dAmbTemp = m_WeatherLink.getAmbianTemp();
    dWind = m_WeatherLink.getWindSpeed();
	nPercentHumdity = int(m_WeatherLink.getHumidity());
	dDewPointTemp = m_WeatherLink.getDewPointTemp();
	nRainFlag = m_WeatherLink.getRainFlag()>=1?2:0;
	nWetFlag = nRainFlag;

    dBarometricPressure = m_WeatherLink.getBarometricPressure();

    dWindCond = m_WeatherLink.getWindCondition();

    windCondition = x2WindCond::windCalm;
    if (dWindCond >= 20) {
        windCondition = x2WindCond::windWindy;
    }
    if (dWindCond >= 40) {
        windCondition = x2WindCond::windVeryWindy;
    }

    rainCondition = nRainFlag==0?(x2RainCond::rainDry): (x2RainCond::rainRain);
	nRoofCloseThisCycle = nRainFlag==0?0:1;

	return nErr;
}

WeatherStationDataInterface::x2WindSpeedUnit X2WeatherStation::windSpeedUnit()
{
    WeatherStationDataInterface::x2WindSpeedUnit nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
    int WeatherLinkUnit;
    std::stringstream tmp;

    WeatherLinkUnit = m_WeatherLink.getWindSpeedUnit(WeatherLinkUnit);

    switch(WeatherLinkUnit) {
        case KPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
            break;
        case MPS:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMps;
            break;
        case MPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMph;
            break;
    }

    return nUnit ;
}
