#include "orbbec_sensors.h"

Sensors::Sensors()
{
    try {
        // Query all connected device list
        m_deviceList = m_context->queryDeviceList();

        if (m_deviceStringList.size() == 0) {
            int devCount = m_deviceList->deviceCount();
            for (int i = 0; i < devCount; i++) {
                auto device = m_deviceList->getDevice(i);
                auto deviceInfo = device->getDeviceInfo();

                std::string deviceName = deviceInfo->name();
                deviceName.append(" #SN:").append(deviceInfo->serialNumber()).append(" USB:").append(deviceInfo->usbType());
                m_deviceStringList.push_back(deviceName);
                device.reset();
            }
        }
    }
    catch (ob::Error& e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        exit(EXIT_FAILURE);
    }
}

Sensors::~Sensors()
{
    deinitCurSensor();
}

int Sensors::initCurSensor(const int deviceIndex)
{
    try {
        ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_INFO);

        // Obtain device and create pipeline
        m_device = m_deviceList->getDevice(deviceIndex);
        m_pipeline = std::make_shared<ob::Pipeline>(m_device);
        // Configure which streams to enable or disable for the Pipeline by creating a Config
        m_config = std::make_shared<ob::Config>();

        // Retrieve Depth work mode list 
        if (m_device->isPropertySupported(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, OB_PERMISSION_READ_WRITE)) {
            try {
                // Get the current Depth work mode
                auto curDepthMode = m_device->getCurrentDepthWorkMode();
                // Obtain the work mode list for Depth camera
                auto depthModeList = m_device->getDepthWorkModeList();
                for (uint32_t i = 0; i < depthModeList->count(); i++) {
                    m_deviceDepthModeStringList.push_back((*depthModeList)[i].name);
                    if (strcmp(curDepthMode.name, (*depthModeList)[i].name) == 0) {
                        m_curDeviceDepthMode = i;
                    }
                }
                m_device->switchDepthWorkMode(m_deviceDepthModeStringList[0].c_str());
            }
            catch (ob::Error& e) {
                std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
            }
        }
        
        // Obtain all Stream Profiles for Color camera, including resolution, frame rate, and format
        m_colorStreamProfileList = m_pipeline->getStreamProfileList(OB_SENSOR_COLOR);
        m_colorStreamProfile = nullptr;
        try {
            // According to the desired configurations to find the corresponding Profile, preference to RGB888 format
            m_colorStreamProfile = m_colorStreamProfileList->getVideoStreamProfile(DEFAULT_WIDTH, DEFAULT_HEIGHT, OB_FORMAT_RGB888, 30);
        }
        catch (ob::Error& e) {
            // Open default Profile if cannot find the corresponding format
            m_colorStreamProfile = std::const_pointer_cast<ob::StreamProfile>(m_colorStreamProfileList->getProfile(0))->as<ob::VideoStreamProfile>();
        }


        // Obtain all Stream Profiles for Depth camera, including resolution, frame rate, and format
        m_depthStreamProfileList = m_pipeline->getStreamProfileList(OB_SENSOR_DEPTH);
        m_depthStreamProfile = nullptr;
        try {
            // According to the desired configurations to find the corresponding Profile, preference to Y16 format
            m_depthStreamProfile = m_depthStreamProfileList->getVideoStreamProfile(DEFAULT_WIDTH, 0, OB_FORMAT_Y16, 30);
        }
        catch (ob::Error& e) {
            // Open default Profile if cannot find the corresponding format
            m_depthStreamProfile = std::const_pointer_cast<ob::StreamProfile>(m_depthStreamProfileList->getProfile(0))->as<ob::VideoStreamProfile>();
        }

        // Try find supported depth to color align hardware mode profile
        m_depthStreamProfileList = m_pipeline->getD2CDepthProfileList(m_colorStreamProfile, ALIGN_D2C_HW_MODE);
        if (m_depthStreamProfileList->count() > 0) {
            m_bIsSWD2C = false;
        }
        else {
            // Try find supported depth to color align software mode profile
            m_depthStreamProfileList = m_pipeline->getD2CDepthProfileList(m_colorStreamProfile, ALIGN_D2C_SW_MODE);
            if (m_depthStreamProfileList->count() > 0) {
                m_bIsSWD2C = true;
            }
        }

        // Obtain all Stream Profiles for IR camera, including resolution, frame rate, and format
        try {
            m_irStreamProfileList = m_pipeline->getStreamProfileList(OB_SENSOR_IR);
            m_irStreamProfile = nullptr;
            try {
                // According to the desired configurations to find the corresponding Profile, preference to Y16 format
                m_irStreamProfile = m_irStreamProfileList->getVideoStreamProfile(DEFAULT_WIDTH, DEFAULT_HEIGHT, OB_FORMAT_Y16, 30);
            }
            catch (ob::Error& e) {
                // Open default Profile if cannot find the corresponding format
                m_irStreamProfile = std::const_pointer_cast<ob::StreamProfile>(m_irStreamProfileList->getProfile(0))->as<ob::VideoStreamProfile>();
            }
        }
        catch (ob::Error& e) {
            g_isIRUnique = false;
            // Dual IR, open with IR Left in default
            m_irStreamProfileList = m_pipeline->getStreamProfileList(OB_SENSOR_IR_LEFT);
            m_irStreamProfile = nullptr;
            try {
                // According to the desired configurations to find the corresponding Profile, preference to Y16 format
                m_irStreamProfile = m_irStreamProfileList->getVideoStreamProfile(DEFAULT_WIDTH, DEFAULT_HEIGHT, OB_FORMAT_Y16, 30);
            }
            catch (ob::Error& e) {
                // Open default Profile if cannot find the corresponding format
                m_irStreamProfile = std::const_pointer_cast<ob::StreamProfile>(m_irStreamProfileList->getProfile(0))->as<ob::VideoStreamProfile>();
            }
        }

        // TO OBTAIN A DEFAULT CAMERA PARAMETER
        try {
            m_config->enableStream(m_depthStreamProfile);
            m_pipeline->start(m_config);
            m_curCameraParams = m_pipeline->getCameraParam();
            m_pipeline->stop();
            m_config->disableStream(OB_STREAM_DEPTH);
        }
        catch (ob::Error& e) {
            std::cerr << "startCurDepth: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        }

        return 0;
    }
    catch (ob::Error& e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        return -1;
    }
}

