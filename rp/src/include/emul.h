/**
 * emul.h — SidecarTridge emulation core for Pico3D
 */

#ifndef EMUL_H
#define EMUL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "debug.h"
#include "memfunc.h"
#include "hardware/dma.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdlib.h"
#include "reset.h"
#include "romemul.h"
#include "select.h"
#include "vga/font.h"
#include "vga/vga.h"

/* ROM3 / ROM4 GPIO signals */
#ifndef ROM3_GPIO
#define ROM3_GPIO 26
#endif
#ifndef ROM4_GPIO
#define ROM4_GPIO 22
#endif

/* High bit of the address bus word */
#define ADDRESS_HIGH_BIT 0x8000

/* Shared memory layout offsets (within ROM_IN_RAM) */
#define CUSTOM_FRAMEBUFFER_INDEX  0x5FC   /* which framebuffer is active */
#define CUSTOM_DISPLAY_COMMAND    0x5F8   /* command channel to ST */
#define ST_KEY_BITMAP_OFFSET      0x500   /* 16-byte IKBD key state (scancodes 0-127) */
#define ST_ESC_EXIT_ENABLE_OFFSET 0x510   /* 1 byte: non-zero enables ST ESC->Booster shortcut */
#define ST_PALETTE_OFFSET         0x520   /* 16 x uint16_t ST palette words (32 bytes) */

/* Sleep loop period */
#define SLEEP_LOOP_MS 100

/* Commands written to CUSTOM_DISPLAY_COMMAND */
#define DISPLAY_COMMAND_NOP       0x0
#define DISPLAY_COMMAND_RESET     0x1
#define DISPLAY_COMMAND_CONTINUE  0x2
#define DISPLAY_COMMAND_BOOSTER   0x3
#define DISPLAY_COMMAND_START     0x4

/* Shared-variable block (at TERM_RANDOM_TOKEN_OFFSET) */
#define TERM_RANDOM_TOKEN_OFFSET           0xF000
#define TERM_RANDON_TOKEN_SEED_OFFSET      (TERM_RANDOM_TOKEN_OFFSET + 4)
#define SHARED_VARIABLE_SHARED_FUNCTIONS_SIZE 16
#define TERM_SHARED_VARIABLES_OFFSET       (TERM_RANDOM_TOKEN_OFFSET + (SHARED_VARIABLE_SHARED_FUNCTIONS_SIZE * 4))
#define TERM_HARDWARE_TYPE    (0)
#define TERM_HARDWARE_VERSION (1)

#define SEND_COMMAND_TO_DISPLAY(command) \
    do { \
        DPRINTF("Sending command: %08x\n", command); \
        WRITE_AND_SWAP_LONGWORD(emul_getCommandAddress(), 0, command); \
    } while (0)

uint32_t emul_getCommandAddress(void);
void emul_preinit(void);
void emul_start(void);

/* Set the address in ROM_IN_RAM where the ST writes the IKBD key bitmap */
void input_set_key_bitmap_address(const void *addr);

#endif /* EMUL_H */
