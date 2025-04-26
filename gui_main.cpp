#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>
#include <implot.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <chrono>
#include <numeric> // For std::iota
#include <algorithm> // For std::sort
#include <json.hpp> // Assuming json.hpp is from nlohmann
#include "fan_control.h"

// Helper function to convert vector<uint8_t> to vector<int> for ImGui sliders
std::vector<int> convertVecU8ToVecInt(const std::vector<uint8_t>& vec_u8) {
    std::vector<int> vec_int;
    vec_int.reserve(vec_u8.size());
    for (uint8_t val : vec_u8) {
        vec_int.push_back(static_cast<int>(val));
    }
    return vec_int;
}

// Helper function to convert vector<int> back to vector<uint8_t> 
std::vector<uint8_t> convertVecIntToVecU8(const std::vector<int>& vec_int) {
    std::vector<uint8_t> vec_u8;
    vec_u8.reserve(vec_int.size());
    for (int val : vec_int) {
        // Clamp values to the valid uint8_t range
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        vec_u8.push_back(static_cast<uint8_t>(val));
    }
    return vec_u8;
}

struct PlotPoint {
    int temp;
    int rpm;
    size_t original_index; // Keep track of original index if needed for other arrays

    // Sort by temperature
    bool operator<(const PlotPoint& other) const {
        return temp < other.temp;
    }
};

// Helper to create sorted PlotPoint vectors for plotting/editing
std::vector<PlotPoint> createPlotPoints(const std::vector<int>& temps, const std::vector<int>& curve_points_scaled) {
    std::vector<PlotPoint> points;
    size_t n = std::min(temps.size(), curve_points_scaled.size());
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        // Assuming curve_points_scaled is 0-100, convert to RPM for plot
        points.push_back({temps[i], curve_points_scaled[i] * 100, i});
    }
    std::sort(points.begin(), points.end()); // Sort by temperature for plotting
    return points;
}

// Helper to extract data back from PlotPoints
void extractDataFromPlotPoints(const std::vector<PlotPoint>& points, std::vector<int>& temps, std::vector<int>& curve_points_scaled) {
    // Create temporary vectors to hold the potentially reordered data
    std::vector<int> temp_temps(temps.size());
    std::vector<int> temp_curve_points(curve_points_scaled.size());

    for (const auto& p : points) {
        if (p.original_index < temp_temps.size()) {
            temp_temps[p.original_index] = p.temp;
            // Convert RPM back to 0-100 scale for storage
            temp_curve_points[p.original_index] = p.rpm / 100;
        }
    }
    // Assign back to original vectors
    temps = temp_temps;
    curve_points_scaled = temp_curve_points;
}

