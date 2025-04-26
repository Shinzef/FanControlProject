# CMakeLists.txt
cmake_minimum_required(VERSION 3.15) # Minimum version for target_link_options

project(FanControlGUI CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Configuration ---
# Adjust these paths if necessary
set(SDL3_INCLUDE_DIR "C:/Users/rhyem/Downloads/SDL3/include")
set(SDL3_LIBRARY_DIR "C:/Users/rhyem/Downloads/SDL3/lib/x64")
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implot-0.16) # Add ImPlot directory

# Remove verbose flag as it makes output hard to read when debugging
set(CMAKE_EXE_LINKER_FLAGS "")

# Add ImGui sources
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

# --- Add Executable ---
add_executable(FanControlGUI
    gui_main.cpp
    fan_control.cpp
    winring_wrapper.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui_impl_sdlrenderer3.cpp
    ${IMPLOT_DIR}/implot.cpp             # Add ImPlot source
    ${IMPLOT_DIR}/implot_items.cpp       # Add ImPlot source
)

# --- Include Directories ---
target_include_directories(FanControlGUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # For fan_control.h, json.hpp
    ${SDL3_INCLUDE_DIR}
    ${IMGUI_DIR}
    ${IMPLOT_DIR} # Add ImPlot include directory
)

# --- Link Libraries ---
# First add the library search path with both lib and bin directories
target_link_directories(FanControlGUI PRIVATE 
    ${SDL3_LIBRARY_DIR}
    ${SDL3_BIN_DIR}
)

# Link libraries with correct order and explicit .lib extension for Windows
target_link_libraries(FanControlGUI PRIVATE 
    "C:/Users/rhyem/Downloads/SDL3/lib/x64/SDL3.lib"
    SDL3
    gdi32 
    user32
    ole32
    oleaut32
    opengl32 
    imm32 
    version 
    winmm
    setupapi
    shell32
)

# --- Linker Options for Windows GUI ---
if(WIN32)
    # Ensure SDL3 can find its DLLs at runtime
    target_compile_definitions(FanControlGUI PRIVATE 
        SDL_MAIN_HANDLED
    )
endif()

# --- Copy DLLs to Build Directory ---
# Ensure required DLLs are copied next to the executable
add_custom_command(TARGET FanControlGUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_LIBRARY_DIR}/SDL3.dll"
        $<TARGET_FILE_DIR:FanControlGUI>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/winring_wrapper.dll"
        $<TARGET_FILE_DIR:FanControlGUI>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WinRing0x64.dll"
        $<TARGET_FILE_DIR:FanControlGUI>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/WinRing0x64.sys"
        $<TARGET_FILE_DIR:FanControlGUI>
    COMMENT "Copying required DLLs..."
)
# --- Optional: Add ImGui defines ---
# target_compile_definitions(FanControlGUI PRIVATE IMGUI_IMPL_OPENGL_ES2) # If using OpenGL ES