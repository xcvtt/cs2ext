cmake_minimum_required(VERSION 3.20)
project(cs2ext)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.4
)
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(imgui json)

add_library(imgui_lib
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)


add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} imgui_lib nlohmann_json::nlohmann_json d3d11 d3dcompiler dwmapi)

target_link_options(cs2ext PRIVATE
        -static-libgcc
        -static-libstdc++
        -static -lpthread
        -Wl,--gc-sections
        -s
)
target_compile_options(cs2ext PRIVATE
        -Os
        -ffunction-sections
        -fdata-sections
)