void Sensors::deinitCurSensor()
{
    if (m_device) {
        try {
            if (m_curColorFrame) {
                printf("Destroy current color frame.\n");
                m_bIsColorOn = false;
                m_curColorFrame.reset();
            }
            if (m_curDepthFrame) {
                printf("Destroy current depth frame.\n");
                m_bIsDepthOn = false;
                m_curDepthFrame.reset();
            }
            if (m_curIRFrame) {
                printf("Destroy current IR frame.\n");
                m_bIsIROn = false;
                m_curIRFrame.reset();
            }

            // Stop current Pipeline and won't generate further frame data
            m_pipeline->stop();
            printf("Orbbec Pipeline stopped.\n");
        }
        catch (ob::Error& e) {
            std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        }
    }
}

// Obtain Property list
std::vector<OBPropertyItem> Sensors::getPropertyList(std::shared_ptr<ob::Device> device) {
    std::vector<OBPropertyItem> propertyVec;
    propertyVec.clear();
    uint32_t size = device->getSupportedPropertyCount();
    for (uint32_t i = 0; i < size; i++) {
        OBPropertyItem property_item = device->getSupportedProperty(i);
        if (property_item.type != OB_STRUCT_PROPERTY && property_item.permission != OB_PERMISSION_DENY) {
            propertyVec.push_back(property_item);
        }
    }
    return propertyVec;
}

void Sensors::getCurSensorInfo(SensorInfo_S& sensorInfo)
{
    sensorInfo.deviceInfo = m_device->getDeviceInfo();
    sensorInfo.vid = sensorInfo.deviceInfo->vid();
    sensorInfo.pid = sensorInfo.deviceInfo->pid();
    strcpy(sensorInfo.serialNum, sensorInfo.deviceInfo->serialNumber());
    strcpy(sensorInfo.deviceName, sensorInfo.deviceInfo->name());
}

