#include "fan_control.h" // Include the new header
#include <windows.h>
#include <iostream> // Keep for potential debug/error output during init/deinit
#include <vector>
#include <string>
// #include <fstream> // No longer needed here, handled by GUI or main app
#include <iomanip> // Keep for potential debug formatting if needed later
// #include <thread> // No longer needed here
// #include <chrono> // No longer needed here
#include <cstdint>
// #include <cstdlib> // No longer needed here (system("cls"))
// #include <filesystem> // No longer needed here, handled by GUI or main app
#include <stdexcept> // For throwing errors

// #include "json.hpp" // No longer needed here, handled by GUI or main app

// using json = nlohmann::json; // No longer needed here

// --- Constants based on Python script ---
// Keep these internal to the implementation file
namespace { // Use an anonymous namespace for internal linkage
    const uint16_t EC_ADDR_PORT = 0x4E;
    const uint16_t EC_DATA_PORT = 0x4F;

    namespace ITE_REGISTER_MAP {
        const uint16_t ECINDAR0 = 0x103B;
        const uint16_t ECINDAR1 = 0x103C;
        const uint16_t ECINDAR2 = 0x103D;
        const uint16_t ECINDAR3 = 0x103E;
        const uint16_t ECINDDR = 0x103F;
        const uint16_t GPDRA = 0x1601;
        const uint16_t GPCRA0 = 0x1610;
        const uint16_t GPCRA1 = 0x1611;
        const uint16_t GPCRA2 = 0x1612;
        const uint16_t GPCRA3 = 0x1613;
        const uint16_t GPCRA4 = 0x1614;
        const uint16_t GPCRA5 = 0x1615;
        const uint16_t GPCRA6 = 0x1616;
        const uint16_t GPCRA7 = 0x1617;
        const uint16_t GPOTA = 0x1671;
        const uint16_t GPDMRA = 0x1661;
        const uint16_t DCR0 = 0x1802;
        const uint16_t DCR1 = 0x1803;
        const uint16_t DCR2 = 0x1804;
        const uint16_t DCR3 = 0x1805;
        const uint16_t DCR4 = 0x1806; // FAN2 Target Duty Cycle?
        const uint16_t DCR5 = 0x1807; // FAN1 Target Duty Cycle?
        const uint16_t DCR6 = 0x1808;
        const uint16_t DCR7 = 0x1809;
        const uint16_t CTR2 = 0x1842;
        const uint16_t ECHIPID1 = 0x2000;
        const uint16_t ECHIPID2 = 0x2001;
        const uint16_t ECHIPVER = 0x2002;
        const uint16_t ECDEBUG = 0x2003;
        const uint16_t EADDR = 0x2100;
        const uint16_t EDAT = 0x2101;
        const uint16_t ECNT = 0x2102;
        const uint16_t ESTS = 0x2103;
        const uint16_t FW_VER = 0xC2C7;
        const uint16_t FAN_CUR_POINT = 0xC534;
        const uint16_t FAN_POINT = 0xC535; // Not used in C# code?
        const uint16_t FAN1_BASE = 0xC540;
        const uint16_t FAN2_BASE = 0xC550;
        const uint16_t FAN_ACC_BASE = 0xC560;
        const uint16_t FAN_DEC_BASE = 0xC570;
        const uint16_t CPU_TEMP = 0xC580;
        const uint16_t CPU_TEMP_HYST = 0xC590;
        const uint16_t GPU_TEMP = 0xC5A0;
        const uint16_t GPU_TEMP_HYST = 0xC5B0;
        const uint16_t VRM_TEMP = 0xC5C0; // IC Temp in C# code
        const uint16_t VRM_TEMP_HYST = 0xC5D0; // IC Temp Hyst in C# code
        const uint16_t FAN1_TARGET_DUTY = 0xC5FC - 0x18; // 0xC5E4
        const uint16_t FAN2_TARGET_DUTY = 0xC5FD - 0x18; // 0xC5E5
        const uint16_t FAN1_TARGET_CURVE_VAL = 0xC5FC;
        const uint16_t FAN2_TARGET_CURVE_VAL = 0xC5FD;
        const uint16_t CPU_TEMP_EN = 0xC631;
        const uint16_t GPU_TEMP_EN = 0xC632;
        const uint16_t VRM_TEMP_EN = 0xC633;
        const uint16_t FAN1_ACC_TIMER = 0xC3DA;
        const uint16_t FAN2_ACC_TIMER = 0xC3DB;
        const uint16_t FAN1_CUR_ACC = 0xC3DC;
        const uint16_t FAN1_CUR_DEC = 0xC3DD;
        const uint16_t FAN2_CUR_ACC = 0xC3DE;
        const uint16_t FAN2_CUR_DEC = 0xC3DF;
        const uint16_t FAN1_RPM_LSB = 0xC5E0;
        const uint16_t FAN1_RPM_MSB = 0xC5E1;
        const uint16_t FAN2_RPM_LSB = 0xC5E2;
        const uint16_t FAN2_RPM_MSB = 0xC5E3;
    }
} // end anonymous namespace

