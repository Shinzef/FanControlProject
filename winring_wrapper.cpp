#include <windows.h>
#include <iostream> // Optional: for debug output during development
#include <string>   // For std::string
#include <vector>   // For path manipulation buffer
#include <libloaderapi.h> // For GetModuleFileName

// Define function pointer types
typedef BOOL (WINAPI *InitializeOls_t)();
typedef VOID (WINAPI *DeinitializeOls_t)();
typedef DWORD (WINAPI *GetDllStatus_t)();
typedef BYTE (WINAPI *ReadIoPortByte_t)(WORD port);
typedef VOID (WINAPI *WriteIoPortByte_t)(WORD port, BYTE value);

// Global variables
HMODULE hWinRing0 = NULL;
InitializeOls_t InitializeOlsFunc = NULL;
DeinitializeOls_t DeinitializeOlsFunc = NULL;
GetDllStatus_t GetDllStatusFunc = NULL;
ReadIoPortByte_t ReadIoPortByteFunc = NULL;
WriteIoPortByte_t WriteIoPortByteFunc = NULL;
BOOL winRingLoaded = FALSE;
BOOL winRingInitialized = FALSE;
DWORD lastLoadError = 0; // Variable to store LoadLibrary error

// Helper function to get the directory of the current module (the wrapper DLL)
std::wstring GetModuleDirectory(HMODULE hModule) {
    std::vector<wchar_t> pathBuf;
    DWORD copied = 0;
    do {
        pathBuf.resize(pathBuf.size() + MAX_PATH);
        copied = GetModuleFileNameW(hModule, pathBuf.data(), static_cast<DWORD>(pathBuf.size()));
    } while (copied >= pathBuf.size());

    pathBuf.resize(copied);

    std::wstring path(pathBuf.begin(), pathBuf.end());

    // Find the last backslash to get the directory
    size_t lastBackslash = path.find_last_of(L"\\/");
    if (std::wstring::npos != lastBackslash) {
        return path.substr(0, lastBackslash);
    }
    return L"."; // Return current directory if path is unusual
}


// Exported function to load the WinRing0 DLL and get function pointers
extern "C" __declspec(dllexport) BOOL LoadWinRing0() {
    if (winRingLoaded) return TRUE; // Already loaded

    lastLoadError = 0; // Reset error code

    // Get the directory where this wrapper DLL is located
    HMODULE hSelf = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&LoadWinRing0, &hSelf);

    std::wstring wrapperDir = GetModuleDirectory(hSelf);
    std::wstring winRingPath = wrapperDir + L"\\WinRing0x64.dll";

    // std::wcout << L"Attempting to load WinRing0 from: " << winRingPath << std::endl; // Debug output

    // Load WinRing0 using the constructed absolute path
    hWinRing0 = LoadLibraryW(winRingPath.c_str()); // Use LoadLibraryW for wide string path
    if (hWinRing0 == NULL) {
        lastLoadError = GetLastError(); // Store the error code
        // Optional: Add logging or error reporting here if needed
        // std::wcerr << L"LoadLibrary failed for path " << winRingPath << L" with error: " << lastLoadError << std::endl;
        return FALSE;
    }

    InitializeOlsFunc = (InitializeOls_t)GetProcAddress(hWinRing0, "InitializeOls");
    DeinitializeOlsFunc = (DeinitializeOls_t)GetProcAddress(hWinRing0, "DeinitializeOls");
    GetDllStatusFunc = (GetDllStatus_t)GetProcAddress(hWinRing0, "GetDllStatus");
    ReadIoPortByteFunc = (ReadIoPortByte_t)GetProcAddress(hWinRing0, "ReadIoPortByte");
    WriteIoPortByteFunc = (WriteIoPortByte_t)GetProcAddress(hWinRing0, "WriteIoPortByte");

    if (!InitializeOlsFunc || !DeinitializeOlsFunc || !GetDllStatusFunc || !ReadIoPortByteFunc || !WriteIoPortByteFunc) {
        lastLoadError = GetLastError(); // Store GetProcAddress error (less likely)
        FreeLibrary(hWinRing0);
        hWinRing0 = NULL;
        return FALSE;
    }

    winRingLoaded = TRUE;
    return TRUE;
}

// Exported function to initialize the WinRing0 driver
extern "C" __declspec(dllexport) BOOL InitWinRing0() {
    if (!winRingLoaded) return FALSE; // Must load first
    if (winRingInitialized) return TRUE; // Already initialized

    if (InitializeOlsFunc()) {
        winRingInitialized = TRUE;
        return TRUE;
    } else {
        return FALSE;
    }
}

// Exported function to deinitialize WinRing0
extern "C" __declspec(dllexport) VOID DeinitWinRing0() {
    if (winRingInitialized && DeinitializeOlsFunc) {
        DeinitializeOlsFunc();
    }
    if (hWinRing0) {
        FreeLibrary(hWinRing0);
    }
    hWinRing0 = NULL;
    InitializeOlsFunc = NULL;
    DeinitializeOlsFunc = NULL;
    GetDllStatusFunc = NULL;
    ReadIoPortByteFunc = NULL;
    WriteIoPortByteFunc = NULL;
    winRingLoaded = FALSE;
    winRingInitialized = FALSE;
    lastLoadError = 0; // Reset error on deinit
}

// Exported function to read from an I/O port
extern "C" __declspec(dllexport) BYTE ReadPort(WORD port) {
    if (!winRingInitialized || !ReadIoPortByteFunc) {
        return 0; // Or some other error indicator
    }
    // Add small delay matching _ec_wait() if necessary for stability
    // Sleep(1); // Example: 1 millisecond delay
    return ReadIoPortByteFunc(port);
}

// Exported function to write to an I/O port
extern "C" __declspec(dllexport) VOID WritePort(WORD port, BYTE value) {
    if (!winRingInitialized || !WriteIoPortByteFunc) {
        return;
    }
    // Add small delay matching _ec_wait() if necessary for stability
    // Sleep(1); // Example: 1 millisecond delay
    WriteIoPortByteFunc(port, value);
}

// Exported function to get WinRing0 status OR the LoadLibrary error
extern "C" __declspec(dllexport) DWORD GetStatus() {
    // If loading failed, return the LoadLibrary error code
    if (!winRingLoaded && lastLoadError != 0) {
        return lastLoadError;
    }
    // If WinRing0's GetDllStatus function pointer is valid, call it
    if (GetDllStatusFunc) {
        return GetDllStatusFunc();
    }
    // Otherwise, return a generic error/unknown status
    return 0xFFFFFFFF; // Example error code for status unavailable
}

// DLL entry point (optional, can be used for cleanup if process detaches unexpectedly)
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        // Optional: Attempt cleanup if the DLL is unloaded unexpectedly
        // DeinitWinRing0(); // Be cautious with cleanup here, might cause issues if called implicitly
        break;
    }
    return TRUE;
}