std::vector<std::string>* Sensors::getSensorStrList()
{
    return &m_deviceStringList;
}

std::string Sensors::getFirmwareVer()
{
    return std::string(m_device->getDeviceInfo()->firmwareVersion());
}

std::string Sensors::getSDKVer()
{
    try {
        // Print out SDK version
        std::string ret = std::to_string(ob::Version::getMajor()) + "." + std::to_string(ob::Version::getMinor()) + "." + std::to_string(ob::Version::getPatch());
        return ret;
    }
    catch (ob::Error& e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        return "";
    }
}

bool Sensors::toggleD2CAlignment(int type)
{
    m_bIsD2CAlignmentOn = !m_bIsD2CAlignmentOn;
    try {
        m_pipeline->stop();  // gotta check if its necessary
        if (m_bIsD2CAlignmentOn) {
            if (type == 0)
                m_config->setAlignMode(m_bIsSWD2C ? ALIGN_D2C_SW_MODE : ALIGN_D2C_HW_MODE);
            else
                m_config->setAlignMode(ALIGN_D2C_SW_MODE);
        }
        else
            m_config->setAlignMode(ALIGN_DISABLE);
        m_pipeline->start(m_config);
    }
    catch (std::exception& e) {
        std::cout << "[ERR] D2C Alignment property not support" << std::endl;
        m_bIsD2CAlignmentOn = false;
    }
    return m_bIsD2CAlignmentOn;
}

bool Sensors::toggleFrameSync()
{
    m_bisFrameSyncOn = !m_bisFrameSyncOn;
    try {
        if (m_bisFrameSyncOn)
            m_pipeline->enableFrameSync();
        else
            m_pipeline->disableFrameSync();

        return m_bisFrameSyncOn;
    }
    catch (...) {
        std::cout << "[ERR] FrameSync property not support" << std::endl;
        return false;
    }
}
bool Sensors::setLaserEnable(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_LASER_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_LASER_BOOL, state);
        }
        else {
            std::cout << "[ERR] Set Laser property not supported" << std::endl;
        }
    }
    catch (...) {
        std::cout << "[ERR] set property failed: setLaserEnable" << std::endl;
        return false;
    }
    return true;
}
int Sensors::getDepthPrecisionLevel()
{
    int ret = -1;
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_DEPTH_PRECISION_LEVEL_INT);
        }
        else {
            std::cout << "[ERR] Get Depth Precision Level property not supported" << std::endl;
        }
    }
    catch (...) {
        std::cout << "[ERR] Get int property failed; getDepthPrecisionLevel" << std::endl;
    }
    return ret;
}
bool Sensors::setDepthPrecisionLevel(int level)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_PRECISION_LEVEL_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_DEPTH_PRECISION_LEVEL_INT, level);
        }
        else {
            std::cout << "[ERR] Set Depth Precision Level property not supported" << std::endl;
        }
    }
    catch (...) {
        std::cout << "[ERR] set property failed: setDepthPrecisionLevel" << std::endl;
        return false;
    }
    return true;
}

OBCameraParam Sensors::getCameraParams()
{
    m_curCameraParams = m_pipeline->getCameraParam();
	return m_curCameraParams;
}

FrameInfo_S Sensors::getCurFrameInfo(int frameType)
{
    FrameInfo_S frameInfo;
    switch (frameType)
    {
    case 0:
        frameInfo.w = m_depthStreamProfile->width();
        frameInfo.h = m_depthStreamProfile->height();
        frameInfo.fps = m_depthStreamProfile->fps();
        break;
    case 1:
        frameInfo.w = m_colorStreamProfile->width();
        frameInfo.h = m_colorStreamProfile->height();
        frameInfo.fps = m_colorStreamProfile->fps();
        break;
    case 2:
        frameInfo.w = m_irStreamProfile->width();
        frameInfo.h = m_irStreamProfile->height();
        frameInfo.fps = m_irStreamProfile->fps();
        break;
    }
    return frameInfo;
}

