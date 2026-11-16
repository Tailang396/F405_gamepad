#include <windows.h>
#include <Xinput.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

struct Button { WORD mask; const char* name; };

static void printState(DWORD index, const XINPUT_STATE& state) {
    static constexpr Button buttons[] = {
        {XINPUT_GAMEPAD_DPAD_UP, "UP"}, {XINPUT_GAMEPAD_DPAD_DOWN, "DOWN"},
        {XINPUT_GAMEPAD_DPAD_LEFT, "LEFT"}, {XINPUT_GAMEPAD_DPAD_RIGHT, "RIGHT"},
        {XINPUT_GAMEPAD_START, "START"}, {XINPUT_GAMEPAD_BACK, "BACK"},
        {XINPUT_GAMEPAD_LEFT_THUMB, "L3"}, {XINPUT_GAMEPAD_RIGHT_THUMB, "R3"},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"}, {XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"},
        {XINPUT_GAMEPAD_A, "A"}, {XINPUT_GAMEPAD_B, "B"},
        {XINPUT_GAMEPAD_X, "X"}, {XINPUT_GAMEPAD_Y, "Y"}
    };
    const auto& p = state.Gamepad;
    std::cout << "[" << index << "] packet=" << state.dwPacketNumber << " buttons=";
    bool any = false;
    for (const auto& button : buttons) {
        if (p.wButtons & button.mask) {
            if (any) std::cout << '+';
            std::cout << button.name;
            any = true;
        }
    }
    if (!any) std::cout << '-';
    std::cout << " mask=0x" << std::hex << std::setw(4) << std::setfill('0')
              << p.wButtons << std::dec << std::setfill(' ')
              << " LT=" << static_cast<unsigned>(p.bLeftTrigger)
              << " RT=" << static_cast<unsigned>(p.bRightTrigger)
              << " LX=" << p.sThumbLX << " LY=" << p.sThumbLY
              << " RX=" << p.sThumbRX << " RY=" << p.sThumbRY << '\n';
}

int main() {
    std::cout << "XInput monitor: move sticks or press buttons; Ctrl+C to exit.\n"
                 "Waiting for controllers in slots 0-3..." << std::endl;
    bool connected[XUSER_MAX_COUNT]{};
    DWORD lastPacket[XUSER_MAX_COUNT]{};
    auto nextScan = std::chrono::steady_clock::now();
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const bool scan = now >= nextScan;
        if (scan) nextScan = now + std::chrono::seconds(2);
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            if (!connected[i] && !scan) continue;
            XINPUT_STATE state{};
            const DWORD result = XInputGetState(i, &state);
            if (result == ERROR_SUCCESS) {
                if (!connected[i]) {
                    connected[i] = true;
                    std::cout << "Controller connected: slot " << i << '\n';
                    printState(i, state);
                } else if (state.dwPacketNumber != lastPacket[i]) {
                    printState(i, state);
                }
                lastPacket[i] = state.dwPacketNumber;
            } else if (connected[i]) {
                connected[i] = false;
                std::cout << "Controller disconnected: slot " << i << " (status=" << result << ")\n";
            }
        }
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
