#pragma once
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc/types_c.h"
#include "orbbec_sensors.h"
#include <numeric>

extern bool g_isStereoCamera;

class Service
{
public:
	Service(int& state, int deviceIndex = 0);
	~Service();
	int initCamera(int deviceIndex);
	int resetCamera();

	std::vector<std::string>* getSensorStrList();
	std::vector<std::string>* getSensorInfo(OBSensorType sensorType);
	FrameInfo_S getCurFrameInfo(int mode);
	inline OBCameraParam getCameraParams() { return mSensors->getCameraParams(); }
	inline std::string getFirmwareVer() { return mSensors->getFirmwareVer(); }
	inline std::string getSDKVer() { return mSensors->getSDKVer(); }
	inline int getRecentDevice() { return mRecentDevice; }
	inline std::string getSerialNum() { return mSerialNum; }
	inline std::string getSensorName() { return mSensorName; }

	inline std::vector<std::string>* getCurDepthWorkModeStrList() { return &mDepthModeStrList; }
	inline int getCurDepthWorkMode() { return mDepthMode; }
	inline bool setDepthWorkMode(int mode) { return mSensors->setDepthWorkMode(mode); }

	int getColorVideoMode();
	void setColorVideoMode(int mode);
	void setColorVideoMode(int width, int height, int fps = 30);

	int getDepthVideoMode();
	void setDepthVideoMode(int mode);
	void setDepthVideoMode(int width, int height, int fps = 30);

	int getIRVideoMode();
	void setIRVideoMode(int mode);
	void setIRVideoMode(int width, int height, int fps = 30);

	int getVideoMode(const std::shared_ptr<ob::VideoStreamProfile>& videoMode, std::vector<std::string>& modeList);

	void switchDepthStream(bool state);
	void switchColorStream(bool state);
	void switchIRStream(bool state);

	// Device Control
	bool toggleFrameSync();
	void toggleD2CAlignment(int type);
	bool toggleLaserEnable(bool state);
	void toggleMirror(OBSensorType sensorType);
	bool getMirrorState(OBSensorType sensorType);
	void toggleFlip(OBSensorType sensorType);
	bool getFlipState(OBSensorType sensorType);
	PropertyInfo_S<int> getAutoExposureStatus(OBSensorType sensorType);
	PropertyInfo_S<int> getGainRange(OBSensorType sensorType);

	// Color
	bool getAutoWhiteBalanceStatus();
	void toggleAutoWhiteBalance(bool state);
	void toggleAutoExposure(bool state);
	int getExposureValue();
	void setExposureValue(int value);
	int getColorGainValue();
	void setColorGainValue(int value);

	// Depth
	void getDepthDispRange(int* range);
	void setDepthDispRange(int* range);
	int getDepthPrecisionLevel();
	bool setDepthPrecisionLevel(int level);
	void toggleDepthAutoExposure(bool state);
	int getDepthExposureValue();
	void setDepthExposureValue(int value);
	int getDepthGainValue();
	void setDepthGainValue(int value);
	bool getLDPStatus();
	void toggleLDP(bool state);

	// IR
	PropertyInfo_S<int> getIRAutoExposureStatus();
	void toggleIRAutoExposure(bool state);
	int getIRExposureValue();
	void setIRExposureValue(int value);
	int getIRGainValue();
	void setIRGainValue(int value);
	void toggleIRFlood(bool& state);
	void toggleMDCA(bool state);
	bool getMDCAStatus();

	void startFrameCapturing(bool* is_checked, int frame_num);
	bool isFrameCapturing() { return mTotalFrame; }
	void readFrame();

	cv::Mat* getColorMat();
	cv::Mat* getDepthMat();
	cv::Mat* getIRMat();

	void togglePointCloud() { mSensors->togglePointCloud(); };
	void getPointCloudPoints(vector<OBColorPoint>& points, bool is_color);

private:
	std::mutex  mMutex;
	Sensors* mSensors;
	std::shared_ptr<ob::DeviceList> mSensorList;
	std::vector<std::string> mSensorListStr;

	std::vector<std::string> mDepthModeStrList;
	int mDepthMode;

	std::vector<std::string> mDepthSupportedModeList;
	std::vector<std::string> mColorSupportedModeList;
	std::vector<std::string> mIRSupportedModeList;

	cv::Mat mColorBGRMat;
	cv::Mat mColorRGBMat;
	cv::Mat mDepthMat;
	cv::Mat mDepthBGRMat;
	cv::Mat mDepthRawMat;
	cv::Mat mIRMat;
	cv::Mat mIRRawMat;

	std::string mSerialNum;
	std::string mSensorName;
	int mSensorPID = -1;
	int mRecentDevice;
	int mCurDepthMode = -1;
	float mDepthValueScale = 1.0f;
	int mDepthDispRange[2] = { 0, 5000 };
	bool mLaserEnable = false;
	bool mIRFlood = true;
	int mExposureValue = 0;
	bool mColorMirror = false;
	bool mDepthMirror = false;
	bool mIRMirror = false;
	bool mColorFlip = false;
	bool mDepthFlip = false;
	bool mIRFlip = false;
	bool mIsCapturing[3] = { 0, 0, 0 };
	uint64_t mFrameCount[3] = { 0, 0, 0 };
	uint64_t mPreviousFrameIdx[3];
	int mTotalFrame = 0;

	void captureFrames();
};

