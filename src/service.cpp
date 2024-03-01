#include "service.h"

Service::Service(int& state, int deviceIndex) :
	mSensors(new Sensors)
{
	state = initCamera(deviceIndex);
	if (!state) return;
}

Service::~Service() { delete mSensors; }

int Service::initCamera(int deviceIndex)
{
	int ret;
	mRecentDevice = -1;
	mSensorList = mSensors->getSensorList();
	int devCount = mSensorList->deviceCount();

	if (devCount == 0) { return INIT_RESULT_NO_DEVICE; }
	if (devCount <= deviceIndex || deviceIndex < 0) { return INIT_RESULT_DEVICE_SELECT_ERROR; }

	ret = mSensors->initCurSensor(deviceIndex);

	mRecentDevice = deviceIndex;

	SensorInfo_S sInfo;
	mSensors->getCurSensorInfo(sInfo);

	mSensorListStr = *mSensors->getSensorStrList();
	mDepthModeStrList = *mSensors->getCurDepthWorkModeStrList();
	mDepthMode = mSensors->getCurDepthWorkMode();

	mSensorName = sInfo.deviceName;
	mSerialNum = sInfo.serialNum;
	mSensorPID = sInfo.pid;

	mDepthValueScale = OBDepthPrecisionLevelToFloat((OBDepthPrecisionLevel)mSensors->getDepthPrecisionLevel());

	mColorMirror = mSensors->getColorMirror();
	mDepthMirror = mSensors->getDepthMirror();
	mIRMirror = mSensors->getIRMirror();

	mColorFlip = mSensors->getColorFlip();
	mDepthFlip = mSensors->getDepthFlip();
	mIRFlip = mSensors->getIRFlip();

	return ret;
}

int Service::resetCamera()
{
	mSensors->deinitCurSensor();
	int ret = 0;
	ret = mSensors->initCurSensor(mRecentDevice);
	return ret;
}

std::vector<std::string>* Service::getSensorStrList()
{
	return &mSensorListStr;
}

std::vector<std::string>* Service::getSensorInfo(OBSensorType sensorType)
{
	std::shared_ptr<ob::StreamProfileList> pSensorInfo;
	std::vector<std::string>* supportedModeList = NULL;

	switch (sensorType) {
	case OBSensorType::OB_SENSOR_DEPTH:
		pSensorInfo = mSensors->getDepthSensorInfo();
		supportedModeList = &mDepthSupportedModeList;
		break;
	case OBSensorType::OB_SENSOR_COLOR:
		mColorSupportedModeList.clear();
		pSensorInfo = mSensors->getColorSensorInfo();
		supportedModeList = &mColorSupportedModeList;
		break;
	case OBSensorType::OB_SENSOR_IR:
	case OBSensorType::OB_SENSOR_IR_LEFT:
	case OBSensorType::OB_SENSOR_IR_RIGHT:
		pSensorInfo = mSensors->getIRSensorInfo();
		supportedModeList = &mIRSupportedModeList;
		break;
	default:
		return NULL;
	}
	// Update when supportedModeList is empty only
	if (pSensorInfo != NULL && supportedModeList != NULL && supportedModeList->size() == 0) {
		for (uint32_t i = 0; i < pSensorInfo->count(); i++) {
			auto profile = pSensorInfo->getProfile(i)->as<ob::VideoStreamProfile>();
			char str[50];
			sprintf(str, "%d x %d @ %d %s", profile->width(), profile->height(), profile->fps(), OBFormatToString(profile->format()).c_str());
			supportedModeList->push_back(str);
		}
	}

	return supportedModeList;
}

FrameInfo_S Service::getCurFrameInfo(int mode) {
	if (mode == 3) {
		FrameInfo_S frameInfo;
		frameInfo.w = mSensors->getCurFrameInfo(1).w;
		frameInfo.h = mSensors->getCurFrameInfo(1).h;
		return frameInfo;
	}
	else
		return mSensors->getCurFrameInfo(mode);
}

