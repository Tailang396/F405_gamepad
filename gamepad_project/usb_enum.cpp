#include <windows.h>
#include <setupapi.h>
#include <usb.h>
#include <usbioctl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// GUID_DEVINTERFACE_USB_HUB, defined locally to avoid SDK-specific GUID linking.
static const GUID hub_guid = {0xf18a0e88, 0xc30c, 0x11d0,
                              {0x88, 0x15, 0x00, 0xa0, 0xc9, 0x06, 0xbe, 0xd8}};

static unsigned le16(const unsigned char* p) { return p[0] | (unsigned(p[1]) << 8); }

static bool get_config(HANDLE hub, ULONG port, UCHAR config_index, std::vector<unsigned char>& data) {
    const size_t prefix = sizeof(USB_DESCRIPTOR_REQUEST);
    auto fetch = [&](size_t bytes, std::vector<unsigned char>& result) {
        std::vector<unsigned char> request(prefix + bytes, 0);
        auto* r = reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(request.data());
        r->ConnectionIndex = port;
        r->SetupPacket.bmRequest = 0x80;
        r->SetupPacket.bRequest = 0x06;
        r->SetupPacket.wValue = static_cast<USHORT>((2u << 8) | config_index);
        r->SetupPacket.wLength = static_cast<USHORT>(bytes);
        DWORD returned = 0;
        if (!DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
                             r, static_cast<DWORD>(request.size()), r,
                             static_cast<DWORD>(request.size()), &returned, nullptr) || returned < prefix)
            return false;
        result.assign(request.begin() + prefix, request.begin() + std::min<size_t>(returned, request.size()));
        return true;
    };
    std::vector<unsigned char> head;
    if (!fetch(9, head) || head.size() < 9 || head[1] != 2) return false;
    const auto total = le16(head.data() + 2);
    return total >= 9 && fetch(total, data) && data.size() >= total;
}

static void print_config(const std::vector<unsigned char>& b) {
    for (size_t pos = 0; pos + 2 <= b.size();) {
        const unsigned length = b[pos], type = b[pos + 1];
        if (length < 2 || pos + length > b.size()) { std::puts("  malformed descriptor"); return; }
        const auto* p = b.data() + pos;
        if (type == 2 && length >= 9)
            std::printf("  CONFIG value=%u interfaces=%u total=%u attributes=0x%02X maxPower=%u mA\n",
                        p[5], p[4], le16(p + 2), p[7], 2u * p[8]);
        else if (type == 4 && length >= 9)
            std::printf("  INTERFACE number=%u alt=%u endpoints=%u class=%02X subclass=%02X protocol=%02X\n",
                        p[2], p[3], p[4], p[5], p[6], p[7]);
        else if (type == 5 && length >= 7)
            std::printf("    ENDPOINT address=0x%02X direction=%s type=%s maxPacket=%u interval=%u\n",
                        p[2], (p[2] & 0x80) ? "IN" : "OUT",
                        (p[3] & 3) == 3 ? "interrupt" : (p[3] & 3) == 2 ? "bulk" :
                        (p[3] & 3) == 1 ? "isochronous" : "control", le16(p + 4), p[6]);
        else
            std::printf("    descriptor type=0x%02X length=%u\n", type, length);
        pos += length;
    }
}

int main() {
    HDEVINFO list = SetupDiGetClassDevsW(&hub_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (list == INVALID_HANDLE_VALUE) { std::printf("Hub enumeration failed: %lu\n", GetLastError()); return 1; }
    unsigned hubs = 0, devices = 0, failures = 0;
    for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(list, nullptr, &hub_guid, i, &iface)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) { std::printf("Hub enumeration error: %lu\n", GetLastError()); ++failures; }
            break;
        }
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(list, &iface, nullptr, 0, &needed, nullptr);
        if (!needed) { ++failures; continue; }
        std::vector<unsigned char> buf(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(list, &iface, detail, needed, nullptr, nullptr)) { ++failures; continue; }
        HANDLE hub = CreateFileW(detail->DevicePath, 0, FILE_SHARE_WRITE | FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, 0, nullptr);
        if (hub == INVALID_HANDLE_VALUE) { std::printf("Cannot open hub %lu: error %lu\n", i, GetLastError()); ++failures; continue; }
        ++hubs;
        USB_NODE_INFORMATION node{}; node.NodeType = UsbHub;
        DWORD returned = 0;
        if (!DeviceIoControl(hub, IOCTL_USB_GET_NODE_INFORMATION, &node, sizeof(node), &node, sizeof(node), &returned, nullptr)) {
            std::printf("Cannot query hub %lu: error %lu\n", i, GetLastError()); ++failures; CloseHandle(hub); continue;
        }
        for (ULONG port = 1; port <= node.u.HubInformation.HubDescriptor.bNumberOfPorts; ++port) {
            std::vector<unsigned char> info_buffer(sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + 32 * sizeof(USB_PIPE_INFO), 0);
            auto* info = reinterpret_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(info_buffer.data());
            info->ConnectionIndex = port;
            if (!DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
                                 info, static_cast<DWORD>(info_buffer.size()), info, static_cast<DWORD>(info_buffer.size()), &returned, nullptr)) {
                std::printf("Hub %lu port %lu: query failed: %lu\n", i, port, GetLastError()); ++failures; continue;
            }
            if (info->ConnectionStatus != DeviceConnected) continue;
            ++devices;
            const auto& d = info->DeviceDescriptor;
            std::printf("\nHub %lu port %lu VID=%04X PID=%04X deviceClass=%02X subclass=%02X protocol=%02X configurations=%u\n",
                        i, port, d.idVendor, d.idProduct, d.bDeviceClass, d.bDeviceSubClass, d.bDeviceProtocol, d.bNumConfigurations);
            for (UCHAR n = 0; n < d.bNumConfigurations; ++n) {
                std::vector<unsigned char> config;
                if (get_config(hub, port, n, config)) print_config(config);
                else { std::printf("  CONFIG index=%u read failed (error=%lu)\n", n, GetLastError()); ++failures; }
            }
        }
        CloseHandle(hub);
    }
    SetupDiDestroyDeviceInfoList(list);
    std::printf("\nHubs opened=%u devices=%u errors=%u\n", hubs, devices, failures);
    return hubs ? 0 : 1;
}
