#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <chrono>
#include <cstdio>
#include <thread>

struct Search { IDirectInput8A* input; IDirectInputDevice8A* device; };

static BOOL CALLBACK find_device(const DIDEVICEINSTANCEA* info, VOID* context) {
    auto* search = static_cast<Search*>(context);
    std::printf("Found: %s VID=%04X PID=%04X\n", info->tszProductName,
                LOWORD(info->guidProduct.Data1), HIWORD(info->guidProduct.Data1));
    // VID 045e / PID 028e is the Xbox-compatible interface reported by this pad.
    if (LOWORD(info->guidProduct.Data1) != 0x045e || HIWORD(info->guidProduct.Data1) != 0x028e)
        return DIENUM_CONTINUE;
    if (FAILED(search->input->CreateDevice(info->guidInstance, &search->device, nullptr)))
        return DIENUM_CONTINUE;
    std::printf("DirectInput device: %s (VID 045E PID 028E)\n", info->tszProductName);
    return DIENUM_STOP;
}

int main() {
    IDirectInput8A* input = nullptr;
    HRESULT result = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
                                         IID_IDirectInput8A, reinterpret_cast<void**>(&input), nullptr);
    if (FAILED(result)) { std::printf("DirectInput initialization failed: 0x%08lx\n", static_cast<unsigned long>(result)); return 1; }
    Search search{input, nullptr};
    result = input->EnumDevices(DI8DEVCLASS_GAMECTRL, find_device, &search, DIEDFL_ATTACHEDONLY);
    if (FAILED(result) || !search.device) {
        std::puts("No matching controller found. Connect the controller in PC/XInput mode.");
        input->Release(); return 1;
    }
    auto* device = search.device;
    result = device->SetDataFormat(&c_dfDIJoystick2);
    if (SUCCEEDED(result)) result = device->SetCooperativeLevel(GetConsoleWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(result)) {
        std::printf("Device setup failed: 0x%08lx\n", static_cast<unsigned long>(result));
        device->Release(); input->Release(); return 1;
    }
    std::puts("Live DirectInput state every 250 ms. Ctrl+C to stop.\n"
              "Button numbers follow Windows mapping; Z may combine the triggers.");
    for (;;) {
        result = device->Poll();
        if (FAILED(result)) result = device->Acquire();
        DIJOYSTATE2 state{};
        if (SUCCEEDED(result)) result = device->GetDeviceState(sizeof(state), &state);
        if (FAILED(result)) {
            std::printf("Waiting for device (0x%08lx)...\n", static_cast<unsigned long>(result));
        } else {
            std::printf("X=%ld Y=%ld Z=%ld RX=%ld RY=%ld RZ=%ld POV=", state.lX, state.lY,
                        state.lZ, state.lRx, state.lRy, state.lRz);
            if (LOWORD(state.rgdwPOV[0]) == 0xffff) std::printf("- ");
            else std::printf("%lu ", static_cast<unsigned long>(state.rgdwPOV[0]));
            std::printf("buttons=");
            bool any = false;
            for (unsigned i = 0; i < 128; ++i) if (state.rgbButtons[i] & 0x80) {
                std::printf("%s%u", any ? "," : "", i + 1); any = true;
            }
            if (!any) std::printf("-");
            std::putchar('\n');
        }
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