int Service::getColorVideoMode()
{
	return getVideoMode(mSensors->getColorVideoMode(), mColorSupportedModeList);
}
void Service::setColorVideoMode(int mode)
{
	mSensors->setColorVideoMode(mode);
}
void Service::setColorVideoMode(int width, int height, int fps)
{
	mSensors->setColorVideoMode(width, height, fps);
}

int Service::getDepthVideoMode()
{
	mCurDepthMode = getVideoMode(mSensors->getDepthVideoMode(), mDepthSupportedModeList);
	return mCurDepthMode;
}
void Service::setDepthVideoMode(int mode)
{
	mCurDepthMode = mode;
	mSensors->setDepthVideoMode(mode);
}
void Service::setDepthVideoMode(int width, int height, int fps)
{
	mSensors->setDepthVideoMode(width, height, fps);
}

int Service::getIRVideoMode()
{
	return getVideoMode(mSensors->getIRVideoMode(), mIRSupportedModeList);
}
void Service::setIRVideoMode(int mode)
{
	mSensors->setIRVideoMode(mode);
}
void Service::setIRVideoMode(int width, int height, int fps)
{
	mSensors->setIRVideoMode(width, height, fps);
}

int Service::getVideoMode(const std::shared_ptr<ob::VideoStreamProfile>& videoMode, std::vector<std::string>& modeList)
{
	char str[50];
	sprintf(str, "%d x %d @ %d %s", videoMode->width(), videoMode->height(), videoMode->fps(), OBFormatToString(videoMode->format()).c_str());

	for (int n = 0; n < (int)modeList.size(); n++) {
		if (modeList[n].compare(str) == 0) {
			return n;
		}
	}

	return -1;
}

void Service::switchDepthStream(bool state)
{
	if (state) {
		mSensors->startCurDepth();
	}
	else
		mSensors->stopCurDepth();
}

void Service::switchColorStream(bool state)
{
	if (state) {
		mSensors->startCurColor();
	}
	else
		mSensors->stopCurColor();
}

void Service::switchIRStream(bool state)
{
	if (state) {
		mSensors->startCurIR();
	}
	else
		mSensors->stopCurIR();
}

// Device Control
bool Service::toggleFrameSync()
{
	return mSensors->toggleFrameSync();
}
// D2C/alignment; 0: hardware 1: software
void Service::toggleD2CAlignment(int type)
{
	mSensors->toggleD2CAlignment(type);
}
bool Service::toggleLaserEnable(bool state)
{
	mLaserEnable = mSensors->setLaserEnable(state);
	return mLaserEnable;
}

void Service::toggleMirror(OBSensorType sensorType)
{
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		mColorMirror = !mColorMirror;
		mSensors->toggleColorMirror(mColorMirror);
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		mDepthMirror = !mDepthMirror;
		mSensors->toggleDepthMirror(mDepthMirror);
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		mIRMirror = !mIRMirror;
		mSensors->toggleIRMirror(mIRMirror);
	}
}
bool Service::getMirrorState(OBSensorType sensorType)
{
	bool ret = false;
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		mColorMirror = mSensors->getColorMirror();
		ret = mColorMirror;
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		mDepthMirror = mSensors->getDepthMirror();
		ret = mDepthMirror;
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		mIRMirror = mSensors->getIRMirror();
		ret = mIRMirror;
	}
	return ret;
}

void Service::toggleFlip(OBSensorType sensorType)
{
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		mColorFlip = !mColorFlip;
		mSensors->toggleColorFlip(mColorFlip);
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		mDepthFlip = !mDepthFlip;
		mSensors->toggleDepthFlip(mDepthFlip);
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		mIRFlip = !mIRFlip;
		mSensors->toggleIRFlip(mIRFlip);
	}
}
bool Service::getFlipState(OBSensorType sensorType)
{
	bool ret = false;
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		mColorFlip = mSensors->getColorFlip();
		ret = mColorFlip;
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		mDepthFlip = mSensors->getDepthFlip();
		ret = mDepthFlip;
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		mIRFlip = mSensors->getIRFlip();
		ret = mIRFlip;
	}
	return ret;
}
PropertyInfo_S<int> Service::getAutoExposureStatus(OBSensorType sensorType)
{
	PropertyInfo_S<int> ret = { false, -1, -1 };
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		return mSensors->getAutoExposureStatus();
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		return mSensors->getDepthAutoExposureStatus();
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		return mSensors->getIRAutoExposureStatus();
	}
	return ret;
}

