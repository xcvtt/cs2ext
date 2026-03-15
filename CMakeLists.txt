cmake_minimum_required(VERSION 3.20)
project(cs2ext)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
        -DNOMINMAX
        -DIMGUI_DISABLE_DEMO_WINDOWS
        -DIMGUI_DISABLE_DEBUG_TOOLS
)

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

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
        imgui_lib
        nlohmann_json::nlohmann_json
        d3d11
        d3dcompiler
        dwmapi
        windowscodecs
        ole32
)

# Compiler specific flags
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /EHsc
            /Ob2
            /Oi
            /GL
            /GS-
            /Gy
            /Gw
    )

else()

    target_compile_options(${PROJECT_NAME} PRIVATE
            -Os
            -ffunction-sections
            -fdata-sections
    )

    target_link_options(${PROJECT_NAME} PRIVATE
            -static-libgcc
            -static-libstdc++
            -static
            -Wl,--gc-sections
            -s
    )

endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/icons $<TARGET_FILE_DIR:${PROJECT_NAME}>/icons
)