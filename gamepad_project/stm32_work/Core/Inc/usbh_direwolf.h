#ifndef USBH_DIREWOLF_H
#define USBH_DIREWOLF_H

#include "usbh_core.h"

extern USBH_ClassTypeDef USBH_Direwolf_Class;
/* Called from the USB host process, not an interrupt. Data valid until return. */
void Direwolf_OnReport(const uint8_t *data, uint32_t length);

#endif