void Sensors::setColorVideoMode(int index)
{
    try {
        bool bIsStreamOn = m_bIsColorOn;
        if (bIsStreamOn) {
            m_curColorFrame.reset();
            m_pipeline->stop();
        }

        m_colorStreamProfile = m_colorStreamProfileList->getProfile(index)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsColorOn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setColorVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void Sensors::setDepthVideoMode(int index)
{
    try {
        bool bIsStreamOn = m_bIsDepthOn;
        if (bIsStreamOn) {
            m_curDepthFrame.reset();
            m_pipeline->stop();
        }

        m_depthStreamProfile = m_depthStreamProfileList->getProfile(index)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsDepthOn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setDepthVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void Sensors::setIRVideoMode(int index)
{
    try {
        bool bIsStreamOn = m_bIsIROn;
        if (bIsStreamOn) {
            m_curIRFrame.reset();
            m_pipeline->stop();
        }

        m_irStreamProfile = m_irStreamProfileList->getProfile(index)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsIROn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setIRVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void Sensors::setColorVideoMode(int width, int height, int fps)
{
    try {
        bool bIsStreamOn = m_bIsColorOn;
        if (bIsStreamOn) {
            m_curColorFrame.reset();
            m_pipeline->stop();
        }

        m_colorStreamProfile = m_colorStreamProfileList->getVideoStreamProfile(width, height, OB_FORMAT_UNKNOWN, fps)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsColorOn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setColorVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void Sensors::setDepthVideoMode(int width, int height, int fps)
{
    try {
        bool bIsStreamOn = m_bIsDepthOn;
        if (bIsStreamOn) {
            m_curDepthFrame.reset();
            m_pipeline->stop();
        }

        m_depthStreamProfile = m_depthStreamProfileList->getVideoStreamProfile(width, height, OB_FORMAT_UNKNOWN, fps)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsDepthOn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setDepthVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

void Sensors::setIRVideoMode(int width, int height, int fps)
{
    try {
        bool bIsStreamOn = m_bIsIROn;
        if (bIsStreamOn) {
            m_curIRFrame.reset();
            m_pipeline->stop();
        }

        m_irStreamProfile = m_irStreamProfileList->getVideoStreamProfile(width, height, OB_FORMAT_UNKNOWN, fps)->as<ob::VideoStreamProfile>();

        if (bIsStreamOn) {
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsIROn = true;
        }
    }
    catch (ob::Error& e) {
        std::cerr << "setIRVideoMode: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }
}

int Sensors::startCurColor()
{
    try {
        m_pipeline->stop();
        if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
        if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
        m_config->enableStream(m_colorStreamProfile);
        m_pipeline->start(m_config);
        m_bIsColorOn = true;
    }
    catch (ob::Error& e) {
        std::cerr << "startCurColor: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }

    if (!m_bIsColorOn) {
        printf("Couldn't start COLOR stream.\n");
        return -2;
    }

    return 0;
}

void Sensors::stopCurColor()
{
    if (m_curColorFrame) {
        try {
            m_curColorFrame.reset();
            m_pipeline->stop();
            m_config->disableStream(OB_STREAM_COLOR);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsColorOn = false;
        }
        catch (ob::Error& e) {
            std::cerr << "stopCurColor: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        }
    }
    if (m_bIsColorOn) {
        printf("Couldn't stop COLOR stream.\n");
    }
}

void Sensors::getColorInfoFromModeList(int mode, int& width, int& height, int& fps)
{
    auto profile = m_colorStreamProfileList->getProfile(mode)->as<ob::VideoStreamProfile>();
    width = profile->width();
    height = profile->height();
    fps = profile->fps();
}

unsigned char* Sensors::getCurColorData()
{
    if (m_curColorFrame == nullptr || m_curColorFrame->dataSize() < 1024) return NULL;

    auto videoFrame = m_curColorFrame->as<ob::VideoFrame>();
    return (unsigned char*)videoFrame->data();
}

bool Sensors::getColorMirror()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_MIRROR_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_COLOR_MIRROR_BOOL);
        }
        else {
            printf("[ERR] Get Color Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Mirror property failed.\n");
    }
    return ret;
}
void Sensors::toggleColorMirror(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_MIRROR_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_COLOR_MIRROR_BOOL, state);
        }
        else {
            printf("[ERR] Set Color Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Mirror property failed.\n");
    }
}

bool Sensors::getColorFlip()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_FLIP_BOOL, OB_PERMISSION_READ_WRITE)) {
            ret = m_device->getBoolProperty(OB_PROP_COLOR_FLIP_BOOL);
        }
        else {
            printf("[ERR] Get Color Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Flip property failed.\n");
    }
    return ret;
}
void Sensors::toggleColorFlip(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_FLIP_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_COLOR_FLIP_BOOL, state);
        }
        else {
            printf("[ERR] Set Color Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Flip property failed.\n");
    }
}

bool Sensors::getAutoWhiteBalanceStatus()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
        }
        else {
            printf("[ERR] Get Color Auto White Balance property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Auto White Balance failed.\n");
    }
    return ret;
}
void Sensors::toggleAutoWhiteBalance(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, state);
        }
        else {
            printf("[ERR] Set Color Auto White Balance property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Auto White Balance property failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getAutoExposureStatus()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_READ)) {
            ret.state = m_device->getBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_COLOR_EXPOSURE_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get Color Auto White Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Auto Exposure failed.\n");
    }
    return ret;
}
void Sensors::toggleAutoExposure(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, state);
        }
        else {
            printf("[ERR] Set Color Auto White Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Auto Exposure property failed.\n");
    }
}

