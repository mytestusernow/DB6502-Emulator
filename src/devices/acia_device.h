/*
 * DB6502 Emulator - 65C51 ACIA device
 *
 * WDC 65C51 Asynchronous Communications Interface Adapter
 * with ImGui serial terminal window
 */

#ifndef _DB6502_ACIA_DEVICE_H_
#define _DB6502_ACIA_DEVICE_H_

#include "devices/device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Function:  createAciaDevice
 * --------------------
 * create a 65C51 ACIA device with ImGui terminal
 */
HBC56Device createAciaDevice(uint16_t baseAddr, uint8_t irq);

/* Function:  aciaDeviceReceiveByte
 * --------------------
 * push a byte into the ACIA receive buffer (from terminal input)
 */
void aciaDeviceReceiveByte(HBC56Device* device, uint8_t byte);

/* Function:  aciaRenderTerminal
 * --------------------
 * render the ImGui terminal window
 */
/* Function:  aciaDeviceRxBufEmpty
 * --------------------
 * returns non-zero if the ACIA receive buffer is empty
 */
int aciaDeviceRxBufEmpty(HBC56Device* device);

void aciaRenderTerminal(HBC56Device* device, bool* show);

#ifdef __cplusplus
}
#endif

#endif