PropertyInfo_S<int> Service::getGainRange(OBSensorType sensorType)
{
	PropertyInfo_S<int> ret = { false, -1, -1 };
	if (sensorType == OBSensorType::OB_SENSOR_COLOR) {
		return mSensors->getColorGainRange();
	}
	else if (sensorType == OBSensorType::OB_SENSOR_DEPTH) {
		return mSensors->getDepthGainRange();
	}
	else if (sensorType == OBSensorType::OB_SENSOR_IR) {
		return mSensors->getIRGainRange();
	}
	return ret;
}

// Color
bool Service::getAutoWhiteBalanceStatus()
{
	return mSensors->getAutoWhiteBalanceStatus();
}
void Service::toggleAutoWhiteBalance(bool state)
{
	mSensors->toggleAutoWhiteBalance(state);
}

void Service::toggleAutoExposure(bool state)
{
	mSensors->toggleAutoExposure(state);
}

int Service::getExposureValue()
{
	return mSensors->getColorExposureValue();
}
void Service::setExposureValue(int value)
{
	mSensors->setColorExposureValue(value);
}

int Service::getColorGainValue()
{
	return mSensors->getColorGainValue();
}
void Service::setColorGainValue(int value)
{
	mSensors->setColorGainValue(value);
}

// Depth
void Service::getDepthDispRange(int* range)
{
	memcpy(range, mDepthDispRange, sizeof(mDepthDispRange));
}
void Service::setDepthDispRange(int* range)
{
	memcpy(mDepthDispRange, range, sizeof(mDepthDispRange));
}

int Service::getDepthPrecisionLevel()
{
	return mSensors->getDepthPrecisionLevel();
}
bool Service::setDepthPrecisionLevel(int level)
{
	bool ret = mSensors->setDepthPrecisionLevel(level);
	if (ret) mDepthValueScale = OBDepthPrecisionLevelToFloat((OBDepthPrecisionLevel)level);
	return ret;
}

void Service::toggleDepthAutoExposure(bool state)
{
	mSensors->toggleDepthAutoExposure(state);
}

int Service::getDepthExposureValue()
{
	return mSensors->getDepthExposureValue();
}
void Service::setDepthExposureValue(int value)
{
	mSensors->setDepthExposureValue(value);
}

int Service::getDepthGainValue()
{
	return mSensors->getDepthGainValue();
}
void Service::setDepthGainValue(int value)
{
	mSensors->setDepthGainValue(value);
}

bool Service::getLDPStatus()
{
	return mSensors->getLDPStatus();
}
void Service::toggleLDP(bool state)
{
	mSensors->toggleLDP(state);
}


// IR
PropertyInfo_S<int> Service::getIRAutoExposureStatus()
{
	return mSensors->getIRAutoExposureStatus();
}
void Service::toggleIRAutoExposure(bool state)
{
	mSensors->toggleIRAutoExposure(state);
}

int Service::getIRExposureValue()
{
	mExposureValue = mSensors->getIRExposureValue();
	return mExposureValue;
}
void Service::setIRExposureValue(int value)
{
	mExposureValue = value;
	mSensors->setIRExposureValue(value);
}

int Service::getIRGainValue()
{
	return mSensors->getIRGainValue();
}
void Service::setIRGainValue(int value)
{
	mSensors->setIRGainValue(value);
}

void Service::toggleIRFlood(bool& state)
{
	mIRFlood = !mIRFlood;
	state = mIRFlood;
	return mSensors->toggleIRFlood(state);
}

void Service::toggleMDCA(bool state)
{
	mSensors->toggleMDCA(state);
}
bool Service::getMDCAStatus()
{
	return mSensors->getMDCAStatus();
}

