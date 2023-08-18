
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

    int nCloseOnWindy;

	m_bLinked = false;
    m_bUiEnabled = false;

    if (m_pIniUtil) {
        char szIpAddress[128];
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_IP, "192.168.0.10", szIpAddress, 128);
        m_WeatherLink.setIpAddress(std::string(szIpAddress));
        m_WeatherLink.setTcpPort(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_PORT, 80));
        m_dWindyThreshold = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_WINDY, 20);
        m_dVeryWindyThreshold = m_pIniUtil->readDouble(PARENT_KEY, CHILD_KEY_VERY_WINDY, 30);
        nCloseOnWindy = m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_CLOSE_ON_WINDY,0);
        m_bCloseOnWindy = nCloseOnWindy?true:false;
        
        m_WeatherLink.setTempTxId(m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_TxIdTemp,1));
        m_WeatherLink.setWindTxId(m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_TxIdWind,1));
        m_WeatherLink.setRainTxId(m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_TxIdRain,1));
        m_WeatherLink.setHumTxId(m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_TxIdHum,1));
        m_WeatherLink.setDewTxId(m_pIniUtil->readInt(PARENT_KEY,CHILD_KEY_TxIdDew,1));
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
    std::stringstream ssTmp;
    std::string sIpAddress;
    int nTcpPort;
    int nId;
    int j;
    std::vector<int> txIds;
    std::vector<int>::iterator itr;

    m_bUiEnabled = false;

    if (NULL == ui)
        return ERR_POINTER;
    if ((nErr = ui->loadUserInterface("WeatherLink.ui", deviceType(), m_nPrivateISIndex))) {
        return nErr;
    }

    if (NULL == (dx = uiutil.X2DX())) {
        return ERR_POINTER;
    }
    X2MutexLocker ml(GetMutex());

    m_WeatherLink.getIpAddress(sIpAddress);
    dx->setPropertyString("IPAddress", "text", sIpAddress.c_str());
    m_WeatherLink.getTcpPort(nTcpPort);
    ssTmp << nTcpPort;
    dx->setPropertyString("tcpPort", "text", ssTmp.str().c_str());


    if(m_bLinked) {
        m_WeatherLink.getTxIds();

        // we can't change the value for the ip and port if we're connected
        dx->setEnabled("IPAddress", false);
        dx->setEnabled("tcpPort", false);
        dx->setEnabled("pushButton", true);
        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getAmbianTemp() << " C";
        dx->setPropertyString("temperature", "text", ssTmp.str().c_str());

        txIds = m_WeatherLink.getTempTxIds();
        for( int i : txIds) {
            dx->comboBoxAppendString("comboBox",std::to_string(i).c_str());
        }
        nId = m_WeatherLink.getTempTxId();
        itr = std::find(txIds.begin(), txIds.end(), nId);
        j = (int) std::distance(txIds.begin(), itr);
        dx->setCurrentIndex("comboBox",j);

        txIds = m_WeatherLink.getWindTxIds();
        for( int i : txIds) {
            dx->comboBoxAppendString("comboBox_2",std::to_string(i).c_str());
        }
        nId = m_WeatherLink.getWindTxId();
        itr = std::find(txIds.begin(), txIds.end(), nId);
        j = (int) std::distance(txIds.begin(), itr);
        dx->setCurrentIndex("comboBox_2",j);

        txIds = m_WeatherLink.getRainTxIds();
        for( int i : txIds) {
            dx->comboBoxAppendString("comboBox_3",std::to_string(i).c_str());
        }
        nId = m_WeatherLink.getRainTxId();
        itr = std::find(txIds.begin(), txIds.end(), nId);
        j = (int) std::distance(txIds.begin(), itr);
        dx->setCurrentIndex("comboBox_3",j);

        txIds = m_WeatherLink.getHumTxIds();
        for( int i : txIds) {
            dx->comboBoxAppendString("comboBox_4",std::to_string(i).c_str());
        }
        nId = m_WeatherLink.getHumTxId();
        itr = std::find(txIds.begin(), txIds.end(), nId);
        j = (int) std::distance(txIds.begin(), itr);
        dx->setCurrentIndex("comboBox_4",j);

        txIds = m_WeatherLink.getDewTxIds();
        for( int i : txIds) {
            dx->comboBoxAppendString("comboBox_5",std::to_string(i).c_str());
        }
        nId = m_WeatherLink.getDewTxId();
        itr = std::find(txIds.begin(), txIds.end(), nId);
        j = (int) std::distance(txIds.begin(), itr);
        dx->setCurrentIndex("comboBox_5",j);

        
        std::stringstream().swap(ssTmp);
        ssTmp<< std::dec << m_WeatherLink.getHumidity() << " %";
        dx->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getDewPointTemp() << " C";
        dx->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getBarometricPressure() << " mbar";
        dx->setPropertyString("pressure", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindSpeed() << " km/h";
        dx->setPropertyString("windSpeed", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindCondition() << " km/h";
        dx->setPropertyString("windSpeed10min", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getRainCondition() << " cm";
        dx->setPropertyString("rainfallLast15Min", "text", ssTmp.str().c_str());
    }
    else {
        dx->setEnabled("IPAddress", true);
        dx->setEnabled("tcpPort", true);
        dx->setEnabled("pushButton", false);
        dx->setEnabled("comboBox", false);
        dx->setEnabled("comboBox_2", false);
        dx->setEnabled("comboBox_3", false);
        dx->setEnabled("comboBox_4", false);
        dx->setEnabled("comboBox_5", false);
    }

    if(m_bCloseOnWindy) {
        dx->setChecked("checkBox", 1);
    }
    else {
        dx->setChecked("checkBox", 0);
    }

    dx->setPropertyDouble("WindyThreshold", "value", m_dWindyThreshold);
    dx->setPropertyDouble("VeryWindyThreshold", "value", m_dVeryWindyThreshold);

    m_bUiEnabled = true;

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
        dx->propertyDouble("WindyThreshold", "value", m_dVeryWindyThreshold);
        dx->propertyDouble("VeryWindyThreshold", "value", m_dVeryWindyThreshold);
        m_bCloseOnWindy = (dx->isChecked("checkBox") == 1);
        m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_WINDY, m_dWindyThreshold);
        m_pIniUtil->writeDouble(PARENT_KEY, CHILD_KEY_VERY_WINDY, m_dVeryWindyThreshold);
        m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_CLOSE_ON_WINDY, m_bCloseOnWindy?1:0);

        if(m_bLinked) {
            nId = dx->currentIndex("comboBox");
            txIds = m_WeatherLink.getTempTxIds();
            m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TxIdTemp, txIds.at(nId));

            nId = dx->currentIndex("comboBox_2");
            txIds = m_WeatherLink.getWindTxIds();
            m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TxIdWind, txIds.at(nId));

            nId = dx->currentIndex("comboBox_3");
            txIds = m_WeatherLink.getRainTxIds();
            m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TxIdRain, txIds.at(nId));

            nId = dx->currentIndex("comboBox_4");
            txIds = m_WeatherLink.getHumTxIds();
            m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TxIdHum, txIds.at(nId));

            nId = dx->currentIndex("comboBox_5");
            txIds = m_WeatherLink.getDewTxIds();
            m_pIniUtil->writeInt(PARENT_KEY, CHILD_KEY_TxIdDew, txIds.at(nId));
        }
    }
    return nErr;
}