int Sensors::getColorExposureValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_EXPOSURE_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_COLOR_EXPOSURE_INT);
        }
        else {
            printf("[ERR] Get Color Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Exposure value failed.\n");
    }

    return ret;
}
void Sensors::setColorExposureValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, value);
        }
        else {
            printf("[ERR] Set Color Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Exposure value failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getColorGainRange()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_GAIN_INT, OB_PERMISSION_READ)) {
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_COLOR_GAIN_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get Color Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Gain property failed.\n");
    }
    return ret;
}
int Sensors::getColorGainValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_GAIN_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_COLOR_GAIN_INT);
        }
        else {
            printf("[ERR] Get Color Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Color Gain value failed.\n");
    }

    return ret;
}
void Sensors::setColorGainValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_COLOR_GAIN_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_COLOR_GAIN_INT, value);
        }
        else {
            printf("[ERR] Set Color Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Color Gain value failed.\n");
    }
}

int Sensors::startCurDepth()
{
    try {
        m_pipeline->stop();
        if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
        if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
        m_config->enableStream(m_depthStreamProfile);
        m_pipeline->start(m_config);
        m_bIsDepthOn = true;
    }
    catch (ob::Error& e) {
        std::cerr << "startCurDepth: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }

    if (!m_bIsDepthOn) {
        printf("Couldn't start DEPTH stream.\n");
        return -2;
    }

    return 0;
}

void Sensors::stopCurDepth()
{
    if (m_curDepthFrame) {
        try {
            m_curDepthFrame.reset();
            m_pipeline->stop();
            m_config->disableStream(OB_STREAM_DEPTH);
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsIROn)      m_config->enableStream(m_irStreamProfile);
            m_pipeline->start(m_config);
            m_bIsDepthOn = false;
        }
        catch (ob::Error& e) {
            std::cerr << "stopCurDepth: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        }
    }
    if (m_bIsDepthOn) {
        printf("Couldn't stop DEPTH stream.\n");
    }
}

unsigned char* Sensors::getCurDepthData()
{
    if (m_curDepthFrame == nullptr || m_curDepthFrame->dataSize() < 1024) return NULL;

    auto videoFrame = m_curDepthFrame->as<ob::VideoFrame>();
    return (unsigned char*)videoFrame->data();
}

bool Sensors::getDepthMirror()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_MIRROR_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL);
        }
        else {
            printf("[ERR] Get Depth Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Mirror property failed.\n");
    }
    return ret;
}
void Sensors::toggleDepthMirror(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_MIRROR_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL, state);
        }
        else {
            printf("[ERR] Set Depth Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Depth Mirror property failed.\n");
    }
}