// --- FanController Implementation ---

FanController::FanController() : hWinRing0Wrapper(nullptr), winring_init_ok(false) {
    // Constructor: Initialize pointers to null
    pLoadWinRing0 = nullptr;
    pInitWinRing0 = nullptr;
    pReadPort = nullptr;
    pWritePort = nullptr;
    pGetStatus = nullptr;
    pDeinitWinRing0 = nullptr;
}

FanController::~FanController() {
    // Destructor: Ensure deinitialization is called
    deinitialize();
}

void FanController::setError(const std::string& errorMsg) {
    lastError = errorMsg;
    // Optionally log to a file or debug output here instead of cerr
    // std::cerr << "Error: " << errorMsg << std::endl;
}

bool FanController::initialize() {
    if (winring_init_ok) {
        return true; // Already initialized
    }
    setError(""); // Clear previous errors

    // Use LoadLibraryA for ANSI compatibility if needed, or LoadLibraryW for Unicode
    hWinRing0Wrapper = LoadLibraryA("winring_wrapper.dll");
    if (!hWinRing0Wrapper) {
        DWORD errorCode = GetLastError();
        setError("Could not load winring_wrapper.dll. Error code: " + std::to_string(errorCode) + ". Ensure DLL and dependencies (WinRing0x64.dll, MinGW runtimes) are present.");
        return false;
    }

    // Get function pointers
    pLoadWinRing0 = (LoadWinRing0_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "LoadWinRing0");
    pInitWinRing0 = (InitWinRing0_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "InitWinRing0");
    pReadPort = (ReadPort_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "ReadPort");
    pWritePort = (WritePort_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "WritePort");
    pGetStatus = (GetStatus_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "GetStatus");
    pDeinitWinRing0 = (DeinitWinRing0_t)GetProcAddress((HMODULE)hWinRing0Wrapper, "DeinitWinRing0");

    if (!pLoadWinRing0 || !pInitWinRing0 || !pReadPort || !pWritePort || !pGetStatus || !pDeinitWinRing0) {
        setError("Could not get one or more function addresses from wrapper DLL.");
        FreeLibrary((HMODULE)hWinRing0Wrapper);
        hWinRing0Wrapper = nullptr;
        return false;
    }

    if (!pLoadWinRing0()) {
         setError("LoadWinRing0() via wrapper failed.");
         FreeLibrary((HMODULE)hWinRing0Wrapper);
         hWinRing0Wrapper = nullptr;
         return false;
    }

    if (!pInitWinRing0()) {
        uint32_t status = pGetStatus ? pGetStatus() : 0;
        setError("InitWinRing0() via wrapper failed. Status: " + std::to_string(status));
        // Don't call deinit here as init failed, just free library
        FreeLibrary((HMODULE)hWinRing0Wrapper);
        hWinRing0Wrapper = nullptr;
        return false;
    }

    winring_init_ok = true;
    return true;
}