void X2WeatherStation::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    std::stringstream ssTmp;
    int nIdx;
    std::vector<int> txIds;

    // the test for m_bUiEnabled is done because even if the UI is not displayed we get events on the comboBox changes when we fill it.
    if(!m_bLinked | !m_bUiEnabled)
        return;


    if (!strcmp(pszEvent, "on_timer") && m_bLinked) {
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getAmbianTemp() << " C";
        uiex->setPropertyString("temperature", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::dec << m_WeatherLink.getHumidity() << " %";
        uiex->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getDewPointTemp() << " C";
        uiex->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getBarometricPressure() << " mbar";
        uiex->setPropertyString("pressure", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindSpeed() << " km/h";
        uiex->setPropertyString("windSpeed", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindCondition() << " km/h";
        uiex->setPropertyString("windSpeed10min", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getRainCondition() << " cm";
        uiex->setPropertyString("rainfallLast15Min", "text", ssTmp.str().c_str());
    }

    else if (!strcmp(pszEvent, "on_comboBox_currentIndexChanged")) {
        nIdx = uiex->currentIndex("comboBox");
        txIds = m_WeatherLink.getTempTxIds();
        m_WeatherLink.setTempTxId(txIds.at(nIdx));
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getAmbianTemp() << " C";
        uiex->setPropertyString("temperature", "text", ssTmp.str().c_str());
    }

    else if (!strcmp(pszEvent, "on_comboBox_2_currentIndexChanged")) {
        nIdx = uiex->currentIndex("comboBox_2");
        txIds = m_WeatherLink.getWindTxIds();
        m_WeatherLink.setWindTxId(txIds.at(nIdx));
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindSpeed() << " km/h";
        uiex->setPropertyString("windSpeed", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getWindCondition() << " km/h";
        uiex->setPropertyString("windSpeed10min", "text", ssTmp.str().c_str());
    }

    else if (!strcmp(pszEvent, "on_comboBox_3_currentIndexChanged")) {
        nIdx = uiex->currentIndex("comboBox_3");
        txIds = m_WeatherLink.getRainTxIds();
        m_WeatherLink.setRainTxId(txIds.at(nIdx));
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getRainCondition() << " cm";
        uiex->setPropertyString("rainfallLast15Min", "text", ssTmp.str().c_str());
    }

    else if (!strcmp(pszEvent, "on_comboBox_4_currentIndexChanged")) {
        nIdx = uiex->currentIndex("comboBox_4");
        txIds = m_WeatherLink.getHumTxIds();
        m_WeatherLink.setHumTxId(txIds.at(nIdx));
        ssTmp<< std::dec << m_WeatherLink.getHumidity() << " %";
        uiex->setPropertyString("humidity", "text", ssTmp.str().c_str());
    }

    else if (!strcmp(pszEvent, "on_comboBox_5_currentIndexChanged")) {
        nIdx = uiex->currentIndex("comboBox_5");
        txIds = m_WeatherLink.getDewTxIds();
        m_WeatherLink.setDewTxId(txIds.at(nIdx));
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherLink.getDewPointTemp() << " C";
        uiex->setPropertyString("dewPoint", "text", ssTmp.str().c_str());
    }
}

void X2WeatherStation::driverInfoDetailedInfo(BasicStringInterface& str) const
{
    str = "WeatherLink Live X2 plugin for Davis weather station by Rodolphe Pineau";
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

	return nErr;
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

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nSecondsSinceGoodData = 1; // was 900 , aka 15 minutes ?
    dAmbTemp = m_WeatherLink.getAmbianTemp();
    dWind = m_WeatherLink.getWindSpeed();
	nPercentHumdity = int(m_WeatherLink.getHumidity());
	dDewPointTemp = m_WeatherLink.getDewPointTemp();
	nRainFlag = m_WeatherLink.getRainFlag()>0?2:0;
	nWetFlag = nRainFlag;

    dBarometricPressure = m_WeatherLink.getBarometricPressure();

    dWindCond = m_WeatherLink.getWindCondition();

    windCondition = x2WindCond::windCalm;

    if (dWindCond >= m_dWindyThreshold) {
        windCondition = x2WindCond::windWindy;
    }

    if (dWindCond >= m_dVeryWindyThreshold) {
        windCondition = x2WindCond::windVeryWindy;
    }

    rainCondition = nRainFlag==0?(x2RainCond::rainDry): (x2RainCond::rainRain);
	nRoofCloseThisCycle = nRainFlag==0?0:1;
    if(!nRoofCloseThisCycle) {
        if ((m_bCloseOnWindy && windCondition==x2WindCond::windWindy) || windCondition==x2WindCond::windVeryWindy )  {
            nRoofCloseThisCycle = 1;
        }
    }

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