bool Sensors::getDepthFlip()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_FLIP_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_DEPTH_FLIP_BOOL);
        }
        else {
            printf("[ERR] Get Depth Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Flip property failed.\n");
    }
    return ret;
}
void Sensors::toggleDepthFlip(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_FLIP_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_DEPTH_FLIP_BOOL, state);
        }
        else {
            printf("[ERR] Set Depth Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Depth Flip property failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getDepthAutoExposureStatus()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PERMISSION_READ)) {
            ret.state = m_device->getBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_DEPTH_EXPOSURE_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get Depth Auto Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Auto Exposure property failed.\n");
    }
    return ret;
}
void Sensors::toggleDepthAutoExposure(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, state);
        }
        else {
            printf("[ERR] Set Depth Auto Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Depth Auto Exposure property failed.\n");
    }
}

int Sensors::getDepthExposureValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);
        }
        else {
            printf("[ERR] Get Depth Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Exposure value failed.\n");
    }

    return ret;
}
void Sensors::setDepthExposureValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, value);
        }
        else {
            printf("[ERR] Set Depth Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Depth Exposure value failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getDepthGainRange()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_GAIN_INT, OB_PERMISSION_READ)) {
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_DEPTH_GAIN_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get Depth Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Gain property failed.\n");
    }
    return ret;
}
int Sensors::getDepthGainValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_GAIN_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_DEPTH_GAIN_INT);
        }
        else {
            printf("[ERR] Get Depth Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Depth Gain value failed.\n");
    }

    return ret;
}
void Sensors::setDepthGainValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_DEPTH_GAIN_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_DEPTH_GAIN_INT, value);
        }
        else {
            printf("[ERR] Set Depth Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Depth Gain value failed.\n");
    }
}

bool Sensors::getLDPStatus()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_LDP_STATUS_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_LDP_STATUS_BOOL);
        }
        else {
            printf("[ERR] Get LDP Status property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get LDP status property failed.\n");
    }
    return ret;
}
void Sensors::toggleLDP(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_LDP_STATUS_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_LDP_STATUS_BOOL, state);
        }
        else {
            printf("[ERR] Set LDP Status property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set LDP property failed.\n");
    }
}


bool Sensors::getMDCAStatus()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL);
        }
        else {
            printf("[ERR] Get Hardware Distortion property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get Hardware Distortion property failed.\n");
    }
    return ret;
}
void Sensors::toggleMDCA(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_HARDWARE_DISTORTION_SWITCH_BOOL, state);
        }
        else {
            printf("[ERR] Set Hardware Distortion property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set Hardware Distortion failed.\n");
    }
}
bool Sensors::setDepthWorkMode(int mode)
{
    try {
        auto ret = m_device->switchDepthWorkMode(m_deviceDepthModeStringList[mode].c_str());

        auto curDepthMode = m_device->getCurrentDepthWorkMode();
        if (strcmp(curDepthMode.name, m_deviceDepthModeStringList[mode].c_str()) == 0) {
            std::cout << "Switch depth work mode success! currentDepthMode: " << curDepthMode.name << std::endl;
        }
        else {
            std::cout << "Switch depth work mode failed!" << std::endl;
        }
        return !ret;
    }
    catch (ob::Error& e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        return false;
    }
}