int main(int argc, char* argv[]) {
    
     // Setup SDL
    // [If using SDL_MAIN_USE_CALLBACKS: all code below until the main loop starts would likely be your SDL_AppInit() function]
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    // Create window with SDL_Renderer graphics context
    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_Renderer example", 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_SetWindowOpacity(window, 0.4f); // Set window opacity to 40%
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext(); // Initialize ImPlot context
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Our state
    bool show_demo_window = true;
    
    // Main loop
    bool done = false;

    // --- Fan Control Logic Initialization ---
    FanController fanController;
    FanConfigData currentConfig; // Holds the currently applied config
    FanStatusData currentStatus; // Holds the latest status read
    std::string statusMessage = "Initializing...";
    bool controllerInitialized = false;

    if (fanController.initialize()) {
        controllerInitialized = true;
        // Read initial status, which includes current config settings
        if (fanController.readStatus(currentStatus)) {
            statusMessage = "Controller Initialized. Status/Config loaded.";
            // Copy config parts from status to currentConfig
            currentConfig.fan1_curve = currentStatus.fan1_curve;
            currentConfig.fan2_curve = currentStatus.fan2_curve;
            currentConfig.acc_time = currentStatus.acc_time;
            currentConfig.dec_time = currentStatus.dec_time;
            currentConfig.cpu_lower_temp = currentStatus.cpu_lower_temp;
            currentConfig.cpu_upper_temp = currentStatus.cpu_upper_temp;
            currentConfig.gpu_lower_temp = currentStatus.gpu_lower_temp;
            currentConfig.gpu_upper_temp = currentStatus.gpu_upper_temp;
            currentConfig.vrm_lower_temp = currentStatus.vrm_lower_temp;
            currentConfig.vrm_upper_temp = currentStatus.vrm_upper_temp;
        } else {
            statusMessage = "Controller Initialized, but failed to read initial status/config: " + fanController.getLastError();
            // Use default config (from FanConfigData constructor) if read fails
            currentConfig = FanConfigData();
        }
    } else {
        statusMessage = "Failed to initialize Fan Controller: " + fanController.getLastError();
        // Use default config if controller init fails
        currentConfig = FanConfigData();
    }

    // Create editable copy and int versions for ImGui
    FanConfigData editableConfig = currentConfig;
    std::vector<int> fan1_curve_int = convertVecU8ToVecInt(editableConfig.fan1_curve);
    std::vector<int> fan2_curve_int = convertVecU8ToVecInt(editableConfig.fan2_curve);
    std::vector<int> cpu_upper_temp_int = convertVecU8ToVecInt(editableConfig.cpu_upper_temp);
    std::vector<int> gpu_upper_temp_int = convertVecU8ToVecInt(editableConfig.gpu_upper_temp);
    // Assuming acc_time/dec_time vectors have at least 2 elements (for fan1, fan2)
    int fan1_acc_time_int = (editableConfig.acc_time.size() > 0) ? editableConfig.acc_time[0] : 0;
    int fan1_dec_time_int = (editableConfig.dec_time.size() > 0) ? editableConfig.dec_time[0] : 0;
    int fan2_acc_time_int = (editableConfig.acc_time.size() > 1) ? editableConfig.acc_time[1] : 0;
    int fan2_dec_time_int = (editableConfig.dec_time.size() > 1) ? editableConfig.dec_time[1] : 0;
    // Correct initialization for lower temp int vectors
    std::vector<int> cpu_lower_temp_int = convertVecU8ToVecInt(editableConfig.cpu_lower_temp);
    std::vector<int> gpu_lower_temp_int = convertVecU8ToVecInt(editableConfig.gpu_lower_temp);
    // Data For Plots
    std::vector<PlotPoint> fan1_plot_points = createPlotPoints(cpu_upper_temp_int, fan1_curve_int);
    std::vector<PlotPoint> fan2_plot_points = createPlotPoints(gpu_upper_temp_int, fan2_curve_int);

    // Main loop state
    // bool done = false;
    auto lastUpdateTime = std::chrono::steady_clock::now();
    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.00f); // Adjusted clear color

    while (!done)

    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

         // --- Periodic Update ---\n       
        auto now = std::chrono::steady_clock::now();
         auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime).count();
         if (controllerInitialized && elapsed > 1000) { // Update status every second
             lastUpdateTime = now;
             if (!fanController.readStatus(currentStatus)) {
                  statusMessage = "Error reading status: " + fanController.getLastError();
             } else {
                 // Optionally clear status message on successful read
                 // statusMessage = "Status updated.";
             }
         }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // --- ImGui Fan Control UI ---\    

        ImGui::Text("%s", statusMessage.c_str());
        ImGui::Separator();

        if (controllerInitialized) {
            ImGui::Text("Current Status:");
            // Display available status data
            ImGui::Text("  Fan 1 Speed: %d RPM (%d%%)", currentStatus.fan1_speed, currentStatus.fan1_percent);
            ImGui::Text("  Fan 2 Speed: %d RPM (%d%%)", currentStatus.fan2_speed, currentStatus.fan2_percent);
            ImGui::Text("  FW Ver: %d, Chip: %02X%02X, Ver: %02X", currentStatus.fw_ver, currentStatus.chip_id1, currentStatus.chip_id2, currentStatus.chip_ver);
            ImGui::Separator();

            ImGui::Text("Configuration:");

            const float tempPadding = 5.0f;  // Degrees C padding
            const float rpmPadding1 = 100.0f; // RPM padding for Fan 1
            const float rpmPadding2 = 100.0f; // RPM padding for Fan 2

            // --- Fan 1 Curve Plot ---
            ImGui::Text("Fan 1 Curve (CPU Temp vs RPM)");
            if (ImPlot::BeginPlot("Fan 1 Curve", "Temperature (°C)", "RPM", ImVec2(-1, 300), ImPlotFlags_NoInputs)) {
                // Setup axes limits (Static with Padding)
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                
                ImPlot::SetupAxisLimits(ImAxis_X1, 0 - tempPadding, 127 + tempPadding); // Temp with padding
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0 - rpmPadding1, 5200 + rpmPadding1); // RPM with padding

                // Prepare data for plotting (needs arrays of doubles or floats)
                std::vector<double> temps_d(fan1_plot_points.size());
                std::vector<double> rpms_d(fan1_plot_points.size());
                for(size_t i = 0; i < fan1_plot_points.size(); ++i) {
                    temps_d[i] = static_cast<double>(fan1_plot_points[i].temp);
                    rpms_d[i] = static_cast<double>(fan1_plot_points[i].rpm);
                }

                // Plot the line connecting the points
                ImPlot::PlotLine("Curve", temps_d.data(), rpms_d.data(), fan1_plot_points.size());

                // Add draggable points
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
                for (size_t i = 0; i < fan1_plot_points.size(); ++i) {
                    // Use doubles for DragPoint interaction
                    double current_temp = static_cast<double>(fan1_plot_points[i].temp);
                    double current_rpm = static_cast<double>(fan1_plot_points[i].rpm);

                    if (ImPlot::DragPoint(i, &current_temp, &current_rpm, ImVec4(0,0.9f,0,1), 4.0f)) {
                        // Update the upper temp and RPM from dragging
                        fan1_plot_points[i].temp = std::max(0, std::min(127, static_cast<int>(current_temp + 0.5))); // Clamp Temp 0-127
                        fan1_plot_points[i].rpm = std::max(0, std::min((int)FanController::MAX_FAN1_RPM, static_cast<int>(current_rpm + 0.5))); // Clamp RPM 0-Max

                        // --- Automatically update the LOWER temp of the NEXT point ---
                        size_t original_idx = fan1_plot_points[i].original_index;
                        size_t next_lower_idx = original_idx + 1; // Index of the next point's lower bound

                        // Check if the next point exists within the bounds of the lower temp array
                        if (next_lower_idx < cpu_lower_temp_int.size()) {
                            // Set the next point's lower temp 3 degrees below the current point's upper temp
                            cpu_lower_temp_int[next_lower_idx] = std::max(0, fan1_plot_points[i].temp - 3);
                        }
                        // --- End lower temp update ---

                        // Ensure points remain sorted by temperature after dragging
                        std::sort(fan1_plot_points.begin(), fan1_plot_points.end());
                    }
                }
                ImPlot::EndPlot();
            }

            // --- Fan 2 Curve Plot ---
            ImGui::Text("Fan 2 Curve (GPU Temp vs RPM)");
             if (ImPlot::BeginPlot("Fan 2 Curve", "Temperature (°C)", "RPM", ImVec2(-1, 300), ImPlotFlags_NoInputs)) {
                // Setup axes limits (Static with Padding)
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                // const float tempPadding = 5.0f; // Already defined above, can reuse
                const float rpmPadding2 = 100.0f; // RPM padding
                ImPlot::SetupAxisLimits(ImAxis_X1, 0 - tempPadding, 127 + tempPadding); // Temp with padding
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0 - rpmPadding2, 5000 + rpmPadding2); // RPM with padding

                // Prepare data for plotting
                std::vector<double> temps_d(fan2_plot_points.size());
                std::vector<double> rpms_d(fan2_plot_points.size());
                 for(size_t i = 0; i < fan2_plot_points.size(); ++i) {
                    temps_d[i] = static_cast<double>(fan2_plot_points[i].temp);
                    rpms_d[i] = static_cast<double>(fan2_plot_points[i].rpm);
                }

                // Plot the line
                ImPlot::PlotLine("Curve", temps_d.data(), rpms_d.data(), fan2_plot_points.size());

                // Add draggable points
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
                for (size_t i = 0; i < fan2_plot_points.size(); ++i) {
                    double current_temp = static_cast<double>(fan2_plot_points[i].temp);
                    double current_rpm = static_cast<double>(fan2_plot_points[i].rpm);

                    if (ImPlot::DragPoint(i + fan1_plot_points.size(), &current_temp, &current_rpm, ImVec4(0,0.9f,0,1), 4.0f)) { // Ensure unique ID
                        // Update the upper temp and RPM from dragging
                        fan2_plot_points[i].temp = std::max(0, std::min(127, static_cast<int>(current_temp + 0.5)));
                        fan2_plot_points[i].rpm = std::max(0, std::min((int)FanController::MAX_FAN2_RPM, static_cast<int>(current_rpm + 0.5)));

                        // --- Automatically update the LOWER temp of the NEXT point ---
                        size_t original_idx = fan2_plot_points[i].original_index;
                        size_t next_lower_idx = original_idx + 1; // Index of the next point's lower bound

                        // Check if the next point exists within the bounds of the lower temp array
                        if (next_lower_idx < gpu_lower_temp_int.size()) {
                             // Set the next point's lower temp 3 degrees below the current point's upper temp
                            gpu_lower_temp_int[next_lower_idx] = std::max(0, fan2_plot_points[i].temp - 3);
                        }
                        // --- End lower temp update ---

                        std::sort(fan2_plot_points.begin(), fan2_plot_points.end());
                    }
                }
                ImPlot::EndPlot();
            }

            // REMOVED Slider code for fan1_curve_int and fan2_curve_int

            // ... (Keep sliders/inputs for acc_time, dec_time, lower_temps if needed) ...
            ImGui::Separator();
            ImGui::Text("Acceleration/Deceleration Time (per point, 0-255)");
            // Example for Fan 1 Acc/Dec - you might want sliders for the whole vector
            ImGui::SliderInt("Fan 1 Acc Time (P0)", &fan1_acc_time_int, 0, 255); // Example for point 0
            ImGui::SliderInt("Fan 1 Dec Time (P0)", &fan1_dec_time_int, 0, 255); // Example for point 0
            // Add similar controls for Fan 2 and potentially other points if needed

            ImGui::Separator();
            if (ImGui::Button("Apply Config")) {
                printf("--- Apply Config Button Pressed ---\n"); // Log button press

                // Extract data from plot points back into the original _int vectors
                extractDataFromPlotPoints(fan1_plot_points, cpu_upper_temp_int, fan1_curve_int);
                extractDataFromPlotPoints(fan2_plot_points, gpu_upper_temp_int, fan2_curve_int);

                // Convert back from int vectors/values to uint8_t vectors/values
                editableConfig.fan1_curve = convertVecIntToVecU8(fan1_curve_int);
                editableConfig.fan2_curve = convertVecIntToVecU8(fan2_curve_int);
                editableConfig.cpu_upper_temp = convertVecIntToVecU8(cpu_upper_temp_int);
                editableConfig.gpu_upper_temp = convertVecIntToVecU8(gpu_upper_temp_int);
                editableConfig.cpu_lower_temp = convertVecIntToVecU8(cpu_lower_temp_int);
                editableConfig.gpu_lower_temp = convertVecIntToVecU8(gpu_lower_temp_int);
                // ... convert other vectors ...

                 // Update acc_time/dec_time vectors
                if (editableConfig.acc_time.size() > 0) editableConfig.acc_time[0] = static_cast<uint8_t>(fan1_acc_time_int);
                if (editableConfig.dec_time.size() > 0) editableConfig.dec_time[0] = static_cast<uint8_t>(fan1_dec_time_int);
                // ... update fan2 acc/dec ...

                // --- Log Data Before Writing ---
                printf("Data to be written:\n");
                printf("  Fan1 Curve: "); for(uint8_t v : editableConfig.fan1_curve) printf("%d ", v); printf("\n");
                printf("  Fan2 Curve: "); for(uint8_t v : editableConfig.fan2_curve) printf("%d ", v); printf("\n");
                printf("  CPU Upper:  "); for(uint8_t v : editableConfig.cpu_upper_temp) printf("%d ", v); printf("\n");
                printf("  CPU Lower:  "); for(uint8_t v : editableConfig.cpu_lower_temp) printf("%d ", v); printf("\n");
                printf("  GPU Upper:  "); for(uint8_t v : editableConfig.gpu_upper_temp) printf("%d ", v); printf("\n");
                printf("  GPU Lower:  "); for(uint8_t v : editableConfig.gpu_lower_temp) printf("%d ", v); printf("\n");
                // Add logs for acc/dec/vrm if needed
                printf("Attempting fanController.writeConfig...\n");
                // --- End Log Data ---

                if (fanController.writeConfig(editableConfig)) {
                    printf("fanController.writeConfig returned TRUE.\n"); // Log success
                    statusMessage = "Config written. Verifying...";

                   // --- Verification Step ---
                   FanStatusData verifyStatus;
                   SDL_Delay(100); // Short delay
                   if (fanController.readStatus(verifyStatus)) {
                       bool verified = true;
                       std::string verificationError = "";

                       // Compare key vectors (direct comparison)
                       if (editableConfig.fan1_curve != verifyStatus.fan1_curve) {
                           verified = false; verificationError += " Fan1 Curve mismatch.";
                       }
                       if (editableConfig.fan2_curve != verifyStatus.fan2_curve) {
                           verified = false; verificationError += " Fan2 Curve mismatch.";
                       }
                       if (editableConfig.cpu_upper_temp != verifyStatus.cpu_upper_temp) {
                           verified = false; verificationError += " CPU Upper Temp mismatch.";
                       }
                       if (editableConfig.cpu_lower_temp != verifyStatus.cpu_lower_temp) {
                           verified = false; verificationError += " CPU Lower Temp mismatch.";
                       }
                       if (editableConfig.gpu_upper_temp != verifyStatus.gpu_upper_temp) {
                           verified = false; verificationError += " GPU Upper Temp mismatch.";
                       }
                       if (editableConfig.gpu_lower_temp != verifyStatus.gpu_lower_temp) {
                           verified = false; verificationError += " GPU Lower Temp mismatch.";
                       }
                       // ... Add comparisons for other vectors if needed ...

                       // --- Explicit Overlap Verification REMOVED ---
                       // The direct comparison of cpu_lower_temp and gpu_lower_temp
                       // implicitly verifies the overlap if the calculation during
                       // drag-point was correct and the write succeeded.
                       // --- End Overlap Verification REMOVED ---


                       if (verified) {
                           statusMessage = "Config written and verified successfully."; // Removed "(including overlap)"
                           currentConfig = editableConfig;
                           currentStatus = verifyStatus;
                           // Optionally update plot points again from verified data
                           // fan1_plot_points = createPlotPoints(convertVecU8ToVecInt(currentStatus.cpu_upper_temp), convertVecU8ToVecInt(currentStatus.fan1_curve));
                           // fan2_plot_points = createPlotPoints(convertVecU8ToVecInt(currentStatus.gpu_upper_temp), convertVecU8ToVecInt(currentStatus.fan2_curve));
                       } else {
                           statusMessage = "Config written, but VERIFICATION FAILED:" + verificationError;
                           printf("--- VERIFICATION FAILED ---\n");
                           printf("Verification Error Details: %s\n", verificationError.c_str()); // Print the specific error message

                           printf("Expected Fan1 Curve: "); for(uint8_t v : editableConfig.fan1_curve) printf("%d ", v); printf("\n");
                           printf("Actual Fan1 Curve:   "); for(uint8_t v : verifyStatus.fan1_curve) printf("%d ", v); printf("\n");

                           printf("Expected Fan2 Curve: "); for(uint8_t v : editableConfig.fan2_curve) printf("%d ", v); printf("\n");
                           printf("Actual Fan2 Curve:   "); for(uint8_t v : verifyStatus.fan2_curve) printf("%d ", v); printf("\n");

                           printf("Expected CPU Upper: "); for(uint8_t v : editableConfig.cpu_upper_temp) printf("%d ", v); printf("\n");
                           printf("Actual CPU Upper:   "); for(uint8_t v : verifyStatus.cpu_upper_temp) printf("%d ", v); printf("\n");

                           printf("Expected CPU Lower: "); for(uint8_t v : editableConfig.cpu_lower_temp) printf("%d ", v); printf("\n");
                           printf("Actual CPU Lower:   "); for(uint8_t v : verifyStatus.cpu_lower_temp) printf("%d ", v); printf("\n");

                           printf("Expected GPU Upper: "); for(uint8_t v : editableConfig.gpu_upper_temp) printf("%d ", v); printf("\n");
                           printf("Actual GPU Upper:   "); for(uint8_t v : verifyStatus.gpu_upper_temp) printf("%d ", v); printf("\n");

                           printf("Expected GPU Lower: "); for(uint8_t v : editableConfig.gpu_lower_temp) printf("%d ", v); printf("\n");
                           printf("Actual GPU Lower:   "); for(uint8_t v : verifyStatus.gpu_lower_temp) printf("%d ", v); printf("\n");
                       }
                       printf("---------------------------\n"); // Log end of verification
                   } else {
                    statusMessage = "Config written, but failed to read back for verification: " + fanController.getLastError();
                  }
                } else {
                    // Log failure and the error message
                    printf("fanController.writeConfig returned FALSE.\n");
                    statusMessage = "Error writing config: " + fanController.getLastError();
                    printf("Error details: %s\n", fanController.getLastError().c_str());
                }
                 printf("--- Apply Config Action Finished ---\n"); // Log end of action
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload Config")) {
                 // Re-read status which contains the current config
                if (fanController.readStatus(currentStatus)) {
                    statusMessage = "Config reloaded from EC.";

                        // --- DEBUG: Print raw data from readStatus ---
                        printf("--- Reloading ---\n");
                        printf("Raw Fan 1 Curve (readStatus): ");
                        for(uint8_t val : currentStatus.fan1_curve) { printf("%d ", val); } printf("\n");
                        printf("Raw CPU Upper Temp (readStatus): ");
                        for(uint8_t val : currentStatus.cpu_upper_temp) { printf("%d ", val); } printf("\n");
                        printf("Raw Fan 2 Curve (readStatus): ");
                        for(uint8_t val : currentStatus.fan2_curve) { printf("%d ", val); } printf("\n");
                        printf("Raw GPU Upper Temp (readStatus): ");
                        for(uint8_t val : currentStatus.gpu_upper_temp) { printf("%d ", val); } printf("\n");
                        // --- END DEBUG ---


                        // Copy config parts from status to editableConfig
                        editableConfig.fan1_curve = currentStatus.fan1_curve;
                        editableConfig.fan2_curve = currentStatus.fan2_curve;
                        editableConfig.acc_time = currentStatus.acc_time;
                        editableConfig.dec_time = currentStatus.dec_time;
                        editableConfig.cpu_lower_temp = currentStatus.cpu_lower_temp;
                        editableConfig.cpu_upper_temp = currentStatus.cpu_upper_temp;
                        editableConfig.gpu_lower_temp = currentStatus.gpu_lower_temp;
                        editableConfig.gpu_upper_temp = currentStatus.gpu_upper_temp;
                        editableConfig.vrm_lower_temp = currentStatus.vrm_lower_temp;
                        editableConfig.vrm_upper_temp = currentStatus.vrm_upper_temp;

                        // Update int versions for ImGui controls (sliders, plots, etc.)
                        fan1_curve_int = convertVecU8ToVecInt(editableConfig.fan1_curve);
                        fan2_curve_int = convertVecU8ToVecInt(editableConfig.fan2_curve);
                        cpu_upper_temp_int = convertVecU8ToVecInt(editableConfig.cpu_upper_temp);
                        gpu_upper_temp_int = convertVecU8ToVecInt(editableConfig.gpu_upper_temp);
                        cpu_lower_temp_int = convertVecU8ToVecInt(editableConfig.cpu_lower_temp);
                        gpu_lower_temp_int = convertVecU8ToVecInt(editableConfig.gpu_lower_temp);
                        // ... update other int vectors/values ...
                        fan1_acc_time_int = (editableConfig.acc_time.size() > 0) ? editableConfig.acc_time[0] : 0;
                        fan1_dec_time_int = (editableConfig.dec_time.size() > 0) ? editableConfig.dec_time[0] : 0;
                        // ... update fan2 acc/dec ...

                        // Update plot points from the reloaded config
                        fan1_plot_points = createPlotPoints(cpu_upper_temp_int, fan1_curve_int);
                        fan2_plot_points = createPlotPoints(gpu_upper_temp_int, fan2_curve_int);

                        // --- DEBUG: Print generated plot points ---
                        printf("Generated Fan 1 Plot Points (Temp, RPM): ");
                        for(const auto& p : fan1_plot_points) { printf("(%d, %d) ", p.temp, p.rpm); } printf("\n");
                        printf("Generated Fan 2 Plot Points (Temp, RPM): ");
                        for(const auto& p : fan2_plot_points) { printf("(%d, %d) ", p.temp, p.rpm); } printf("\n");
                        printf("-----------------\n");
                        // --- END DEBUG ---

                } else {
                    statusMessage = "Error reloading config from EC: " + fanController.getLastError();
                }
            }

        } else {
            ImGui::Text("Fan controller not initialized. Check status message.");
        }

        // Rendering
        ImGui::Render();
        //SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }


    // Cleanup
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext(); // Destroy ImPlot context
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}