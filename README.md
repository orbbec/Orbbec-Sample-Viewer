# Orbbec Sample Viewer
An open source viewer for Orbbec cameras

[Orbbec SDK](https://github.com/orbbec/OrbbecSDK)

A sample GUI viewer integrated with Orbbec SDK and [ImGui](https://github.com/ocornut/imgui) to demostrate some basic operations that our SDK is capable of.

### Build

Set the Orbbec SDK and OpenCV library paths within orbbec_sample_viewer/CMakeLists.txt

```bash
cd orbbec_sample_viewer && mkdir build && cd build && cmake .. && cmake --build . --config Release
```

### Run example

Connect your Orbbec camera to your PC, proceed the following steps:

Copy the necessary librarie files(DLLs) from corresponding SDK folder to output folder first on Windows machine

``` bash
cd orbbec_sample_viewer/build/bin	# build output dir
./orbbec_sample_viewer              # orbbec_sample_viewer.exe on Windows machine
```