int Sensors::startCurIR()
{
    try {
        m_pipeline->stop();
        if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
        if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
        m_config->enableStream(m_irStreamProfile);
        m_pipeline->start(m_config);
        m_bIsIROn = true;
    }
    catch (ob::Error& e) {
        std::cerr << "startCurIR: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    }

    if (!m_bIsIROn) {
        printf("Couldn't start IR stream.\n");
        return -2;
    }

    return 0;
}

void Sensors::stopCurIR()
{
    if (m_curIRFrame) {
        try {
            m_curIRFrame.reset();
            m_pipeline->stop();
            m_config->disableStream(OB_STREAM_IR);
            if (m_bIsColorOn)   m_config->enableStream(m_colorStreamProfile);
            if (m_bIsDepthOn)   m_config->enableStream(m_depthStreamProfile);
            m_pipeline->start(m_config);
            m_bIsIROn = false;
        }
        catch (ob::Error& e) {
            std::cerr << "stopCurIR: " << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
        }
    }
    if (m_bIsIROn) {
        printf("Couldn't stop IR stream.\n");
    }
}

unsigned char* Sensors::getCurIRData()
{
    if (m_curIRFrame == nullptr || m_curIRFrame->dataSize() < 1024) return NULL;

    auto videoFrame = m_curIRFrame->as<ob::VideoFrame>();
    return (unsigned char*)videoFrame->data();
}

bool Sensors::getIRMirror()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_MIRROR_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_IR_MIRROR_BOOL);
        }
        else {
            printf("[ERR] Get IR Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Mirror property failed.\n");
    }
    return ret;
}
void Sensors::toggleIRMirror(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_MIRROR_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_IR_MIRROR_BOOL, state);
        }
        else {
            printf("[ERR] Set IR Mirror property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Mirror property failed.\n");
    }
}

bool Sensors::getIRFlip()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_FLIP_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_IR_FLIP_BOOL);
        }
        else {
            printf("[ERR] Get IR Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Flip property failed.\n");
    }
    return ret;
}
void Sensors::toggleIRFlip(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_FLIP_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_IR_FLIP_BOOL, state);
        }
        else {
            printf("[ERR] Set IR Flip property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Flip property failed.\n");
    }
}

bool Sensors::getIRFloodStatus()
{
    bool ret = false;
    try {
        if (m_device->isPropertySupported(OB_PROP_FLOOD_BOOL, OB_PERMISSION_READ)) {
            ret = m_device->getBoolProperty(OB_PROP_FLOOD_BOOL);
        }
        else {
            printf("[ERR] Get IR Flood property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Flood property failed.\n");
    }
    return ret;
}
void Sensors::toggleIRFlood(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_FLOOD_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_FLOOD_BOOL, state);
        }
        else {
            printf("[ERR] Set IR Flood property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Flood property failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getIRAutoExposureStatus()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_READ)) {
            ret.state = m_device->getBoolProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL);
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_IR_EXPOSURE_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get IR Auto Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Auto Exposure property failed.\n");
    }
    return ret;
}
void Sensors::toggleIRAutoExposure(bool state)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_WRITE)) {
            m_device->setBoolProperty(OB_PROP_IR_AUTO_EXPOSURE_BOOL, state);
        }
        else {
            printf("[ERR] Set IR Auto Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Auto Exposure property failed.\n");
    }
}

int Sensors::getIRExposureValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_EXPOSURE_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_IR_EXPOSURE_INT);
        }
        else {
            printf("[ERR] Get IR Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Exposure value failed.\n");
    }

    return ret;
}
void Sensors::setIRExposureValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_IR_EXPOSURE_INT, value);
        }
        else {
            printf("[ERR] Set IR Exposure property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Exposure value failed.\n");
    }
}

PropertyInfo_S<int> Sensors::getIRGainRange()
{
    PropertyInfo_S<int> ret = { false, -1, -1 };
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_GAIN_INT, OB_PERMISSION_READ)) {
            // get the value range
            OBIntPropertyRange valueRange = m_device->getIntPropertyRange(OB_PROP_IR_GAIN_INT);
            ret.min = valueRange.min;
            ret.max = valueRange.max;
        }
        else {
            printf("[ERR] Get IR Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Gain property failed.\n");
    }
    return ret;
}
int Sensors::getIRGainValue()
{
    int ret = 0;
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_GAIN_INT, OB_PERMISSION_READ)) {
            ret = m_device->getIntProperty(OB_PROP_IR_GAIN_INT);
        }
        else {
            printf("[ERR] Get IR Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Get IR Gain value failed.\n");
    }

    return ret;
}
void Sensors::setIRGainValue(int value)
{
    try {
        if (m_device->isPropertySupported(OB_PROP_IR_GAIN_INT, OB_PERMISSION_WRITE)) {
            m_device->setIntProperty(OB_PROP_IR_GAIN_INT, value);
        }
        else {
            printf("[ERR] Set IR Gain property not supported.\n");
        }
    }
    catch (...) {
        printf("[ERR] Set IR Gain value failed.\n");
    }
}

