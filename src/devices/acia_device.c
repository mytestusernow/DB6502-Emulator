/*
 * DB6502 Emulator - 65C51 ACIA device
 *
 * WDC 65C51 Asynchronous Communications Interface Adapter
 * Emulates serial I/O with an ImGui terminal window.
 *
 * Registers:
 *   base+0: Data register (R/W)
 *   base+1: Status register (R) / Programmed Reset (W)
 *   base+2: Command register (R/W)
 *   base+3: Control register (R/W)
 */

#include "devices/acia_device.h"
#include "hbc56emu.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static FILE* aciaLog = NULL;
static FILE* getAciaLog(void) {
  if (!aciaLog) {
    aciaLog = fopen("/tmp/acia_debug.log", "w");
    if (aciaLog) setbuf(aciaLog, NULL); /* unbuffered */
  }
  return aciaLog;
}

/* ACIA register offsets */
#define ACIA_DATA_REG     0x00
#define ACIA_STATUS_REG   0x01
#define ACIA_COMMAND_REG  0x02
#define ACIA_CONTROL_REG  0x03

/* Status register bits */
#define ACIA_STATUS_PE    0x01  /* Parity Error */
#define ACIA_STATUS_FE    0x02  /* Framing Error */
#define ACIA_STATUS_OVRN  0x04  /* Overrun */
#define ACIA_STATUS_RDRF  0x08  /* Receive Data Register Full */
#define ACIA_STATUS_TDRE  0x10  /* Transmit Data Register Empty */
#define ACIA_STATUS_DCD   0x20  /* Data Carrier Detect (active low) */
#define ACIA_STATUS_DSR   0x40  /* Data Set Ready (active low) */
#define ACIA_STATUS_IRQ   0x80  /* Interrupt flag */

/* Command register bits */
#define ACIA_CMD_DTR      0x01  /* Data Terminal Ready */
#define ACIA_CMD_RX_IRQ   0x02  /* Receiver IRQ disable (0=enabled) */
#define ACIA_CMD_TX_MASK  0x0C  /* Transmit control */
#define ACIA_CMD_ECHO     0x10  /* Echo mode */

/* Receive buffer */
#define ACIA_RX_BUF_SIZE  256
#define ACIA_RX_BUF_MASK  (ACIA_RX_BUF_SIZE - 1)

/* Terminal output buffer */
#define ACIA_TERM_BUF_SIZE 65536

/* Forward declarations */
static void resetAciaDevice(HBC56Device*);
static void destroyAciaDevice(HBC56Device*);
static uint8_t readAciaDevice(HBC56Device*, uint16_t, uint8_t*, uint8_t);
static uint8_t writeAciaDevice(HBC56Device*, uint16_t, uint8_t);
static void tickAciaDevice(HBC56Device*, uint32_t, float);

struct AciaDevice
{
  uint16_t  baseAddr;
  uint8_t   irq;

  /* registers */
  uint8_t   commandReg;
  uint8_t   controlReg;
  uint8_t   statusReg;

  /* receive buffer (circular) */
  uint8_t   rxBuffer[ACIA_RX_BUF_SIZE];
  int       rxHead;
  int       rxTail;

  /* terminal output buffer */
  char      termBuffer[ACIA_TERM_BUF_SIZE];
  int       termLen;
  int       termScrollToBottom;

  /* cursor position for basic terminal emulation */
  int       cursorX;
};
typedef struct AciaDevice AciaDevice;


static int rxBufCount(AciaDevice* acia)
{
  return (acia->rxHead - acia->rxTail) & ACIA_RX_BUF_MASK;
}

static void rxBufPush(AciaDevice* acia, uint8_t byte)
{
  acia->rxBuffer[acia->rxHead] = byte;
  acia->rxHead = (acia->rxHead + 1) & ACIA_RX_BUF_MASK;
}

static uint8_t rxBufPop(AciaDevice* acia)
{
  uint8_t byte = acia->rxBuffer[acia->rxTail];
  acia->rxTail = (acia->rxTail + 1) & ACIA_RX_BUF_MASK;
  return byte;
}

