#include "gamepad.h"
#include "gamepad_usb.h"
#include "gamepad_internal.h"
#include "usbh_ioreq.h"
#include "usbh_pipes.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* USB 类私有状态；缓冲区用于接收中断 IN 端点的数据。 */
typedef struct {
    uint8_t buffer[BSP_GAMEPAD_REPORT_MAX];
    uint8_t pipe, endpoint, packet_size, interval, waiting;
    uint32_t last_submit;
    uint8_t out_pipe, out_endpoint, out_packet_size, out_interval;
    uint8_t out_inflight, out_retries;
    uint32_t out_last_submit;
} gamepad_host_t;

static gamepad_host_t host;
/* APP 仅提交请求；USB Host 任务独占缓冲区直到传输结束。 */
static uint8_t out_buffer[BSP_GAMEPAD_REPORT_MAX];
static uint8_t out_length;
static uint8_t out_ready;
static gamepad_tx_status_t out_status = GAMEPAD_TX_DISCONNECTED;
static USBH_StatusTypeDef gamepad_init(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef gamepad_deinit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef gamepad_request(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef gamepad_process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef gamepad_sof(USBH_HandleTypeDef *phost);
static void gamepad_process_out(USBH_HandleTypeDef *phost);

uint8_t gamepad_usb_queue_send(const uint8_t *data, uint8_t length)
{
    uint8_t accepted = 0U;
    if (data == NULL || length == 0U || length > BSP_GAMEPAD_REPORT_MAX) return 0U;
    taskENTER_CRITICAL();
    if (out_ready && out_status != GAMEPAD_TX_PENDING && length <= host.out_packet_size) {
        memcpy(out_buffer, data, length);
        out_length = length;
        out_status = GAMEPAD_TX_PENDING;
        accepted = 1U;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

gamepad_tx_status_t gamepad_usb_send_status(void)
{
    gamepad_tx_status_t status;
    taskENTER_CRITICAL();
    status = out_status;
    taskEXIT_CRITICAL();
    return status;
}

/* ST USB Host 在枚举、收包和断开时调用这些类回调。 */
USBH_ClassTypeDef USBH_Gamepad_Class = {
    "Gamepad", 0xffU, gamepad_init, gamepad_deinit,
    gamepad_request, gamepad_process, gamepad_sof, NULL
};

static USBH_StatusTypeDef gamepad_init(USBH_HandleTypeDef *phost)
{
    uint8_t i, n;
    memset(&host, 0, sizeof(host));
    host.pipe = 0xffU;
    host.out_pipe = 0xffU;
    gamepad_store_reset();
    /* 限定已观察到的设备 ID 和接口；相同 ID 不保证设备身份唯一。 */
    if (phost->device.DevDesc.idVendor != 0x045eU ||
        phost->device.DevDesc.idProduct != 0x028eU) return USBH_FAIL;
    i = USBH_FindInterface(phost, 0xffU, 0x5dU, 0x01U);
    if (i == 0xffU || i >= USBH_MAX_NUM_INTERFACES ||
        USBH_SelectInterface(phost, i) != USBH_OK) return USBH_FAIL;
    /* 从描述符查找中断 IN 端点，并按其包长和间隔配置 pipe。 */
    for (n = 0; n < phost->device.CfgDesc.Itf_Desc[i].bNumEndpoints &&
                n < USBH_MAX_NUM_ENDPOINTS; ++n) {
        USBH_EpDescTypeDef *ep = &phost->device.CfgDesc.Itf_Desc[i].Ep_Desc[n];
        if ((ep->bEndpointAddress & 0x80U) &&
            (ep->bmAttributes & 3U) == USB_EP_TYPE_INTR &&
            ep->wMaxPacketSize > 0U && ep->wMaxPacketSize <= BSP_GAMEPAD_REPORT_MAX) {
            host.endpoint = ep->bEndpointAddress;
            host.packet_size = (uint8_t)ep->wMaxPacketSize;
            host.interval = ep->bInterval ? ep->bInterval : 1U;
        } else if (!(ep->bEndpointAddress & 0x80U) &&
                   (ep->bmAttributes & 3U) == USB_EP_TYPE_INTR &&
                   ep->wMaxPacketSize > 0U && ep->wMaxPacketSize <= BSP_GAMEPAD_REPORT_MAX) {
            host.out_endpoint = ep->bEndpointAddress;
            host.out_packet_size = (uint8_t)ep->wMaxPacketSize;
            host.out_interval = ep->bInterval ? ep->bInterval : 1U;
        }
    }
    if (!host.endpoint || !host.out_endpoint) return USBH_FAIL;
    host.pipe = USBH_AllocPipe(phost, host.endpoint);
    if (host.pipe == 0xffU) return USBH_FAIL;
    if (USBH_OpenPipe(phost, host.pipe, host.endpoint, phost->device.address,
                      phost->device.speed, USB_EP_TYPE_INTR, host.packet_size) != USBH_OK) {
        USBH_FreePipe(phost, host.pipe);
        host.pipe = 0xffU;
        return USBH_FAIL;
    }
    USBH_LL_SetToggle(phost, host.pipe, 0U);
    host.out_pipe = USBH_AllocPipe(phost, host.out_endpoint);
    if (host.out_pipe == 0xffU) {
        USBH_ClosePipe(phost, host.pipe);
        USBH_FreePipe(phost, host.pipe);
        host.pipe = 0xffU;
        return USBH_FAIL;
    }
    if (USBH_OpenPipe(phost, host.out_pipe, host.out_endpoint, phost->device.address,
                      phost->device.speed, USB_EP_TYPE_INTR, host.out_packet_size) != USBH_OK) {
        USBH_FreePipe(phost, host.out_pipe);
        USBH_ClosePipe(phost, host.pipe);
        USBH_FreePipe(phost, host.pipe);
        host.out_pipe = host.pipe = 0xffU;
        return USBH_FAIL;
    }
    USBH_LL_SetToggle(phost, host.out_pipe, 0U);
    return USBH_OK;
}

static USBH_StatusTypeDef gamepad_deinit(USBH_HandleTypeDef *phost)
{
    taskENTER_CRITICAL();
    out_ready = 0U;
    out_status = GAMEPAD_TX_DISCONNECTED;
    taskEXIT_CRITICAL();
    /* 拔出时释放 pipe，并通知 BSP 清除旧数据。 */
    if (host.pipe != 0xffU) {
        USBH_ClosePipe(phost, host.pipe);
        USBH_FreePipe(phost, host.pipe);
        host.pipe = 0xffU;
    }
    if (host.out_pipe != 0xffU) {
        USBH_ClosePipe(phost, host.out_pipe);
        USBH_FreePipe(phost, host.out_pipe);
        host.out_pipe = 0xffU;
    }
    gamepad_store_connected(0U);
    return USBH_OK;
}

static USBH_StatusTypeDef gamepad_request(USBH_HandleTypeDef *phost)
{
    /* 当前实现不发送额外初始化命令。 */
    gamepad_store_connected(1U);
    taskENTER_CRITICAL();
    out_ready = 1U;
    out_status = GAMEPAD_TX_IDLE;
    taskEXIT_CRITICAL();
    phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
    return USBH_OK;
}

static USBH_StatusTypeDef gamepad_process(USBH_HandleTypeDef *phost)
{
    /* 在 USB Host 任务中提交接收，并根据 URB 状态处理完成或错误。 */
    USBH_URBStateTypeDef state;
    gamepad_process_out(phost);
    if (!host.waiting) {
        if ((uint32_t)(phost->Timer - host.last_submit) < host.interval) return USBH_OK;
        if (USBH_InterruptReceiveData(phost, host.buffer, host.packet_size, host.pipe) == USBH_OK) {
            host.last_submit = phost->Timer;
            host.waiting = 1U;
        }
        return USBH_OK;
    }
    state = USBH_LL_GetURBState(phost, host.pipe);
    if (state == USBH_URB_DONE) {
        uint32_t length = USBH_LL_GetLastXferSize(phost, host.pipe);
        if (length > 0U && length <= sizeof(host.buffer)) {
            gamepad_store_report(host.buffer, (uint8_t)length);
        }
        host.waiting = 0U;
    } else if (state == USBH_URB_STALL) {
        if (USBH_ClrFeature(phost, host.endpoint) == USBH_OK) {
            host.waiting = 0U;
            gamepad_store_error();
        }
    } else if (state == USBH_URB_ERROR || state == USBH_URB_NOTREADY) {
        host.waiting = 0U;
        if (state == USBH_URB_ERROR) {
            gamepad_store_error();
        }
    }
    return USBH_OK;
}

/* 每次只处理一包 OUT；完成前不允许应用覆盖 out_buffer。 */
static void gamepad_process_out(USBH_HandleTypeDef *phost)
{
    USBH_URBStateTypeDef state;
    if (gamepad_usb_send_status() != GAMEPAD_TX_PENDING) return;

    if (host.out_inflight) {
        state = USBH_LL_GetURBState(phost, host.out_pipe);
        if (state == USBH_URB_DONE) {
            host.out_inflight = 0U;
            host.out_retries = 0U;
            taskENTER_CRITICAL(); out_status = GAMEPAD_TX_SENT; taskEXIT_CRITICAL();
        } else if (state == USBH_URB_NOTREADY || state == USBH_URB_NYET) {
            host.out_inflight = 0U;
            if (++host.out_retries >= 8U) {
                host.out_retries = 0U;
                taskENTER_CRITICAL(); out_status = GAMEPAD_TX_FAILED; taskEXIT_CRITICAL();
            }
        } else if (state == USBH_URB_ERROR || state == USBH_URB_STALL) {
            host.out_inflight = 0U;
            host.out_retries = 0U;
            taskENTER_CRITICAL(); out_status = GAMEPAD_TX_FAILED; taskEXIT_CRITICAL();
        }
        return;
    }

    if ((uint32_t)(phost->Timer - host.out_last_submit) < host.out_interval) return;
    host.out_last_submit = phost->Timer;
    if (USBH_LL_SubmitURB(phost, host.out_pipe, 0U, USBH_EP_INTERRUPT,
                          USBH_PID_DATA, out_buffer, out_length, 0U) == USBH_OK) {
        host.out_inflight = 1U;
    } else {
        host.out_retries = 0U;
        taskENTER_CRITICAL(); out_status = GAMEPAD_TX_FAILED; taskEXIT_CRITICAL();
    }
}

/* SOF 中断中按端点间隔发送事件，唤醒 USB Host 任务；此处不解析报文。 */
static USBH_StatusTypeDef gamepad_sof(USBH_HandleTypeDef *phost)
{
    if ((phost->Timer % host.interval) == 0U)
        USBH_OS_PutMessage(phost, USBH_URB_EVENT, 0U, 0U);
    return USBH_OK;
}