void Sensors::togglePointCloud() {
    m_bIsPointCloudOn = !m_bIsPointCloudOn;
    m_curCameraParams = m_pipeline->getCameraParam();
}

void Sensors::generatePointCloudPoints(vector<OBColorPoint> &points, bool is_color)
{
    m_curFrameSet = m_pipeline->waitForFrames(100);

    if (m_curFrameSet != nullptr && m_curFrameSet->depthFrame() != nullptr) {
        if (m_curFrameSet->colorFrame() != nullptr && is_color) {
            // point position value multiply depth value scale to convert uint to millimeter (for some devices, the default depth value uint is not millimeter)
            auto depthValueScale = m_curFrameSet->depthFrame()->getValueScale();
            ob::PointCloudFilter m_pointCloud;
            m_pointCloud.setCameraParam(m_curCameraParams);
            m_pointCloud.setPositionDataScaled(depthValueScale);
            try {
                // Generate a colored point cloud
                m_pointCloud.setCreatePointFormat(OB_FORMAT_RGB_POINT);
                std::shared_ptr<ob::Frame> frame = m_pointCloud.process(m_curFrameSet);

                OBColorPoint* point = (OBColorPoint*)frame->data();
                int pointsSize = frame->dataSize() / sizeof(OBColorPoint);
                for (int i = 0; i < pointsSize; i++) {
                    points.push_back(*point);
                    point++;
                }
            }
            catch (std::exception& e) {
                std::cout << "Get point cloud failed" << std::endl;
            }
        }
        else {
            const float max_dis = 5120.0f;
            // point position value multiply depth value scale to convert uint to millimeter (for some devices, the default depth value uint is not millimeter)
            auto depthValueScale = m_curFrameSet->depthFrame()->getValueScale();
            ob::PointCloudFilter m_pointCloud;
            m_pointCloud.setCameraParam(m_curCameraParams);
            m_pointCloud.setPositionDataScaled(depthValueScale);
            try {
                // generate point cloud 
                m_pointCloud.setCreatePointFormat(OB_FORMAT_POINT);
                std::shared_ptr<ob::Frame> frame = m_pointCloud.process(m_curFrameSet);

                OBPoint* point = (OBPoint*)frame->data();
                int pointsSize = frame->dataSize() / sizeof(OBPoint);
                for (int i = 0; i < pointsSize; i++) {
                    OBColorPoint tmpPoint;
                    tmpPoint.x = point->x;
                    tmpPoint.y = point->y;
                    tmpPoint.z = point->z;
                    tmpPoint.r = point->z / max_dis * 255;
                    tmpPoint.g = point->z / max_dis * 255;
                    tmpPoint.b = point->z / max_dis * 255;
                    points.push_back(tmpPoint);
                    point++;
                }
            }
            catch (std::exception& e) {
                std::cout << "Get point cloud failed" << std::endl;
            }
        }
    }
    else {
        std::cout << "Get color frame or depth frame failed!" << std::endl;
    }
}

void Sensors::readFrame()
{
    m_curFrameSet = m_pipeline->waitForFrames(100);
    if (m_curFrameSet == nullptr) { return; }

    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
    if (lock.try_lock()) {
        if (m_curFrameSet->colorFrame() != nullptr) {
            m_curColorFrame = m_curFrameSet->colorFrame();
        }
        if (m_curFrameSet->depthFrame() != nullptr) {
            m_curDepthFrame = m_curFrameSet->depthFrame();
        }
        if (m_curFrameSet->irFrame() != nullptr || m_curFrameSet->getFrame(OB_FRAME_IR_LEFT) != nullptr) {
            if (g_isIRUnique)
                m_curIRFrame = m_curFrameSet->irFrame();
            else
                m_curIRFrame = m_curFrameSet->getFrame(OB_FRAME_IR_LEFT);
        }
    }
}
