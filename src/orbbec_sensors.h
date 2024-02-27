#pragma once
#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"
#include "utils.hpp"
#include <thread>
#include <mutex>
#include <string.h>

using namespace std;

extern bool g_isIRUnique;

class Sensors
{
public:
    Sensors();
    ~Sensors();

    int initCurSensor(const int deviceIndex);
    void deinitCurSensor();

    inline const std::shared_ptr<ob::DeviceList> getSensorList() { return m_deviceList; }   // Might not needed
    void getCurSensorInfo(SensorInfo_S& sensorInfo);
    std::vector<std::string>* getSensorStrList();
    std::string getFirmwareVer();
    std::string getSDKVer();

    // ##### Device Control #####
    bool toggleD2CAlignment(int type);
	bool toggleFrameSync();
    bool setLaserEnable(bool state);
    int getDepthPrecisionLevel();
    bool setDepthPrecisionLevel(int level);

    OBCameraParam getCameraParams();

    inline const std::shared_ptr<ob::StreamProfileList> getColorSensorInfo()    { return m_colorStreamProfileList; }
    inline const std::shared_ptr<ob::StreamProfileList> getDepthSensorInfo()    { return m_depthStreamProfileList; }
    inline const std::shared_ptr<ob::StreamProfileList> getIRSensorInfo()       { return m_irStreamProfileList; }

    FrameInfo_S getCurFrameInfo(int frameType);

    inline const std::shared_ptr<ob::VideoStreamProfile> getColorVideoMode()     { return m_colorStreamProfile; }
    inline const std::shared_ptr<ob::VideoStreamProfile> getDepthVideoMode()     { return m_depthStreamProfile; }
    inline const std::shared_ptr<ob::VideoStreamProfile> getIRVideoMode()        { return m_irStreamProfile; }

    inline const std::shared_ptr<ob::Frame> getCurColorFrame()  { return m_curColorFrame; }
    inline const std::shared_ptr<ob::Frame> getCurDepthFrame()  { return m_curDepthFrame; }
    inline const std::shared_ptr<ob::Frame> getCurIRFrame()     { return m_curIRFrame; }

    void setColorVideoMode(int index);
    void setDepthVideoMode(int index);
    void setIRVideoMode(int index);

    void setColorVideoMode(int width, int height, int fps);
    void setDepthVideoMode(int width, int height, int fps);
    void setIRVideoMode(int width, int height, int fps);

    void readFrame();

    //color
    int startCurColor();
    void stopCurColor();
    void getColorInfoFromModeList(int mode, int &width, int &height, int &fps);
    unsigned char* getCurColorData();
	bool getColorMirror();
    void toggleColorMirror(bool state);
    bool getColorFlip();
    void toggleColorFlip(bool state);
    bool getAutoWhiteBalanceStatus();
    void toggleAutoWhiteBalance(bool state);
    PropertyInfo_S<int> getAutoExposureStatus();
    void toggleAutoExposure(bool state);
    int getColorExposureValue();
    void setColorExposureValue(int value);
    PropertyInfo_S<int> getColorGainRange();
    int getColorGainValue();
    void setColorGainValue(int value);

    //depth
    int startCurDepth();
    void stopCurDepth();
    unsigned char* getCurDepthData();
	bool getDepthMirror();
    void toggleDepthMirror(bool state);
    bool getDepthFlip();
    void toggleDepthFlip(bool state);
    PropertyInfo_S<int> getDepthAutoExposureStatus();
    void toggleDepthAutoExposure(bool state);
    int getDepthExposureValue();
    void setDepthExposureValue(int value);
    PropertyInfo_S<int> getDepthGainRange();
    int getDepthGainValue();
    void setDepthGainValue(int value);
	bool getLDPStatus();
	void toggleLDP(bool state);
	bool getMDCAStatus();
	void toggleMDCA(bool state);
    std::vector<std::string>* getCurDepthWorkModeStrList() { return &m_deviceDepthModeStringList; }
    int getCurDepthWorkMode() { return m_curDeviceDepthMode; }
    bool setDepthWorkMode(int mode);

    //ir
    int startCurIR();
    void stopCurIR();
    unsigned char* getCurIRData();
	bool getIRMirror();
    void toggleIRMirror(bool state);
    bool getIRFlip();
    void toggleIRFlip(bool state);
    bool getIRFloodStatus();
	void toggleIRFlood(bool state);
    PropertyInfo_S<int> getIRAutoExposureStatus();
	void toggleIRAutoExposure(bool state);
    int getIRExposureValue();
    void setIRExposureValue(int value);
    PropertyInfo_S<int> getIRGainRange();
    int getIRGainValue();
    void setIRGainValue(int value);

    // Point Cloud
    void togglePointCloud();
    void generatePointCloudPoints(vector<OBColorPoint> &points, bool is_color);

private:
    std::vector<OBPropertyItem> getPropertyList(std::shared_ptr<ob::Device> device);

private:
    std::mutex m_mutex;
    std::shared_ptr<ob::Context> m_context = std::make_shared<ob::Context>();
    std::shared_ptr<ob::Pipeline> m_pipeline = std::make_shared<ob::Pipeline>();
    std::shared_ptr<ob::Config> m_config;
    std::shared_ptr<ob::DeviceList> m_deviceList;
    std::shared_ptr<ob::Device> m_device = nullptr;
    std::vector<std::string> m_deviceStringList;
    std::vector<std::string> m_deviceDepthModeStringList;
    int m_curDeviceDepthMode;

    OBCameraParam m_curCameraParams;

    std::shared_ptr<ob::StreamProfileList> m_colorStreamProfileList;
    std::shared_ptr<ob::StreamProfileList> m_depthStreamProfileList;
    std::shared_ptr<ob::StreamProfileList> m_irStreamProfileList;

    std::shared_ptr<ob::VideoStreamProfile> m_colorStreamProfile;
    std::shared_ptr<ob::VideoStreamProfile> m_depthStreamProfile;
    std::shared_ptr<ob::VideoStreamProfile> m_irStreamProfile;

    std::shared_ptr<ob::Frame> m_curColorFrame;
    std::shared_ptr<ob::Frame> m_curDepthFrame;
    std::shared_ptr<ob::Frame> m_curIRFrame;

    std::shared_ptr<ob::FrameSet> m_curFrameSet;

    bool m_bIsD2CAlignmentOn = false;
    bool m_bIsSWD2C = false;
	bool m_bisFrameSyncOn = false;

    bool m_bIsDepthOn = false;
    bool m_bIsColorOn = false;
    bool m_bIsIROn = false;
    bool m_bIsPointCloudOn = false; 
    //ob::PointCloudFilter m_pointCloud;
    vector<OBColorPoint> m_points;
};
