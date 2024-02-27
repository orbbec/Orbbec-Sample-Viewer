#ifdef _WIN32
#include <Windows.h>
#include "direct.h"
#else
#include <sys/stat.h>
#include <sys/types.h>
#include "unistd.h"
#endif
#include <map>
#include <sstream>
#include <iomanip>
#include <string.h>

#define MAX_DEPTH           10000
#define DEFAULT_WIDTH       640
#define DEFAULT_HEIGHT      480
#define FPS_FRAME_COUNT		10
#define MAX_DEPTH           10000
#define DEFAULT_WIDTH       640
#define DEFAULT_HEIGHT      480
#define FPS_FRAME_COUNT		10

typedef struct SensorInfo_S {
    std::shared_ptr<ob::DeviceInfo> deviceInfo;
    char deviceName[32];
    char serialNum[12];
    int pid;
    int vid;
} SensorInfo_S;

template <typename T>
struct PropertyInfo_S {
    bool state;
    T min, max;
};

typedef struct FrameInfo_S {
    short w, h;
    double fps;
} FrameInfo_S;

typedef struct Point3D_S {
    float x, y, z;
} Point3D_S;

typedef enum {
	INIT_RESULT_SUCCESS = 0,
	INIT_RESULT_DEVICE_OPEN_FAIL = -1,
	INIT_RESULT_DEPTH_OPEN_FAIL = -2,
	INIT_RESULT_COLOR_OPEN_FAIL = -3,
	INIT_RESULT_IR_OPEN_FAIL = -4,
	INIT_RESULT_NO_DEVICE = -5,
	INIT_RESULT_DEVICE_SELECT_ERROR = -6,
} ServiceErrorCode;


inline float OBDepthPrecisionLevelToFloat(OBDepthPrecisionLevel level) {
	switch (level) {
	case OB_PRECISION_1MM:
		return 1.0f;
	case OB_PRECISION_0MM8:
		return 0.8f;
	case OB_PRECISION_0MM4:
		return 0.4f;
	case OB_PRECISION_0MM1:
		return 0.1f;
	case OB_PRECISION_0MM2:
		return 0.2f;
	default:
		return 1.f;			// return 1.0f when not support
	}
}

inline std::string OBFrameTypeToString(OBFrameType type) {
	switch (type) {
	case OB_FRAME_IR:
		return "IR";
	case OB_FRAME_IR_LEFT:
		return "IR_L";
	case OB_FRAME_IR_RIGHT:
		return "IR_R";
	case OB_FRAME_COLOR:
		return "Color";
	case OB_FRAME_DEPTH:
		return "Depth";
	case OB_FRAME_ACCEL:
	case OB_FRAME_GYRO:
		return "IMU";
	default:
		return "ERROR_TYPE";
	}
}

inline std::string OBSensorTypeToString(OBSensorType type) {
	switch (type) {
	case OB_SENSOR_UNKNOWN:
		return "UNKNOWN";
	case OB_SENSOR_IR:
		return "IR";
	case OB_SENSOR_IR_LEFT:
		return "IR_L";
	case OB_SENSOR_IR_RIGHT:
		return "IR_R";
	case OB_SENSOR_COLOR:
		return "Color";
	case OB_SENSOR_DEPTH:
		return "Depth";
	case OB_SENSOR_ACCEL:
	case OB_SENSOR_GYRO:
		return "IMU";
	default:
		return "ERROR_TYPE";
	}
}

inline OBFormat stringToOBFormat(std::string str_fmt) {
	static std::map<std::string, OBFormat> ob_format_map = { { "YUYV", OB_FORMAT_YUYV }, { "UYVY", OB_FORMAT_UYVY }, { "NV12", OB_FORMAT_NV12 }, { "NV21", OB_FORMAT_NV21 }, { "MJPG", OB_FORMAT_MJPG },
															 { "H264", OB_FORMAT_H264 }, { "H265", OB_FORMAT_HEVC }, { "I420", OB_FORMAT_I420 }, { "Y16", OB_FORMAT_Y16 },   { "RLE", OB_FORMAT_RLE } };
	auto itor = ob_format_map.find(str_fmt);
	if (itor != ob_format_map.end()) {
		return itor->second;
	}
	return OB_FORMAT_UNKNOWN;
}

inline std::string OBFormatToString(OBFormat fmt) {
	switch (fmt) {
	case OB_FORMAT_YUYV:
		return "YUYV";
	case OB_FORMAT_UYVY:
		return "UYVY";
	case OB_FORMAT_NV12:
		return "NV12";
	case OB_FORMAT_NV21:
		return "NV21";
	case OB_FORMAT_MJPG:
		return "MJPG";
	case OB_FORMAT_H264:
		return "H264";
	case OB_FORMAT_H265:
	case OB_FORMAT_HEVC:
		return "H265";
	case OB_FORMAT_I420:
		return "I420";
	case OB_FORMAT_RLE:
		return "RLE";
	case OB_FORMAT_Y16:
		return "Y16";
	case OB_FORMAT_Y8:
		return "Y8";
	default:
		return "ERROR_TYPE";
	}
}

inline std::string int2str(const int& int_temp)
{
	std::stringstream stream;
	stream << int_temp;
	return stream.str();   // Can be replaced with stream>>string_temp
}

inline std::string getCurrentDateTime(bool useLocalTime) {
	std::stringstream currentDateTime;
	// current date/time based on current system
	time_t pttNow = time(NULL);
	tm* ptmNow;

	if (useLocalTime) ptmNow = localtime(&pttNow);
	else ptmNow = gmtime(&pttNow);

	currentDateTime << 1900 + ptmNow->tm_year
		<< std::setfill('0') << std::setw(2) << (1 + ptmNow->tm_mon)
		<< std::setfill('0') << std::setw(2) << ptmNow->tm_mday << "_"
		<< std::setfill('0') << std::setw(2) << ptmNow->tm_hour
		<< std::setfill('0') << std::setw(2) << ptmNow->tm_min
		<< std::setfill('0') << std::setw(2) << ptmNow->tm_sec;

	return currentDateTime.str();
}

inline bool createSubDirectory(std::string folderDir)
{
#ifdef _WIN32
	if (_mkdir(folderDir.c_str())) {
#else
	if (mkdir(folderDir.c_str(), 0777)) {
#endif
		return true;
	}
	else {
		return false;
	}
}
