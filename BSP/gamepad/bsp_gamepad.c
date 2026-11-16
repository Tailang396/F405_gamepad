#include "bsp_gamepad.h"
#include <stddef.h>

/* 报文字节按小端顺序排列。 */
static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static int16_t read_i16_le(const uint8_t *data)
{
    uint16_t value = read_u16_le(data);
    return value < 0x8000U ? (int16_t)value : (int16_t)((int32_t)value - 65536);
}

static int16_t signed12(uint16_t value)
{
    return value < 0x800U ? (int16_t)value : (int16_t)((int32_t)value - 4096);
}

/* 解包原始快照，仅由本文件内部使用。 */
static bsp_gamepad_data_t bsp_gamepad_unpack(const gamepad_raw_data_t *raw)
{
    bsp_gamepad_data_t result = {0};
    const uint8_t *data;

    if (raw == NULL) return result;
    result.connected = raw->connected;
    if (!raw->connected || raw->length < 14U ||
        raw->length > BSP_GAMEPAD_REPORT_MAX ||
        raw->data[0] != 0x00U || raw->data[1] < 0x14U) return result;

    data = raw->data;
    result.buttons = read_u16_le(data + 2);
    result.lt = data[4];
    result.rt = data[5];
    result.lx = read_i16_le(data + 6);
    result.ly = read_i16_le(data + 8);
    result.rx = read_i16_le(data + 10);
    result.ry = read_i16_le(data + 12);

    if (raw->length >= 17U) {
        result.motion_h = signed12((uint16_t)(data[14] | ((uint16_t)(data[16] >> 4) << 8)));
        result.motion_v = signed12((uint16_t)(data[15] | ((uint16_t)(data[16] & 0x0fU) << 8)));
        result.motion_available = 1U;
    }
    result.valid = 1U;
    return result;
}

void bsp_gamepad_get_raw_data(gamepad_raw_data_t *snapshot)
{
    gamepad_get_raw_data(snapshot);
}

bsp_gamepad_data_t bsp_gamepad_get_data(void)
{
    gamepad_raw_data_t raw;
    bsp_gamepad_get_raw_data(&raw);
    return bsp_gamepad_unpack(&raw);
}

uint8_t bsp_gamepad_set_vibration(bsp_gamepad_rumble_side_t side, uint8_t strength)
{
    /* Xbox 360 兼容格式；是否支持及左右通道对应关系需实机验证。 */
    uint8_t command[8] = {0x00U, 0x08U, 0x00U, 0x00U,
                          0x00U, 0x00U, 0x00U, 0x00U};
    switch (side) {
    case BSP_GAMEPAD_RUMBLE_LEFT:
        command[3] = strength;
        break;
    case BSP_GAMEPAD_RUMBLE_RIGHT:
        command[4] = strength;
        break;
    case BSP_GAMEPAD_RUMBLE_BOTH:
        command[3] = strength;
        command[4] = strength;
        break;
    default:
        return 0U;
    }
    return bsp_gamepad_send_data(command, sizeof(command));
}

uint8_t bsp_gamepad_send_data(const uint8_t *data, uint8_t length)
{
    return gamepad_send_data(data, length);
}

gamepad_tx_status_t bsp_gamepad_get_send_status(void)
{
    return gamepad_get_send_status();
}