void FanController::deinitialize() {
    if (hWinRing0Wrapper && winring_init_ok && pDeinitWinRing0) {
        pDeinitWinRing0();
    }

    if (hWinRing0Wrapper) {
        FreeLibrary((HMODULE)hWinRing0Wrapper);
        hWinRing0Wrapper = nullptr;
    }
    winring_init_ok = false;
    // Reset pointers
    pLoadWinRing0 = nullptr;
    pInitWinRing0 = nullptr;
    pReadPort = nullptr;
    pWritePort = nullptr;
    pGetStatus = nullptr;
    pDeinitWinRing0 = nullptr;
}

bool FanController::isInitialized() const {
    return winring_init_ok;
}

std::string FanController::getLastError() const {
    return lastError;
}


// --- EC Access Functions (Private) ---
uint8_t FanController::read_io_port_byte(uint16_t port) {
    if (!winring_init_ok || !pReadPort) {
        // setError("Attempted to read IO port while not initialized."); // Avoid flooding errors
        return 0;
    }
    return pReadPort(port);
}

void FanController::write_io_port_byte(uint16_t port, uint8_t value) {
    if (!winring_init_ok || !pWritePort) {
        // setError("Attempted to write IO port while not initialized."); // Avoid flooding errors
        return;
    }
    pWritePort(port, value);
}

uint8_t FanController::direct_ec_read(uint16_t addr) {
    // Ensure thread safety if called from multiple threads (using a mutex might be needed)
    // For now, assuming single-threaded access from the GUI event loop.
    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x11);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    write_io_port_byte(EC_DATA_PORT, (addr >> 8) & 0xFF);

    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x10);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    write_io_port_byte(EC_DATA_PORT, addr & 0xFF);

    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x12);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    return read_io_port_byte(EC_DATA_PORT);
}

void FanController::direct_ec_write(uint16_t addr, uint8_t data) {
    // Ensure thread safety if called from multiple threads
    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x11);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    write_io_port_byte(EC_DATA_PORT, (addr >> 8) & 0xFF);

    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x10);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    write_io_port_byte(EC_DATA_PORT, addr & 0xFF);

    write_io_port_byte(EC_ADDR_PORT, 0x2E);
    write_io_port_byte(EC_DATA_PORT, 0x12);
    write_io_port_byte(EC_ADDR_PORT, 0x2F);
    write_io_port_byte(EC_DATA_PORT, data);
}

std::vector<uint8_t> FanController::direct_ec_read_array(uint16_t addr_base, size_t size) {
    std::vector<uint8_t> buffer(size);
    for (size_t i = 0; i < size; ++i) {
        // Add error handling? What if read fails mid-array?
        buffer[i] = direct_ec_read(addr_base + static_cast<uint16_t>(i));
    }
    return buffer;
}

void FanController::direct_ec_write_array(uint16_t addr_base, const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    for (size_t i = 0; i < data.size(); ++i) {
         // Add error handling?
        direct_ec_write(addr_base + static_cast<uint16_t>(i), data[i]);
    }
}