static void termPutChar(AciaDevice* acia, char c)
{
  if (c == '\r')
  {
    /* CR produces a newline */
    if (acia->termLen < ACIA_TERM_BUF_SIZE - 1)
    {
      acia->termBuffer[acia->termLen++] = '\n';
    }
    acia->cursorX = 0;
  }
  else if (c == '\n')
  {
    /* LF after CR - ignore to avoid double newline */
    /* If last char was already a newline, skip */
    if (acia->termLen > 0 && acia->termBuffer[acia->termLen - 1] == '\n')
    {
      /* skip duplicate newline from CR+LF sequence */
    }
    else
    {
      if (acia->termLen < ACIA_TERM_BUF_SIZE - 1)
      {
        acia->termBuffer[acia->termLen++] = '\n';
      }
      acia->cursorX = 0;
    }
  }
  else if (c == '\b' || c == 0x7F)
  {
    /* backspace */
    if (acia->termLen > 0 && acia->termBuffer[acia->termLen - 1] != '\n')
    {
      acia->termLen--;
      acia->cursorX--;
    }
  }
  else if (c >= 0x20 || c == '\t')
  {
    if (acia->termLen < ACIA_TERM_BUF_SIZE - 1)
    {
      acia->termBuffer[acia->termLen++] = c;
      acia->cursorX++;
    }
  }

  acia->termBuffer[acia->termLen] = '\0';
  acia->termScrollToBottom = 1;

  /* if buffer is getting full, discard first half */
  if (acia->termLen > ACIA_TERM_BUF_SIZE - 256)
  {
    int half = acia->termLen / 2;
    memmove(acia->termBuffer, acia->termBuffer + half, acia->termLen - half);
    acia->termLen -= half;
    acia->termBuffer[acia->termLen] = '\0';
  }
}

static void updateIrq(AciaDevice* acia)
{
  int irqActive = 0;

  /* RX interrupt: RDRF set and RX IRQ enabled (bit 1 of command = 0 means enabled) */
  if ((acia->statusReg & ACIA_STATUS_RDRF) && !(acia->commandReg & ACIA_CMD_RX_IRQ))
  {
    irqActive = 1;
  }

  if (irqActive)
  {
    acia->statusReg |= ACIA_STATUS_IRQ;
    hbc56Interrupt(acia->irq, INTERRUPT_RAISE);
  }
  else
  {
    acia->statusReg &= ~ACIA_STATUS_IRQ;
    hbc56Interrupt(acia->irq, INTERRUPT_RELEASE);
  }
}


HBC56Device createAciaDevice(uint16_t baseAddr, uint8_t irq)
{
  HBC56Device device = createDevice("65C51 ACIA");
  AciaDevice* acia = (AciaDevice*)calloc(1, sizeof(AciaDevice));
  if (acia)
  {
    acia->baseAddr = baseAddr;
    acia->irq = irq;
    acia->statusReg = ACIA_STATUS_TDRE; /* TX always ready */
    acia->termBuffer[0] = '\0';

    device.data = acia;
    device.resetFn = &resetAciaDevice;
    device.destroyFn = &destroyAciaDevice;
    device.readFn = &readAciaDevice;
    device.writeFn = &writeAciaDevice;
    device.tickFn = &tickAciaDevice;
  }
  return device;
}

static void resetAciaDevice(HBC56Device* device)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  acia->commandReg = 0x00;
  acia->controlReg = 0x00;
  acia->statusReg = ACIA_STATUS_TDRE;
  acia->rxHead = 0;
  acia->rxTail = 0;
  hbc56Interrupt(acia->irq, INTERRUPT_RELEASE);
}

static void destroyAciaDevice(HBC56Device* device)
{
  /* data freed by device framework */
}

