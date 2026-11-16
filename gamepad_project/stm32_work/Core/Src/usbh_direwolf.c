#include "usbh_direwolf.h"
#include <string.h>

#define DW_REPORT_CAPACITY 64U

typedef struct {
    uint8_t buffer[DW_REPORT_CAPACITY];
    uint8_t pipe;
    uint8_t endpoint;
    uint8_t packet_size;
    uint8_t interval;
    uint8_t waiting;
    uint32_t last_submit;
} DirewolfHost;

static DirewolfHost host;
static USBH_StatusTypeDef dw_init(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef dw_deinit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef dw_request(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef dw_process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef dw_sof(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef USBH_Direwolf_Class = {
    "Direwolf2", 0xffU, dw_init, dw_deinit, dw_request, dw_process, dw_sof, NULL
};

static USBH_StatusTypeDef dw_init(USBH_HandleTypeDef *phost) {
    uint8_t i, n;
    memset(&host, 0, sizeof host);
    host.pipe = 0xffU;
    if (phost->device.DevDesc.idVendor != 0x045eU || phost->device.DevDesc.idProduct != 0x028eU)
        return USBH_FAIL;
    i = USBH_FindInterface(phost, 0xffU, 0x5dU, 0x01U);
    if (i == 0xffU || i >= USBH_MAX_NUM_INTERFACES || USBH_SelectInterface(phost, i) != USBH_OK)
        return USBH_FAIL;
    for (n = 0; n < phost->device.CfgDesc.Itf_Desc[i].bNumEndpoints && n < USBH_MAX_NUM_ENDPOINTS; ++n) {
        USBH_EpDescTypeDef *ep = &phost->device.CfgDesc.Itf_Desc[i].Ep_Desc[n];
        if ((ep->bEndpointAddress & 0x80U) && (ep->bmAttributes & 3U) == USB_EP_TYPE_INTR &&
            ep->wMaxPacketSize > 0U && ep->wMaxPacketSize <= DW_REPORT_CAPACITY) {
            host.endpoint = ep->bEndpointAddress;
            host.packet_size = (uint8_t)ep->wMaxPacketSize;
            host.interval = ep->bInterval ? ep->bInterval : 1U;
            break;
        }
    }
    if (!host.endpoint) return USBH_FAIL;
    host.pipe = USBH_AllocPipe(phost, host.endpoint);
    if (host.pipe == 0xffU) return USBH_FAIL;
    if (USBH_OpenPipe(phost, host.pipe, host.endpoint, phost->device.address,
                      phost->device.speed, USB_EP_TYPE_INTR, host.packet_size) != USBH_OK) {
        USBH_FreePipe(phost, host.pipe);
        host.pipe = 0xffU;
        return USBH_FAIL;
    }
    USBH_LL_SetToggle(phost, host.pipe, 0U);
    return USBH_OK;
}

static USBH_StatusTypeDef dw_deinit(USBH_HandleTypeDef *phost) {
    if (host.pipe != 0xffU) {
        USBH_ClosePipe(phost, host.pipe);
        USBH_FreePipe(phost, host.pipe);
        host.pipe = 0xffU;
    }
    return USBH_OK;
}

/* No undocumented commands are sent; some firmware may require initialization. */
static USBH_StatusTypeDef dw_request(USBH_HandleTypeDef *phost) {
    (void)phost;
    return USBH_OK;
}

static USBH_StatusTypeDef dw_process(USBH_HandleTypeDef *phost) {
    USBH_URBStateTypeDef urb;
    if (!host.waiting) {
        if (USBH_InterruptReceiveData(phost, host.buffer, host.packet_size, host.pipe) == USBH_OK) {
            host.last_submit = phost->Timer;
            host.waiting = 1U;
        }
        return USBH_OK;
    }
    urb = USBH_LL_GetURBState(phost, host.pipe);
    if (urb == USBH_URB_DONE) {
        uint32_t length = USBH_LL_GetLastXferSize(phost, host.pipe);
        if (length && length <= sizeof host.buffer) Direwolf_OnReport(host.buffer, length);
        host.waiting = 0U;
    } else if (urb == USBH_URB_STALL) {
        if (USBH_ClrFeature(phost, host.endpoint) == USBH_OK) host.waiting = 0U;
    } else if (urb == USBH_URB_ERROR || urb == USBH_URB_NOTREADY) {
        if ((uint32_t)(phost->Timer - host.last_submit) >= host.interval) host.waiting = 0U;
    }
    return USBH_OK;
}

static USBH_StatusTypeDef dw_sof(USBH_HandleTypeDef *phost) {
    (void)phost;
    return USBH_OK;
}