// --- Status Reading (Public) ---
bool FanController::readStatus(FanStatusData& statusData) {
    if (!winring_init_ok) {
        setError("WinRing0 not initialized, cannot read status.");
        return false;
    }
    setError(""); // Clear previous errors

    try {
        // Fan Speeds
        uint8_t fan1_low = direct_ec_read(ITE_REGISTER_MAP::FAN1_RPM_LSB);
        uint8_t fan1_high = direct_ec_read(ITE_REGISTER_MAP::FAN1_RPM_MSB);
        statusData.fan1_speed = (static_cast<uint16_t>(fan1_high) << 8) | fan1_low;

        uint8_t fan2_low = direct_ec_read(ITE_REGISTER_MAP::FAN2_RPM_LSB);
        uint8_t fan2_high = direct_ec_read(ITE_REGISTER_MAP::FAN2_RPM_MSB);
        statusData.fan2_speed = (static_cast<uint16_t>(fan2_high) << 8) | fan2_low;

        // Calculate percentages using the class static constants
        statusData.fan1_percent = (FanController::MAX_FAN1_RPM > 0) ?
                           static_cast<int>((static_cast<double>(statusData.fan1_speed) / FanController::MAX_FAN1_RPM) * 100.0) :
                           0;
        statusData.fan2_percent = (FanController::MAX_FAN2_RPM > 0) ?
                           static_cast<int>((static_cast<double>(statusData.fan2_speed) / FanController::MAX_FAN2_RPM) * 100.0) :
                           0;

        // Read curves and temps
        statusData.fan1_curve = direct_ec_read_array(ITE_REGISTER_MAP::FAN1_BASE, 10);
        statusData.fan2_curve = direct_ec_read_array(ITE_REGISTER_MAP::FAN2_BASE, 10);
        statusData.acc_time = direct_ec_read_array(ITE_REGISTER_MAP::FAN_ACC_BASE, 10);
        statusData.dec_time = direct_ec_read_array(ITE_REGISTER_MAP::FAN_DEC_BASE, 10);
        statusData.cpu_upper_temp = direct_ec_read_array(ITE_REGISTER_MAP::CPU_TEMP, 10);
        statusData.cpu_lower_temp = direct_ec_read_array(ITE_REGISTER_MAP::CPU_TEMP_HYST, 10);
        statusData.gpu_upper_temp = direct_ec_read_array(ITE_REGISTER_MAP::GPU_TEMP, 10);
        statusData.gpu_lower_temp = direct_ec_read_array(ITE_REGISTER_MAP::GPU_TEMP_HYST, 10);
        statusData.vrm_upper_temp = direct_ec_read_array(ITE_REGISTER_MAP::VRM_TEMP, 10); // Renamed from IC
        statusData.vrm_lower_temp = direct_ec_read_array(ITE_REGISTER_MAP::VRM_TEMP_HYST, 10); // Renamed from IC

        // EC Info
        statusData.chip_id1 = direct_ec_read(ITE_REGISTER_MAP::ECHIPID1);
        statusData.chip_id2 = direct_ec_read(ITE_REGISTER_MAP::ECHIPID2);
        statusData.chip_ver = direct_ec_read(ITE_REGISTER_MAP::ECHIPVER);
        // Assuming FW_VER is a single byte read based on original code, but declared as uint16_t.
        // If it's truly 16-bit, it needs two reads. Let's assume single byte for now.
        // If issues arise, check EC documentation for FW_VER address structure.
        statusData.fw_ver = direct_ec_read(ITE_REGISTER_MAP::FW_VER); // Read as single byte

        // Read other relevant single values
        statusData.fan1_target_duty = direct_ec_read(ITE_REGISTER_MAP::FAN1_TARGET_DUTY);
        statusData.fan2_target_duty = direct_ec_read(ITE_REGISTER_MAP::FAN2_TARGET_DUTY);
        statusData.fan1_target_curve_val = direct_ec_read(ITE_REGISTER_MAP::FAN1_TARGET_CURVE_VAL);
        statusData.fan2_target_curve_val = direct_ec_read(ITE_REGISTER_MAP::FAN2_TARGET_CURVE_VAL);
        statusData.fan_cur_point = direct_ec_read(ITE_REGISTER_MAP::FAN_CUR_POINT);


        return true;

    } catch (const std::exception& e) {
        setError(std::string("Error reading EC status: ") + e.what());
        return false;
    } catch (...) {
         setError("Unknown error reading EC status.");
         return false;
    }
}