void Service::startFrameCapturing(bool* is_checked, int frame_num)
{ 
	std::copy(is_checked, is_checked + 3, mIsCapturing); 
	mTotalFrame = frame_num; 
}

void Service::captureFrames()
{
	std::string output_folder = "CapturedFrames";
	createSubDirectory(output_folder);
	string curDateTime = getCurrentDateTime(true);

	char colorFileName[255];
	char depthFileName[255];
	char IRFileName[255];

	if (mIsCapturing[0]) {
		if (mFrameCount[0] > mTotalFrame) {
			mIsCapturing[0] = false;
			mFrameCount[0] = 0;
		}
		else {
			sprintf(colorFileName, "%s/Color_%s_%lld.png", output_folder.c_str(), curDateTime.c_str(), mFrameCount[0]);
			cv::imwrite(colorFileName, mColorBGRMat);
			printf("File saved: %s\n", colorFileName);
		}
	}
	if (mIsCapturing[1]) {
		if (mFrameCount[1] > mTotalFrame) {
			mIsCapturing[1] = false;
			mFrameCount[1] = 0;
		}
		else {
			sprintf(depthFileName, "%s/Depth_%s_%lld.png", output_folder.c_str(), curDateTime.c_str(), mFrameCount[1]);
			cv::imwrite(depthFileName, mDepthRawMat);
			printf("File saved: %s\n", depthFileName);
		}
	}
	if (mIsCapturing[2]) {
		if (mFrameCount[2] > mTotalFrame) {
			mIsCapturing[2] = false;
			mFrameCount[2] = 0;
		}
		else {
			sprintf(IRFileName, "%s/IR_%s_%lld.png", output_folder.c_str(), curDateTime.c_str(), mFrameCount[2]);
			cv::imwrite(IRFileName, mIRMat);
			printf("File saved: %s\n", IRFileName);
		}
	}

	int capturing_check = std::accumulate(mIsCapturing, mIsCapturing + 3, 0);
	if (!capturing_check) mTotalFrame = 0;
}

void Service::getPointCloudPoints(vector<OBColorPoint>& points, bool is_color) {
	mSensors->generatePointCloudPoints(points, is_color); 
}

void Service::readFrame()
{
	mSensors->readFrame();
	if (mTotalFrame) {
		captureFrames();
	}
}

cv::Mat* Service::getColorMat()
{
	auto frame = mSensors->getCurColorFrame();
	if (frame == nullptr || frame->dataSize() < 1024) {
		return NULL;
	}
	
	auto videoFrame = frame->as<ob::VideoFrame>();

	if (videoFrame->type() == OB_FRAME_COLOR && videoFrame->format() == OB_FORMAT_MJPG) {
		cv::Mat rawMat(1, videoFrame->dataSize(), CV_8UC1, videoFrame->data());
		mColorBGRMat = cv::imdecode(rawMat, 1);
		cv::cvtColor(mColorBGRMat, mColorRGBMat, cv::COLOR_RGB2BGR);
	}
	else if (videoFrame->type() == OB_FRAME_COLOR && videoFrame->format() == OB_FORMAT_NV21) {
		cv::Mat rawMat(videoFrame->height() * 3 / 2, videoFrame->width(), CV_8UC1, videoFrame->data());
		cv::cvtColor(rawMat, mColorRGBMat, cv::COLOR_YUV2BGR_NV21);
	}
	else if (videoFrame->type() == OB_FRAME_COLOR && (videoFrame->format() == OB_FORMAT_YUYV || videoFrame->format() == OB_FORMAT_YUY2)) {
		cv::Mat rawMat(videoFrame->height(), videoFrame->width(), CV_8UC2, videoFrame->data());
		cv::cvtColor(rawMat, mColorBGRMat, cv::COLOR_YUV2BGR_YUY2);
		cv::cvtColor(mColorBGRMat, mColorRGBMat, cv::COLOR_RGB2BGR);
	}
	else if (videoFrame->type() == OB_FRAME_COLOR && videoFrame->format() == OB_FORMAT_RGB888) {
		cv::Mat rawMat(videoFrame->height(), videoFrame->width(), CV_8UC3, videoFrame->data());
		cv::cvtColor(rawMat, mColorBGRMat, cv::COLOR_RGB2BGR);
		cv::cvtColor(mColorBGRMat, mColorRGBMat, cv::COLOR_RGB2BGR);
	}

	if (mIsCapturing[0]) {
		if (mPreviousFrameIdx[0] != frame->index()) {
			mFrameCount[0]++;
			mPreviousFrameIdx[0] = frame->index();
		}
	}

	return &mColorRGBMat;
}

