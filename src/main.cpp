// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "service.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <stdio.h>
#include <fstream>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif
#pragma warning(disable : 4996) //_CRT_SECURE_NO_WARNINGS

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Convert OpenCV Mat to texture for rendering
void mat2texture(cv::Mat* source, GLuint& target)
{
    if (source->empty() || source == NULL || (const unsigned char*)source->data == NULL) return;
    if (target != 0) glDeleteTextures(1, &target);

    cv::Mat tmpMat;
    if (source->channels() == 3)
        tmpMat = *source;
    else if (source->channels() == 1)
        cv::cvtColor(*source, tmpMat, CV_GRAY2RGB);

    unsigned char* image = tmpMat.data;

    if (image == 0) return;
    int w = tmpMat.cols;
    int h = tmpMat.rows;

    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    tmpMat.release();
}

void objectDisableBegin()
{
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}
void objectDisableEnd()
{
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}

void qNormalizeAngle(int& angle)
{
    while (angle < 0) angle += 360 * 16;
    while (angle > 360 * 16) angle -= 360 * 16;
}

void setXRotation(int angle, int& target)
{
    qNormalizeAngle(angle);
    if (angle != target) target = angle;
}

void setYRotation(int angle, int& target)
{
    qNormalizeAngle(angle);
    if (angle != target) target = angle;
}

void setZRotation(int angle, int& target)
{
    qNormalizeAngle(angle);
    if (angle != target) target = angle;
}

// Save point cloud data to ply
void savePointsToPly(std::vector<OBColorPoint> points, std::string fileName) {
    int   pointsSize = points.size();
    FILE* fp = fopen(fileName.c_str(), "wb+");
    fprintf(fp, "ply\n");
    fprintf(fp, "format ascii 1.0\n");
    fprintf(fp, "element vertex %d\n", pointsSize);
    fprintf(fp, "property float x\n");
    fprintf(fp, "property float y\n");
    fprintf(fp, "property float z\n");
    fprintf(fp, "property uchar red\n");
    fprintf(fp, "property uchar green\n");
    fprintf(fp, "property uchar blue\n");
    fprintf(fp, "end_header\n");

    for (int i = 0; i < pointsSize; i++) {
        fprintf(fp, "%.3f %.3f %.3f %d %d %d\n", points[i].x, points[i].y, points[i].z, (int)points[i].r, (int)points[i].g, (int)points[i].b);
    }

    fflush(fp);
    fclose(fp);
}