static uint8_t readAciaDevice(HBC56Device* device, uint16_t addr, uint8_t* val, uint8_t dbg)
{
  AciaDevice* acia = (AciaDevice*)device->data;

  if (addr < acia->baseAddr || addr > acia->baseAddr + ACIA_CONTROL_REG) return 0;
  uint16_t reg = addr - acia->baseAddr;

  switch (reg)
  {
    case ACIA_DATA_REG:
      if (rxBufCount(acia) > 0)
      {
        *val = rxBufPop(acia);
        if (!dbg) if(getAciaLog()) fprintf(getAciaLog(), "[ACIA RD] 0x%02X '%c' (remaining=%d)\n",
          *val, (*val >= 0x20 && *val < 0x7F) ? *val : '.', rxBufCount(acia));
        if (rxBufCount(acia) == 0)
        {
          acia->statusReg &= ~ACIA_STATUS_RDRF;
        }
        if (!dbg) updateIrq(acia);
      }
      else
      {
        *val = 0x00;
        if (!dbg) if(getAciaLog()) fprintf(getAciaLog(), "[ACIA RD] EMPTY (no data!)\n");
      }
      break;

    case ACIA_STATUS_REG:
      *val = acia->statusReg;
      /* reading status clears IRQ bit */
      if (!dbg)
      {
        acia->statusReg &= ~ACIA_STATUS_IRQ;
      }
      break;

    case ACIA_COMMAND_REG:
      *val = acia->commandReg;
      break;

    case ACIA_CONTROL_REG:
      *val = acia->controlReg;
      break;
  }

  return 1;
}

static uint8_t writeAciaDevice(HBC56Device* device, uint16_t addr, uint8_t val)
{
  AciaDevice* acia = (AciaDevice*)device->data;

  if (addr < acia->baseAddr || addr > acia->baseAddr + ACIA_CONTROL_REG) return 0;
  uint16_t reg = addr - acia->baseAddr;

  switch (reg)
  {
    case ACIA_DATA_REG:
      /* transmit byte - output to terminal */
      if(getAciaLog()) fprintf(getAciaLog(), "[ACIA TX] 0x%02X '%c'\n", val, (val >= 0x20 && val < 0x7F) ? val : '.');
      termPutChar(acia, (char)val);
      break;

    case ACIA_STATUS_REG:
      /* writing to status reg performs programmed reset */
      acia->commandReg &= 0xE0; /* clear lower 5 bits */
      acia->statusReg &= ~ACIA_STATUS_OVRN;
      break;

    case ACIA_COMMAND_REG:
      acia->commandReg = val;
      updateIrq(acia);
      break;

    case ACIA_CONTROL_REG:
      acia->controlReg = val;
      break;
  }

  return 1;
}

static void tickAciaDevice(HBC56Device* device, uint32_t deltaTicks, float deltaTime)
{
  AciaDevice* acia = (AciaDevice*)device->data;

  /* check if we have data and RDRF is not set */
  if (rxBufCount(acia) > 0 && !(acia->statusReg & ACIA_STATUS_RDRF))
  {
    acia->statusReg |= ACIA_STATUS_RDRF;
    updateIrq(acia);
  }
}

void aciaDeviceReceiveByte(HBC56Device* device, uint8_t byte)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  if(getAciaLog()) fprintf(getAciaLog(), "[ACIA RX] 0x%02X '%c' (buf=%d, RDRF=%d, CMD=0x%02X)\n",
    byte, (byte >= 0x20 && byte < 0x7F) ? byte : '.',
    rxBufCount(acia), (acia->statusReg & ACIA_STATUS_RDRF) ? 1 : 0, acia->commandReg);
  rxBufPush(acia, byte);

  if (!(acia->statusReg & ACIA_STATUS_RDRF))
  {
    acia->statusReg |= ACIA_STATUS_RDRF;
    updateIrq(acia);
  }
}

int aciaDeviceRxBufEmpty(HBC56Device* device)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  return rxBufCount(acia) == 0;
}

void aciaRenderTerminal(HBC56Device* device, bool* show)
{
  /* This is a stub - terminal rendering is done in db6502emu.cpp using ImGui */
  (void)device;
  (void)show;
}

/* accessor for terminal buffer from C++ ImGui code */
const char* aciaGetTermBuffer(HBC56Device* device)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  return acia->termBuffer;
}

int aciaGetTermLen(HBC56Device* device)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  return acia->termLen;
}

int aciaGetScrollToBottom(HBC56Device* device)
{
  AciaDevice* acia = (AciaDevice*)device->data;
  int val = acia->termScrollToBottom;
  acia->termScrollToBottom = 0;
  return val;
}