cv::Mat* Service::getDepthMat()
{
	auto frame = mSensors->getCurDepthFrame();
	if (frame == nullptr || frame->dataSize() < 1024) {
		return NULL;
	}

	auto videoFrame = frame->as<ob::VideoFrame>();

	if (videoFrame->format() == OB_FORMAT_Y16) {
		cv::Mat cvtMat, cvTmpMat;
		mDepthRawMat = cv::Mat(videoFrame->height(), videoFrame->width(), CV_16UC1, videoFrame->data());
		// depth frame pixel value multiply scale to get distance in millimeter
		float scale = videoFrame->as<ob::DepthFrame>()->getValueScale();

		cv::inRange(mDepthRawMat, cv::Scalar(mDepthDispRange[0]), cv::Scalar(mDepthDispRange[1]), cvTmpMat);
		mDepthRawMat.copyTo(cvtMat, cvTmpMat);

		// threshold to 5.12m
		//cv::threshold(cvTmpMat, cvtMat, 5120.0f / scale, 0, cv::THRESH_TRUNC);
		cvtMat.convertTo(cvtMat, CV_8UC1, scale * 0.05);
		cv::applyColorMap(cvtMat, mDepthBGRMat, cv::COLORMAP_JET);
		cvtColor(mDepthBGRMat, mDepthMat, CV_RGB2BGR);
	}

	if (mIsCapturing[1]) {
		if (mPreviousFrameIdx[1] != frame->index()) {
			mFrameCount[1]++;
			mPreviousFrameIdx[1] = frame->index();
		}
	}

	return &mDepthMat;
}

cv::Mat* Service::getIRMat()
{
	auto frame = mSensors->getCurIRFrame();
	if (frame == nullptr || frame->dataSize() < 1024) {
		return NULL;
	}

	auto videoFrame = frame->as<ob::VideoFrame>();
	if (videoFrame->format() == OB_FORMAT_Y16 || videoFrame->format() == OB_FORMAT_YUYV || videoFrame->format() == OB_FORMAT_YUY2) {
		cv::Mat cvtMat;
		mIRRawMat = cv::Mat(videoFrame->height(), videoFrame->width(), CV_16UC1, videoFrame->data());
		float scale = 1.0f / (float)pow(2, videoFrame->pixelAvailableBitSize() - 8);
		cv::convertScaleAbs(mIRRawMat, cvtMat, scale);
		cv::cvtColor(cvtMat, mIRMat, cv::COLOR_GRAY2RGB);
	}

	if (is_ir_frame(videoFrame->type()) && videoFrame->format() == OB_FORMAT_Y8) {
		mIRRawMat = cv::Mat(videoFrame->height(), videoFrame->width(), CV_8UC1, videoFrame->data());

		cv::cvtColor(mIRRawMat, mIRMat, cv::COLOR_GRAY2RGB);
	}
	else if (is_ir_frame(videoFrame->type()) && videoFrame->format() == OB_FORMAT_MJPG) {
		cv::Mat rawMat(1, videoFrame->dataSize(), CV_8UC1, videoFrame->data());
		mIRMat = cv::imdecode(rawMat, 1);
	}

	if (mIsCapturing[2]) {
		if (mPreviousFrameIdx[2] != frame->index()) {
			mFrameCount[2]++;
			mPreviousFrameIdx[2] = frame->index();
		}
	}

	return &mIRMat;
}