bool g_isIRUnique = true;

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    int window_width = 1280, window_height = 720;
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Orbbec Sample Viewer", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // ImGui object color definition
    ImVec4 clear_color      = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImVec4 gray_normal      = ImVec4(0.48f, 0.48f, 0.48f, 1.00f);
    ImVec4 gray_hovered     = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    ImVec4 gray_active      = ImVec4(0.71f, 0.71f, 0.71f, 1.00f);
    ImVec4 blue_normal      = ImVec4(0.17f, 0.35f, 0.72f, 1.00f);
    ImVec4 blue_hovered     = ImVec4(0.13f, 0.39f, 0.90f, 0.40f);
    float icon_window_height    = 48.0f;
    float ctrl_window_width     = 300.0f;
    float stream_btn_width      = 86.0f;
    float ctrl_btn_width        = 40.0f;
    float ctrl_obj_spacing      = 210.0f;
    cv::Point2f streaming_window;

    const static auto flags_icon_window = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    int state = 0;
    Service* ob_service = new Service(state);
    std::string ob_device;
    bool is_booting = true;

    // color, depth, ir
    std::vector<std::string>* ob_stream_res_vec[3];
    int ob_current_mode[3]      = { 0, 0, 0 };
    bool is_streaming[4]        = { 0, 0, 0, 0 };
    bool is_saving[3]           = { 0, 0, 0 };
    bool is_mirror[3]           = { 0, 0, 0 };
    bool is_flip[3]             = { 0, 0, 0 };
    PropertyInfo_S<int> auto_exp[3];
    int exposure[3]             = { 0, 0, 0 };
    PropertyInfo_S<int> gain_range[3];
    int gain[3]                 = { 0, 0, 0 };
    bool auto_white_balance     = false;
    bool is_HW_D2C              = false;
    bool is_save_ply            = false;
    bool is_save_img            = false;
    bool is_export_cam_param    = false;
    GLuint ob_disp_texture[3]   = { 0, 0, 0 };
    cv::Mat* ob_disp_mat[3];
    std::string switch_label;
    int depth_disp_range[2]   = { 100, 5000 };

    // Point Cloud
    int xRot = 0, yRot = 0, zRot = 0;
    double xTrans = 0.0, yTrans = 0.0, zTrans = -12.0;
    double xZoomScale = 0.5, yZoomScale = 0.5, zZoomScale = 0.5;
    const double zoomScaleStep = 0.1;
    const float mouseRotateRatio = 1.0f;
    const double mouseTransRatio = 1000.0;
    std::vector<OBColorPoint> cloud_points;
    bool is_color = false;

    // OpenCV
    bool is_gaussian_blur = false;
    bool is_blur = false;
    int filter_size[2] = { 5, 5 };

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        glfwGetWindowSize(window, &window_width, &window_height);
        streaming_window.x = (float)window_width - ctrl_window_width;
        streaming_window.y = (float)window_height - icon_window_height;
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int streaming_check = std::accumulate(is_streaming, is_streaming + 4, 0);
        if (!is_booting && streaming_check > 0) {
            if (is_streaming[3]) {
                cloud_points.clear();
                ob_service->getPointCloudPoints(cloud_points, is_color);
            }
            else
                ob_service->readFrame();
        }
        if (is_booting) {
            // Wait for GUi to be ready
            ob_device = ob_service->getSensorStrList()->at(0);
            ob_stream_res_vec[0] = ob_service->getSensorInfo(OBSensorType::OB_SENSOR_COLOR);
            ob_stream_res_vec[1] = ob_service->getSensorInfo(OBSensorType::OB_SENSOR_DEPTH);
            ob_stream_res_vec[2] = ob_service->getSensorInfo(OBSensorType::OB_SENSOR_IR);

            // Make sure all stream modes are set to default mode
            ob_service->setColorVideoMode(ob_current_mode[0]);
            ob_service->setDepthVideoMode(ob_current_mode[1]);
            ob_service->setIRVideoMode(ob_current_mode[2]);

            is_mirror[0] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_COLOR);
            is_mirror[1] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_DEPTH);
            is_mirror[2] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_IR);

            is_flip[0] = ob_service->getFlipState(OBSensorType::OB_SENSOR_COLOR);
            is_flip[1] = ob_service->getFlipState(OBSensorType::OB_SENSOR_DEPTH);
            is_flip[2] = ob_service->getFlipState(OBSensorType::OB_SENSOR_IR);

            auto_exp[0] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_COLOR);
            auto_exp[1] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_DEPTH);
            auto_exp[2] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_IR);

            exposure[0] = ob_service->getExposureValue();
            exposure[1] = ob_service->getDepthExposureValue();
            exposure[2] = ob_service->getIRExposureValue();

            gain_range[0] = ob_service->getGainRange(OBSensorType::OB_SENSOR_COLOR);
            gain_range[1] = ob_service->getGainRange(OBSensorType::OB_SENSOR_DEPTH);
            gain_range[2] = ob_service->getGainRange(OBSensorType::OB_SENSOR_IR);

            gain[0] = ob_service->getColorGainValue();
            gain[1] = ob_service->getDepthGainValue();
            gain[2] = ob_service->getIRGainValue();

            auto_white_balance = ob_service->getAutoWhiteBalanceStatus();

            ob_service->getDepthDispRange(depth_disp_range);

            is_booting = false;
        }
        if (is_export_cam_param) {
            OBCameraParam camParams = ob_service->getCameraParams();
            string filename = "CameraParameters_";
            string tmpStr = ob_device.substr(0, ob_device.find_last_of("#") - 1);
            filename.append(tmpStr).append("_");
            filename.append(getCurrentDateTime(true)).append(".ini");

            std::ofstream outFile;
            outFile.open(filename);
            outFile << ob_device << endl;
            outFile << "IR fx" << " = " << camParams.depthIntrinsic.fx << endl;
            outFile << "IR fy" << " = " << camParams.depthIntrinsic.fy << endl;
            outFile << "IR cx" << " = " << camParams.depthIntrinsic.cx << endl;
            outFile << "IR cy" << " = " << camParams.depthIntrinsic.cy << endl;

            outFile << "RGB fx" << " = " << camParams.rgbIntrinsic.fx << endl;
            outFile << "RGB fy" << " = " << camParams.rgbIntrinsic.fy << endl;
            outFile << "RGB cx" << " = " << camParams.rgbIntrinsic.cx << endl;
            outFile << "RGB cy" << " = " << camParams.rgbIntrinsic.cy << endl;

            for (int i = 0; i < 9; i++) outFile << "rot[" << i << "] = " << camParams.transform.rot[i] << endl;
            for (int i = 0; i < 3; i++) outFile << "trans[" << i << "] = " << camParams.transform.trans[i] << endl;

            outFile << "k1 = " << camParams.depthDistortion.k1 << endl;
            outFile << "k2 = " << camParams.depthDistortion.k2 << endl;
            outFile << "k3 = " << camParams.depthDistortion.k3 << endl;
            outFile << "k4 = " << camParams.depthDistortion.k4 << endl;
            outFile << "k5 = " << camParams.depthDistortion.k5 << endl;
            outFile << "k6 = " << camParams.depthDistortion.k6 << endl;
            outFile << "p1 = " << camParams.depthDistortion.p1 << endl;
            outFile << "p2 = " << camParams.depthDistortion.p2 << endl;
            outFile.close();

            is_export_cam_param = false;
        }
        if (is_save_img) {
            is_save_img = ob_service->isFrameCapturing();
            if (!is_save_img) {
                for (int i = 0; i < 3; i++) is_saving[i] = false;
            }
        }

        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ (float)window_width, icon_window_height });
        ImGui::Begin("Icon Window", nullptr, flags_icon_window);
        // Display device information, the first one from getSensorStrList ONLY
        ImGui::Text(ob_device.c_str());
        ImGui::SameLine(400.f);

        if (!is_streaming[3]) {
            // Color button control
            if (is_streaming[0]) {
                ImGui::PushStyleColor(ImGuiCol_Button, blue_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blue_hovered);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, gray_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gray_hovered);
            }
            if (ImGui::Button("Color", ImVec2(stream_btn_width, 0.f))) {
                is_streaming[0] = !is_streaming[0];
                ob_service->switchColorStream(is_streaming[0]);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();

            // Depth button control
            if (is_streaming[1]) {
                ImGui::PushStyleColor(ImGuiCol_Button, blue_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blue_hovered);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, gray_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gray_hovered);
            }
            if (ImGui::Button("Depth", ImVec2(stream_btn_width, 0.f))) {
                is_streaming[1] = !is_streaming[1];
                ob_service->switchDepthStream(is_streaming[1]);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();

            // IR button control
            if (is_streaming[2]) {
                ImGui::PushStyleColor(ImGuiCol_Button, blue_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blue_hovered);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, gray_normal);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gray_hovered);
            }
            if (ImGui::Button("IR", ImVec2(stream_btn_width, 0.f))) {
                is_streaming[2] = !is_streaming[2];
                ob_service->switchIRStream(is_streaming[2]);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
        }
        if (is_streaming[3]) {
            ImGui::PushStyleColor(ImGuiCol_Button, blue_normal);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blue_hovered);
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, gray_normal);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gray_hovered);
        }
        if (ImGui::Button("Point Cloud", ImVec2(stream_btn_width, 0.f))) {
            is_streaming[3] = !is_streaming[3];
            if (!is_streaming[0]) {
                ob_service->switchColorStream(is_streaming[3]);
            }
            if (!is_streaming[1]) {
                ob_service->switchDepthStream(is_streaming[3]);
            }
            if (!is_HW_D2C) {
                is_HW_D2C = !is_HW_D2C;
                ob_service->toggleD2CAlignment(0);
            }
            ob_service->togglePointCloud();
        }
        ImGui::PopStyleColor(2);
        ImGui::End();

        ImGui::SetNextWindowPos({ 0, icon_window_height });
        ImGui::SetNextWindowSize({ ctrl_window_width, (float)window_height - icon_window_height });
        ImGui::Begin("Ctrl Window", nullptr, flags_icon_window);
        if (!is_streaming[3]) {
            // Color
            if (ImGui::CollapsingHeader("Color")) {
                static std::string current_color_str = ob_stream_res_vec[0]->at(ob_current_mode[0]);
                if (ImGui::BeginCombo("##Color Supported List", current_color_str.c_str())) {
                    for (int n = 0; n < ob_stream_res_vec[0]->size(); n++) {
                        bool isSelected = (current_color_str == ob_stream_res_vec[0]->at(n));
                        if (ImGui::Selectable(ob_stream_res_vec[0]->at(n).c_str(), isSelected)) {
                            if (current_color_str != ob_stream_res_vec[0]->at(n)) {
                                current_color_str = ob_stream_res_vec[0]->at(n);
                                ob_service->setColorVideoMode(n);
                                ob_current_mode[0] = n;
                                break;
                            }
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Toggle button for Color Mirror
                switch_label = is_mirror[0] ? "ON" : "OFF";
                ImGui::PushID("Color Mirror");
                ImGui::Text("Mirror");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleMirror(OBSensorType::OB_SENSOR_COLOR);
                    is_mirror[0] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_COLOR);
                }
                ImGui::PopID();

                // Toggle button for Color Flip
                switch_label = is_flip[0] ? "ON" : "OFF";
                ImGui::PushID("Color Flip");
                ImGui::Text("Flip");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleFlip(OBSensorType::OB_SENSOR_COLOR);
                    is_flip[0] = ob_service->getFlipState(OBSensorType::OB_SENSOR_COLOR);
                }
                ImGui::PopID();

                // Toggle button for Color Auto White Balance
                switch_label = auto_white_balance ? "ON" : "OFF";
                ImGui::PushID("Color Auto WB");
                ImGui::Text("Auto White Balance");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    auto_white_balance = !auto_white_balance;
                    ob_service->toggleAutoWhiteBalance(auto_white_balance);
                }
                ImGui::PopID();

                if (auto_exp[0].max > 0 && auto_exp[0].min > 0) {
                    // Toggle button for Color Auto Exposure
                    switch_label = auto_exp[0].state ? "ON" : "OFF";
                    ImGui::PushID("Color Auto Exposure");
                    ImGui::Text("Auto Exposure");
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                    if (ImGui::IsItemClicked(0)) {
                        auto_exp[0].state = !auto_exp[0].state;
                        ob_service->toggleAutoExposure(auto_exp[0].state);
                        auto_exp[0] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_COLOR);
                        if (!auto_exp[0].state) {
                            exposure[0] = ob_service->getExposureValue();
                            gain[0] = ob_service->getColorGainValue();
                        }
                    }
                    // Adjust Exposure and Gain values is available when auto exposure is off
                    if (auto_exp[0].state) objectDisableBegin();
                    // Adjust Exposure value
                    ImGui::Text("Adjust Exposure");
                    ImGui::SliderInt("##ColorExpoValue", &exposure[0], auto_exp[0].min, auto_exp[0].max);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ob_service->setExposureValue(exposure[0]);
                    }
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Text("Adjust");
                    ImGui::PopID();
                }

                // Adjust Gain value
                ImGui::PushID("Color Gain");
                ImGui::Text("Adjust Gain");
                ImGui::SliderInt("##ColorGainValue", &gain[0], gain_range[0].min, gain_range[0].max);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    ob_service->setColorGainValue(gain[0]);
                }
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Text("Adjust");
                ImGui::PopID();
                if (auto_exp[0].state) objectDisableEnd();
            }
            // Depth
            if (ImGui::CollapsingHeader("Depth")) {
                static std::string current_depth_str = ob_stream_res_vec[1]->at(ob_current_mode[1]);
                if (ImGui::BeginCombo("##Depth Supported List", current_depth_str.c_str())) {
                    for (int n = 0; n < ob_stream_res_vec[1]->size(); n++) {
                        bool isSelected = (current_depth_str == ob_stream_res_vec[1]->at(n));
                        if (ImGui::Selectable(ob_stream_res_vec[1]->at(n).c_str(), isSelected)) {
                            if (current_depth_str != ob_stream_res_vec[1]->at(n)) {
                                current_depth_str = ob_stream_res_vec[1]->at(n);
                                ob_service->setDepthVideoMode(n);
                                ob_current_mode[1] = n;
                                break;
                            }
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // Toggle button for Depth Mirror
                switch_label = is_mirror[1] ? "ON" : "OFF";
                ImGui::PushID("Depth Mirror");
                ImGui::Text("Mirror");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleMirror(OBSensorType::OB_SENSOR_DEPTH);
                    is_mirror[1] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_DEPTH);
                }
                ImGui::PopID();

                // Toggle button for Depth Flip
                switch_label = is_flip[1] ? "ON" : "OFF";
                ImGui::PushID("Depth Flip");
                ImGui::Text("Flip");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleFlip(OBSensorType::OB_SENSOR_DEPTH);
                    is_flip[1] = ob_service->getFlipState(OBSensorType::OB_SENSOR_DEPTH);
                }
                ImGui::PopID();

                if (auto_exp[1].max > 0 && auto_exp[1].min > 0) {
                    // Toggle button for Depth Auto Exposure
                    switch_label = auto_exp[1].state ? "ON" : "OFF";
                    ImGui::PushID("Depth Auto Exposure");
                    ImGui::Text("Auto Exposure");
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                    if (ImGui::IsItemClicked(0)) {
                        auto_exp[1].state = !auto_exp[1].state;
                        ob_service->toggleDepthAutoExposure(auto_exp[1].state);
                        auto_exp[1] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_DEPTH);
                        if (!auto_exp[1].state) {
                            exposure[1] = ob_service->getDepthExposureValue();
                            gain[1] = ob_service->getDepthGainValue();
                        }
                    }

                    // Adjust Exposure and Gain values is available when auto exposure is off
                    if (auto_exp[1].state) objectDisableBegin();
                    // Adjust Exposure value
                    ImGui::Text("Adjust Exposure");
                    ImGui::SliderInt("##DepthExpoValue", &exposure[1], auto_exp[1].min, auto_exp[1].max);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ob_service->setDepthExposureValue(exposure[1]);
                    }
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Text("Adjust");
                    ImGui::PopID();

                    // Adjust Gain value
                    ImGui::PushID("Depth Gain");
                    ImGui::Text("Adjust Gain");
                    ImGui::SliderInt("##DepthGainValue", &gain[1], gain_range[1].min, gain_range[1].max);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ob_service->setDepthGainValue(gain[1]);
                    }
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Text("Adjust");
                    ImGui::PopID();
                    if (auto_exp[1].state) objectDisableEnd();
                }

                // Toggle button for D2C Alignment
                switch_label = is_HW_D2C ? "ON" : "OFF";
                ImGui::PushID("Depth D2C");
                ImGui::Text("D2C");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    is_HW_D2C = !is_HW_D2C;
                    ob_service->toggleD2CAlignment(0);
                }
                ImGui::PopID();

                ImGui::Separator();
                ImGui::Text("Visualization");
                ImGui::Text("Display Range (min.)");
                ImGui::SliderInt("##DepthDispRangeMin", &depth_disp_range[0], 0, 12000);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    ob_service->setDepthDispRange(depth_disp_range);
                }
                if (depth_disp_range[0] > depth_disp_range[1]) depth_disp_range[1] = depth_disp_range[0];
                ImGui::Text("Display Range (max.)");
                ImGui::SliderInt("##DepthDispRangeMax", &depth_disp_range[1], 0, 12000);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    ob_service->setDepthDispRange(depth_disp_range);
                }
                if (depth_disp_range[1] < depth_disp_range[0]) depth_disp_range[1] = depth_disp_range[0];
            }
            // IR
            if (ImGui::CollapsingHeader("IR")) {
                static std::string current_ir_str = ob_stream_res_vec[2]->at(ob_current_mode[2]);
                if (ImGui::BeginCombo("##IR Supported List", current_ir_str.c_str())) {
                    for (int n = 0; n < ob_stream_res_vec[2]->size(); n++) {
                        bool isSelected = (current_ir_str == ob_stream_res_vec[2]->at(n));
                        if (ImGui::Selectable(ob_stream_res_vec[2]->at(n).c_str(), isSelected)) {
                            if (current_ir_str != ob_stream_res_vec[2]->at(n)) {
                                current_ir_str = ob_stream_res_vec[2]->at(n);
                                ob_service->setIRVideoMode(n);
                                ob_current_mode[2] = n;
                                break;
                            }
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // Toggle button for IR mirror
                switch_label = is_mirror[2] ? "ON" : "OFF";
                ImGui::PushID("IR Mirror");
                ImGui::Text("Mirror");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleMirror(OBSensorType::OB_SENSOR_IR);
                    is_mirror[2] = ob_service->getMirrorState(OBSensorType::OB_SENSOR_IR);
                }
                ImGui::PopID();

                // Toggle button for IR Flip
                switch_label = is_flip[2] ? "ON" : "OFF";
                ImGui::PushID("IR Flip");
                ImGui::Text("Flip");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    ob_service->toggleFlip(OBSensorType::OB_SENSOR_IR);
                    is_flip[2] = ob_service->getFlipState(OBSensorType::OB_SENSOR_IR);
                }
                ImGui::PopID();

                if (auto_exp[2].max > 0 && auto_exp[2].min > 0) {
                    // Toggle button for IR Auto Exposure
                    switch_label = auto_exp[2].state ? "ON" : "OFF";
                    ImGui::PushID("IR Auto Exposure");
                    ImGui::Text("Auto Exposure");
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                    if (ImGui::IsItemClicked(0)) {
                        auto_exp[2].state = !auto_exp[2].state;
                        ob_service->toggleIRAutoExposure(auto_exp[2].state);
                        auto_exp[2] = ob_service->getAutoExposureStatus(OBSensorType::OB_SENSOR_IR);
                        if (!auto_exp[2].state) {
                            exposure[2] = ob_service->getIRExposureValue();
                            gain[2] = ob_service->getDepthGainValue();
                        }
                    }

                    // Adjust Exposure and Gain values is available when auto exposure is off
                    if (auto_exp[2].state) objectDisableBegin();
                    // Adjust Exposure value
                    ImGui::Text("Adjust Exposure");
                    ImGui::SliderInt("##IRExpoValue", &exposure[2], auto_exp[2].min, auto_exp[2].max);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ob_service->setIRExposureValue(exposure[2]);
                    }
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Text("Adjust");
                    ImGui::PopID();

                    // Adjust Gain value
                    ImGui::PushID("IR Gain");
                    ImGui::Text("Adjust Gain");
                    ImGui::SliderInt("##IRGainValue", &gain[2], gain_range[2].min, gain_range[2].max);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        ob_service->setDepthGainValue(gain[2]);
                    }
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Text("Adjust");
                    ImGui::PopID();
                    if (auto_exp[2].state) objectDisableEnd();
                }
            }
            // OpenCV
            if (ImGui::CollapsingHeader("OpenCV")) {
                ImGui::Text("2D Processing");
                ImGui::Separator();
                ImGui::Text("Color");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Text("TBD\n");
                ImGui::Separator();

                {
                    ImGui::Text("Depth");
                    // Toggle button for cv::GaussianBlur
                    switch_label = is_gaussian_blur ? "ON" : "OFF";
                    ImGui::PushID("Gaussian");
                    ImGui::Text("Gaussian Blur");
                    ImGui::SliderInt("##GaussianBlurFilterSZ", &filter_size[0], 1, 9);
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                    if (ImGui::IsItemClicked(0)) {
                        is_gaussian_blur = !is_gaussian_blur;
                    }
                    ImGui::PopID();

                    // Toggle button for cv::blur
                    switch_label = is_blur ? "ON" : "OFF";
                    ImGui::PushID("Blur");
                    ImGui::Text("Blur");
                    ImGui::SliderInt("##BlurFilterSZ", &filter_size[1], 1, 9);
                    ImGui::SameLine(ctrl_obj_spacing);
                    ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                    if (ImGui::IsItemClicked(0)) {
                        is_blur = !is_blur;
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();

                ImGui::Text("\nIR");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Text("TBD");
            }
        }
        else {
            // Point Cloud
            if (ImGui::CollapsingHeader("Point Cloud")) {
                ImGui::Text("Current Color Resolution");
                ImGui::Text(ob_stream_res_vec[0]->at(ob_current_mode[0]).c_str());
                ImGui::Text("Current Depth Resolution");
                ImGui::Text(ob_stream_res_vec[1]->at(ob_current_mode[1]).c_str());
                ImGui::Checkbox("Map W Color", &is_color);
            }
        }
        // Data Management
        if (ImGui::CollapsingHeader("Data Management")) {
            if (!is_streaming[3]) {
                static int int_frame_num = 1;
                // Toggle button for saving current frame data
                switch_label = is_save_img ? "Saving" : "Save";
                ImGui::Text("Frame Capturing");
                if (streaming_check == 0) objectDisableBegin();
                ImGui::PushID("Save Image");
                if (!is_streaming[0]) objectDisableBegin();
                ImGui::Checkbox("COLOR", &is_saving[0]);
                if (!is_streaming[0]) objectDisableEnd();
                ImGui::SameLine();
                if (!is_streaming[1]) objectDisableBegin();
                ImGui::Checkbox("DEPTH", &is_saving[1]);
                if (!is_streaming[1]) objectDisableEnd();
                ImGui::SameLine();
                if (!is_streaming[2]) objectDisableBegin();
                ImGui::Checkbox("IR", &is_saving[2]);
                if (!is_streaming[2]) objectDisableEnd();
                ImGui::Text("CapturedFrames(frame)");
                ImGui::InputInt("##frame num", &int_frame_num, 0, 0);
                ImGui::SameLine(ctrl_obj_spacing);

                int capturing_check = std::accumulate(is_saving, is_saving + 3, 0);
                if (!capturing_check) objectDisableBegin();
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    is_save_img = true;
                    ob_service->startFrameCapturing(is_saving, int_frame_num);
                }
                if (!capturing_check) objectDisableEnd();
                ImGui::PopID();
                if (streaming_check == 0) objectDisableEnd();
            }
            else {
                // Toggle button for exporting point cloud data to PLY file
                switch_label = is_save_ply ? "Saving" : "Save";
                ImGui::PushID("Save PLY");
                ImGui::Text("Save PLY");
                ImGui::SameLine(ctrl_obj_spacing);
                ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
                if (ImGui::IsItemClicked(0)) {
                    is_save_ply = true;
                }
                ImGui::PopID();
            }
            // Toggle button for exporting camera parameter
            switch_label = is_export_cam_param ? "Saving" : "Save";
            ImGui::PushID("Save Camera Param");
            ImGui::Text("Export Camera Parameters");
            ImGui::SameLine(ctrl_obj_spacing);
            ImGui::Button(switch_label.c_str(), ImVec2({ ctrl_btn_width, 0.0f }));
            if (ImGui::IsItemClicked(0)) {
                is_export_cam_param = true;
            }
            ImGui::PopID();
        }
        ImGui::End();

        if (is_streaming[3]) {
            ImGui::SetNextWindowPos({ ctrl_window_width, icon_window_height });

            static bool isMouseClicked[2] = { 0, 0 };
            ImVec2 mouseValue;
            ImVec2 base_pos = ImGui::GetCursorScreenPos();
            ImVec2 cell(base_pos.x, base_pos.y);
            ImVec2 floor(cell.x + streaming_window.x, cell.y + streaming_window.y);

            // Within the display window
            if (ImGui::GetMousePos().x > cell.x && ImGui::GetMousePos().x < floor.x && ImGui::GetMousePos().y > cell.y && ImGui::GetMousePos().y < floor.y) {
                if (ImGui::IsMouseClicked(0)) isMouseClicked[0] = true;
                if (ImGui::IsMouseClicked(1)) isMouseClicked[1] = true;
                if (ImGui::IsMouseReleased(0)) isMouseClicked[0] = false;
                if (ImGui::IsMouseReleased(1)) isMouseClicked[1] = false;
                if (io.MouseWheel < 0) {
                    if (xZoomScale > 0.1) xZoomScale -= zoomScaleStep;
                    if (yZoomScale > 0.1) yZoomScale -= zoomScaleStep;
                    if (zZoomScale > 0.1) zZoomScale -= zoomScaleStep;
                }
                if (io.MouseWheel > 0) {
                    xZoomScale += zoomScaleStep;
                    yZoomScale += zoomScaleStep;
                    zZoomScale += zoomScaleStep;
                }
                if (ImGui::IsMouseDown(0) && (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0) && isMouseClicked[0]) {
                    mouseValue = ImGui::GetIO().MouseDelta;
                    setXRotation(xRot + mouseRotateRatio * mouseValue.y, xRot);
                    setYRotation(yRot + mouseRotateRatio * mouseValue.x, yRot);
                }
                if (ImGui::IsMouseDown(1) && (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0) && isMouseClicked[1]) {
                    mouseValue = ImGui::GetIO().MouseDelta;
                    xTrans += mouseValue.x / mouseTransRatio;
                    yTrans -= mouseValue.y / mouseTransRatio;
                }
                if (ImGui::IsMouseDown(2) && (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0)) {
                    mouseValue = ImGui::GetIO().MouseDelta;
                    setZRotation(zRot + mouseRotateRatio * mouseValue.x, zRot);
                }
            }
        }
        else {
            ImGui::SetNextWindowPos({ ctrl_window_width, icon_window_height });
            ImGui::SetNextWindowSize({ streaming_window.x / 2, streaming_window.y / 2 });
            ImGui::Begin("ColorStream", nullptr, flags_icon_window);
            if (is_streaming[0]) {
                ob_disp_mat[0] = ob_service->getColorMat();
                if (ob_disp_mat[0] != nullptr) {
                    float disp_ratio = streaming_window.x / 2.06f / ob_disp_mat[0]->cols;
                    float temp_ratio = streaming_window.y / 2.06f / ob_disp_mat[0]->rows;
                    mat2texture(ob_disp_mat[0], ob_disp_texture[0]);
                    if (disp_ratio > temp_ratio) {
                        disp_ratio = temp_ratio;
                        ImGui::SetCursorPosX(abs(streaming_window.x / 2 - ob_disp_mat[0]->cols * disp_ratio) / 2);
                    }
                    ImGui::SetCursorPosY(abs(streaming_window.y / 2 - ob_disp_mat[0]->rows * disp_ratio) / 2);
                    ImGui::Image((void*)(intptr_t)ob_disp_texture[0], ImVec2(ob_disp_mat[0]->cols * disp_ratio, ob_disp_mat[0]->rows * disp_ratio));
                }
            }
            ImGui::End();

            ImGui::SetNextWindowPos({ ctrl_window_width + streaming_window.x / 2, icon_window_height });
            ImGui::SetNextWindowSize({ streaming_window.x / 2, streaming_window.y / 2 });
            ImGui::Begin("DepthStream", nullptr, flags_icon_window);
            if (is_streaming[1]) {
                ob_disp_mat[1] = ob_service->getDepthMat();
                if (ob_disp_mat[1] != nullptr) {
                    float disp_ratio = streaming_window.x / 2.06f / ob_disp_mat[1]->cols;
                    float temp_ratio = streaming_window.y / 2.06f / ob_disp_mat[1]->rows;
                    cv::Mat disp_mat = ob_disp_mat[1]->clone();
                    if (is_gaussian_blur) {
                        cv::GaussianBlur(disp_mat, disp_mat, cv::Size(filter_size[0], filter_size[0]), 0, 0);
                    }
                    else if (is_blur) {
                        cv::blur(disp_mat, disp_mat, cv::Size(filter_size[1], filter_size[1]));
                    }
                    mat2texture(&disp_mat, ob_disp_texture[1]);
                    if (disp_ratio > temp_ratio) {
                        disp_ratio = temp_ratio;
                        ImGui::SetCursorPosX(abs(streaming_window.x / 2 - disp_mat.cols * disp_ratio) / 2);
                    }
                    ImGui::SetCursorPosY(abs(streaming_window.y / 2 - disp_mat.rows * disp_ratio) / 2);
                    ImGui::Image((void*)(intptr_t)ob_disp_texture[1], ImVec2(disp_mat.cols * disp_ratio, disp_mat.rows * disp_ratio));
                }
            }
            ImGui::End();

            ImGui::SetNextWindowPos({ ctrl_window_width, icon_window_height + streaming_window.y / 2 });
            ImGui::SetNextWindowSize({ streaming_window.x / 2, streaming_window.y / 2 });
            ImGui::Begin("IRStream", nullptr, flags_icon_window);
            if (is_streaming[2]) {
                ob_disp_mat[2] = ob_service->getIRMat();
                if (ob_disp_mat[2] != nullptr) {
                    float disp_ratio = streaming_window.x / 2.06f / ob_disp_mat[2]->cols;
                    float temp_ratio = streaming_window.y / 2.06f / ob_disp_mat[2]->rows;
                    mat2texture(ob_disp_mat[2], ob_disp_texture[2]);
                    if (disp_ratio > temp_ratio) {
                        disp_ratio = temp_ratio;
                        ImGui::SetCursorPosX(abs(streaming_window.x / 2 - ob_disp_mat[2]->cols * disp_ratio) / 2);
                    }
                    ImGui::SetCursorPosY(abs(streaming_window.y / 2 - ob_disp_mat[2]->rows * disp_ratio) / 2);
                    ImGui::Image((void*)(intptr_t)ob_disp_texture[2], ImVec2(ob_disp_mat[2]->cols * disp_ratio, ob_disp_mat[2]->rows * disp_ratio));
                }
            }
            ImGui::End();

            ImGui::SetNextWindowPos({ ctrl_window_width + streaming_window.x / 2, icon_window_height + streaming_window.y / 2 });
            ImGui::SetNextWindowSize({ streaming_window.x / 2, streaming_window.y / 2 });
            ImGui::Begin("EmptyStream", nullptr, flags_icon_window);

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Point Cloud rendering
        if (is_streaming[3]) {
            glViewport(ctrl_window_width, 0, streaming_window.x, streaming_window.y);
            glScissor(ctrl_window_width, 0, streaming_window.x, streaming_window.y);

            glEnable(GL_SCISSOR_TEST);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            if (!cloud_points.empty()) {
                if (is_save_ply) {
                    savePointsToPly(cloud_points, "./pointcloud.ply");
                    is_save_ply = false;
                }
                glLoadIdentity();
                glOrtho(-2.0, 2.0, -2.0, +2.0, -1.0, 15.0);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                glTranslatef(xTrans, yTrans, zTrans);
                glScalef(xZoomScale, yZoomScale, zZoomScale);

                glRotatef(xRot / 16.0, 1.0, 0.0, 0.0);
                glRotatef(yRot / 16.0, 0.0, 1.0, 0.0);
                glRotatef(zRot / 16.0, 0.0, 0.0, 1.0);

                glPointSize(0.1f);
                glBegin(GL_POINTS);
                for (unsigned int i = 0; i < cloud_points.size(); i++) {
                    glColor3ub(cloud_points.at(i).r, cloud_points.at(i).g, cloud_points.at(i).b);
                    glVertex3f(cloud_points.at(i).x / 1000.0f, -cloud_points.at(i).y / 1000.0f, cloud_points.at(i).z / 1000.0f);
                }
                glEnd();

                glLineWidth(2.0);
                glBegin(GL_LINES);
                // draw line for x axis
                glColor3f(1.0, 0.0, 0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.0);
                glVertex3f((GLfloat)0.5, (GLfloat)0.0, (GLfloat)0.0);
                glVertex3f((GLfloat)0.46, (GLfloat)0.02, (GLfloat)0.0);
                glVertex3f((GLfloat)0.5, (GLfloat)0.0, (GLfloat)0.0);
                glVertex3f((GLfloat)0.46, (GLfloat)-0.02, (GLfloat)0.0);
                glVertex3f((GLfloat)0.5, (GLfloat)0.0, (GLfloat)0.0);
                // draw line for y axis
                glColor3f(0.0, 1.0, 0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.5, (GLfloat)0.0);
                glVertex3f((GLfloat)-0.02, (GLfloat)0.46, (GLfloat)0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.5, (GLfloat)0.0);
                glVertex3f((GLfloat)0.02, (GLfloat)0.46, (GLfloat)0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.5, (GLfloat)0.0);
                // draw line for Z axis
                glColor3f(0.0, 0.0, 1.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.0);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.5);
                glVertex3f((GLfloat)0.02, (GLfloat)0.0, (GLfloat)0.46);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.5);
                glVertex3f((GLfloat)-0.02, (GLfloat)0.0, (GLfloat)0.46);
                glVertex3f((GLfloat)0.0, (GLfloat)0.0, (GLfloat)0.5);
                glEnd();

                glPopMatrix();
                glPopMatrix();
                glFlush();
            }
            glDisable(GL_SCISSOR_TEST);
        }

        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    if (ob_service != nullptr) delete ob_service;

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
