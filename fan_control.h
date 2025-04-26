#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <vector>
#include <cstdint>
#include <string>

// Structure to hold the status data read from the EC
struct FanStatusData {
    uint16_t fan1_speed = 0;
    uint16_t fan2_speed = 0;
    int fan1_percent = 0;
    int fan2_percent = 0;
    std::vector<uint8_t> fan1_curve;
    std::vector<uint8_t> fan2_curve;
    std::vector<uint8_t> acc_time;
    std::vector<uint8_t> dec_time;
    std::vector<uint8_t> cpu_lower_temp;
    std::vector<uint8_t> cpu_upper_temp;
    std::vector<uint8_t> gpu_lower_temp;
    std::vector<uint8_t> gpu_upper_temp;
    std::vector<uint8_t> vrm_lower_temp; // Renamed from IC
    std::vector<uint8_t> vrm_upper_temp; // Renamed from IC
    uint8_t chip_id1 = 0;
    uint8_t chip_id2 = 0;
    uint8_t chip_ver = 0;
    uint16_t fw_ver = 0; // Changed to uint16_t based on usage
    uint8_t fan1_target_duty = 0;
    uint8_t fan2_target_duty = 0;
    uint8_t fan1_target_curve_val = 0;
    uint8_t fan2_target_curve_val = 0;
    uint8_t fan_cur_point = 0;

    // Add default sizes for vectors to avoid issues if read fails partially
    FanStatusData() :
        fan1_curve(10, 0), fan2_curve(10, 0), acc_time(10, 0), dec_time(10, 0),
        cpu_lower_temp(10, 0), cpu_upper_temp(10, 0), gpu_lower_temp(10, 0), gpu_upper_temp(10, 0),
        vrm_lower_temp(10, 0), vrm_upper_temp(10, 0) {}
};

// Structure to hold the configuration data to write to the EC
struct FanConfigData {
    std::vector<uint8_t> fan1_curve;
    std::vector<uint8_t> fan2_curve;
    std::vector<uint8_t> acc_time;
    std::vector<uint8_t> dec_time;
    std::vector<uint8_t> cpu_lower_temp;
    std::vector<uint8_t> cpu_upper_temp;
    std::vector<uint8_t> gpu_lower_temp;
    std::vector<uint8_t> gpu_upper_temp;
    std::vector<uint8_t> vrm_lower_temp; // Renamed from IC
    std::vector<uint8_t> vrm_upper_temp; // Renamed from IC

     // Constructor to ensure vectors have the correct size
    FanConfigData() :
        fan1_curve(10, 0), fan2_curve(10, 0), acc_time(10, 0), dec_time(10, 0),
        cpu_lower_temp(10, 0), cpu_upper_temp(10, 0), gpu_lower_temp(10, 0), gpu_upper_temp(10, 0),
        vrm_lower_temp(10, 0), vrm_upper_temp(10, 0) {}
};


class FanController {
public:
    // Expose Max RPM values as public static constants
    static const uint16_t MAX_FAN1_RPM = 5200;
    static const uint16_t MAX_FAN2_RPM = 5000;

    FanController();
    ~FanController();

    // Initializes the WinRing0 library
    bool initialize();

    // Deinitializes the WinRing0 library
    void deinitialize();

    // Reads the current status from the EC
    bool readStatus(FanStatusData& statusData);

    // Writes the given configuration to the EC
    bool writeConfig(const FanConfigData& configData);

    // Returns true if WinRing0 was initialized successfully
    bool isInitialized() const;

    // Gets the last error message
    std::string getLastError() const;

private:
    // WinRing0 function pointers and handle
    void* hWinRing0Wrapper = nullptr; // Use void* for HMODULE
    typedef bool (*LoadWinRing0_t)();
    typedef bool (*InitWinRing0_t)();
    typedef uint8_t (*ReadPort_t)(uint16_t port);
    typedef void (*WritePort_t)(uint16_t port, uint8_t value);
    typedef uint32_t (*GetStatus_t)();
    typedef void (*DeinitWinRing0_t)();

    LoadWinRing0_t pLoadWinRing0 = nullptr;
    InitWinRing0_t pInitWinRing0 = nullptr;
    ReadPort_t pReadPort = nullptr;
    WritePort_t pWritePort = nullptr;
    GetStatus_t pGetStatus = nullptr;
    DeinitWinRing0_t pDeinitWinRing0 = nullptr;

    bool winring_init_ok = false;
    std::string lastError;

    // Low-level EC access functions
    uint8_t read_io_port_byte(uint16_t port);
    void write_io_port_byte(uint16_t port, uint8_t value);
    uint8_t direct_ec_read(uint16_t addr);
    void direct_ec_write(uint16_t addr, uint8_t data);
    std::vector<uint8_t> direct_ec_read_array(uint16_t addr_base, size_t size);
    void direct_ec_write_array(uint16_t addr_base, const std::vector<uint8_t>& data);

    // Helper to set last error
    void setError(const std::string& errorMsg);
};

#endif // FAN_CONTROL_H