// --- Write Configuration (Public) ---
bool FanController::writeConfig(const FanConfigData& configData) {
    if (!winring_init_ok) {
        setError("WinRing0 not initialized, cannot write config.");
        return false;
    }
     setError(""); // Clear previous errors

    // Basic validation (ensure vectors have correct size)
    if (configData.fan1_curve.size() != 10 || configData.fan2_curve.size() != 10 ||
        configData.acc_time.size() != 10 || configData.dec_time.size() != 10 ||
        configData.cpu_lower_temp.size() != 10 || configData.cpu_upper_temp.size() != 10 ||
        configData.gpu_lower_temp.size() != 10 || configData.gpu_upper_temp.size() != 10 ||
        configData.vrm_lower_temp.size() != 10 || configData.vrm_upper_temp.size() != 10)
    {
        setError("Invalid configuration data: All arrays must have size 10.");
        return false;
    }


    try {
        // Write arrays to EC
        direct_ec_write_array(ITE_REGISTER_MAP::FAN1_BASE, configData.fan1_curve);
        direct_ec_write_array(ITE_REGISTER_MAP::FAN2_BASE, configData.fan2_curve);
        direct_ec_write_array(ITE_REGISTER_MAP::CPU_TEMP, configData.cpu_upper_temp);
        direct_ec_write_array(ITE_REGISTER_MAP::GPU_TEMP, configData.gpu_upper_temp);
        direct_ec_write_array(ITE_REGISTER_MAP::VRM_TEMP, configData.vrm_upper_temp); // Renamed from IC
        direct_ec_write_array(ITE_REGISTER_MAP::CPU_TEMP_HYST, configData.cpu_lower_temp);
        direct_ec_write_array(ITE_REGISTER_MAP::GPU_TEMP_HYST, configData.gpu_lower_temp);
        direct_ec_write_array(ITE_REGISTER_MAP::VRM_TEMP_HYST, configData.vrm_lower_temp); // Renamed from IC
        direct_ec_write_array(ITE_REGISTER_MAP::FAN_ACC_BASE, configData.acc_time);
        direct_ec_write_array(ITE_REGISTER_MAP::FAN_DEC_BASE, configData.dec_time);

        // Write target values (replicating original logic - this might need adjustment based on how the EC uses these)
        // It might be better to let the EC handle these based on the curves written,
        // but we replicate the original script's behavior for now.
        uint8_t fan1_curve_target = direct_ec_read(ITE_REGISTER_MAP::FAN1_TARGET_CURVE_VAL); // Read current target
        direct_ec_write(ITE_REGISTER_MAP::FAN1_TARGET_DUTY, fan1_curve_target);
        // The DCR calculation seems specific and might be EC internal logic. Replicating it might be correct or might interfere.
        // uint8_t fan1_dcr_val = (fan1_curve_target <= 45) ? static_cast<uint8_t>(fan1_curve_target * 255 / 45) : 255;
        // direct_ec_write(ITE_REGISTER_MAP::DCR5, fan1_dcr_val); // Commenting out DCR write unless confirmed necessary

        uint8_t fan2_curve_target = direct_ec_read(ITE_REGISTER_MAP::FAN2_TARGET_CURVE_VAL); // Read current target
        direct_ec_write(ITE_REGISTER_MAP::FAN2_TARGET_DUTY, fan2_curve_target);
        // uint8_t fan2_dcr_val = (fan2_curve_target <= 45) ? static_cast<uint8_t>(fan2_curve_target * 255 / 45) : 255;
        // direct_ec_write(ITE_REGISTER_MAP::DCR4, fan2_dcr_val); // Commenting out DCR write unless confirmed necessary

        // Update current ACC/DEC based on the *current* curve point index read from EC
        uint8_t acc_dec_time_target_idx = direct_ec_read(ITE_REGISTER_MAP::FAN_CUR_POINT);
        if (acc_dec_time_target_idx < configData.acc_time.size()) {
             direct_ec_write(ITE_REGISTER_MAP::FAN1_CUR_ACC, configData.acc_time[acc_dec_time_target_idx]);
             direct_ec_write(ITE_REGISTER_MAP::FAN2_CUR_ACC, configData.acc_time[acc_dec_time_target_idx]);
        } else {
             // Log warning?
             setError("Warning: Invalid ACC_time target index read from EC: " + std::to_string(static_cast<int>(acc_dec_time_target_idx)));
        }

        if (acc_dec_time_target_idx < configData.dec_time.size()) {
            direct_ec_write(ITE_REGISTER_MAP::FAN1_CUR_DEC, configData.dec_time[acc_dec_time_target_idx]);
            direct_ec_write(ITE_REGISTER_MAP::FAN2_CUR_DEC, configData.dec_time[acc_dec_time_target_idx]);
        } else {
             // Log warning?
             setError("Warning: Invalid DEC_time target index read from EC: " + std::to_string(static_cast<int>(acc_dec_time_target_idx)));
        }

        return true;

    } catch (const std::exception& e) {
        setError(std::string("An error occurred during writeConfig: ") + e.what());
        return false;
    } catch (...) {
         setError("An unknown error occurred during writeConfig.");
         return false;
    }
}

