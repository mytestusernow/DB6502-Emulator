/*
 * DB6502 Emulator - Configuration
 *
 * Based on Troy Schrapel's HBC-56 Emulator (MIT License)
 * https://github.com/visrealm/hbc-56/emulator
 *
 * Adapted for the DB6502 single board computer by Paul
 */

#ifndef _HBC56_CONFIG_H_
#define _HBC56_CONFIG_H_

/* emulator configuration values
  -------------------------------------------------------------------------- */
#define HBC56_HAVE_THREADS      0

#define HBC56_CLOCK_FREQ        4000000   /* 4 MHz emulation speed */
#define HBC56_AUDIO_FREQ        48000
#define HBC56_MAX_DEVICES       16

/* memory map configuration values
  -------------------------------------------------------------------------- */
#define HBC56_RAM_START         0x0000
#define HBC56_RAM_SIZE          0x8000    /* 32KB RAM */

#define HBC56_ROM_START         0x8000
#define HBC56_ROM_SIZE          0x8000    /* 32KB ROM */

/* DB6502 uses direct memory-mapped I/O, no separate I/O window */
#define HBC56_IO_START          0x8000
#define HBC56_IO_SIZE           0x2000

/* device configuration values - DB6502 memory map
  -------------------------------------------------------------------------- */
#define HBC56_HAVE_TMS9918      1
#define HBC56_TMS9918_DAT_ADDR  0x8200
#define HBC56_TMS9918_REG_ADDR  0x8201
#define HBC56_TMS9918_IRQ       0         /* disabled - ROM doesn't handle VDP IRQs */

#define HBC56_HAVE_AY_3_8910    1
#define HBC56_AY_3_8910_COUNT   1
#define HBC56_AY38910_A_ADDR    0x8300
#define HBC56_AY38910_CLOCK     1000000   /* 1 MHz */

#define HBC56_HAVE_ACIA         1
#define HBC56_ACIA_ADDR         0x8400
#define HBC56_ACIA_IRQ          2

#define HBC56_HAVE_VIA2         1
#define HBC56_VIA2_ADDR         0x8800
#define HBC56_VIA2_IRQ          0         /* disabled - ROM doesn't handle VIA2 IRQs */

#define HBC56_HAVE_VIA          1
#define HBC56_VIA_ADDR          0x9000
#define HBC56_VIA_IRQ           0         /* disabled - ROM doesn't handle VIA1 IRQs */

#define HBC56_HAVE_KB           1
#define HBC56_KB_ADDR           0x9000    /* KB on VIA1 port A */
#define HBC56_KB_IRQ            0         /* disabled - ROM doesn't handle KB IRQs */

/* disabled devices */
#define HBC56_HAVE_LCD          0
#define HBC56_HAVE_NES          0

/* computed configuration values
  -------------------------------------------------------------------------- */
#define HBC56_RAM_END           (HBC56_RAM_START + HBC56_RAM_SIZE)
#define HBC56_ROM_END           (HBC56_ROM_START + HBC56_ROM_SIZE)
#define HBC56_RAM_MASK          ~HBC56_RAM_START
#define HBC56_ROM_MASK          ~HBC56_ROM_START
#define HBC56_IO_PORT_MASK      (HBC56_IO_SIZE - 1)

/* compatibility macro - not used for DB6502 but kept for shared code */
#define HBC56_IO_ADDRESS(p)     (p)


#endif
