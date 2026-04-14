; Pico3D ST driver for SidecarTridge Multi-device
; Based on md-sprites-demo/target/atarist/src/main.s
; Adds: ACIA keyboard polling, palette loading from shared memory

ROM4_ADDR           equ $FA0000
FRAMEBUFFER_A_ADDR  equ ($FB0000 - 32000)
FRAMEBUFFER_B_ADDR  equ ($FB0000 - 64000)
COPIED_CODE_OFFSET  equ $00010000
COPIED_CODE_SIZE    equ $00005000
PRE_RESET_WAIT      equ $0000FFFF
SCREEN_A_BASE_ADDR  equ $70000
SCREEN_B_BASE_ADDR  equ $78000
COPYCODE_A_SRCADDR  equ (ROM4_ADDR + $600)
COPYCODE_B_SRCADDR  equ (ROM4_ADDR + $2600)
COPYCODE_A_ADDR     equ (SCREEN_A_BASE_ADDR - COPIED_CODE_OFFSET + $600)
COPYCODE_B_ADDR     equ (SCREEN_A_BASE_ADDR - COPIED_CODE_OFFSET + $2600)
COPYCODE_SIZE       equ $1F46  ; 500 × 16-byte MOVEM pairs + 4 preamble + 4 postamble + 2 RTS

; Shared memory layout in ROM4 space
KEY_BITMAP_ADDR     equ (ROM4_ADDR + $500)  ; 16-byte IKBD scancode bitmap
PALETTE_ADDR        equ (ROM4_ADDR + $520)  ; 16 × uint16_t palette words

; Hardware palette registers
HW_PALETTE          equ $FFFF8240

; IKBD ACIA
ACIA_STATUS         equ $FFFFFC00
ACIA_DATA           equ $FFFFFC02
ACIA_RDRF           equ 1           ; bit 0: Receive Data Register Full

; Video base address registers
VIDEO_BASE_ADDR_LOW  equ $ffff820d
VIDEO_BASE_ADDR_MID  equ $ffff8203
VIDEO_BASE_ADDR_HIGH equ $ffff8201

; Blitter registers
BLT_BASE            equ $FFFF8A00
BLT_SRC_XINC        equ (BLT_BASE+$20)
BLT_SRC_YINC        equ (BLT_BASE+$22)
BLT_SRC_ADDR        equ (BLT_BASE+$24)
BLT_ENDMASK1        equ (BLT_BASE+$28)
BLT_ENDMASK2        equ (BLT_BASE+$2A)
BLT_ENDMASK3        equ (BLT_BASE+$2C)
BLT_DST_XINC        equ (BLT_BASE+$2E)
BLT_DST_YINC        equ (BLT_BASE+$30)
BLT_DST_ADDR        equ (BLT_BASE+$32)
BLT_XCNT            equ (BLT_BASE+$36)
BLT_YCNT            equ (BLT_BASE+$38)
BLT_HOP             equ (BLT_BASE+$3A)
BLT_OP              equ (BLT_BASE+$3B)
BLT_CTRL            equ (BLT_BASE+$3C)
BLT_SKEW            equ (BLT_BASE+$3D)
BLT_HOG_MODE        equ %11000000

_conterm            equ $484
_dskbufp            equ $4c6
_p_cookies          equ $5a0
COOKIE_JAR_STE      equ $00010000
COOKIE_JAR_MEGASTE  equ $00010010

ROMCMD_START_ADDR   equ $FB0000
CMD_BOOSTER         equ ($ABCD)
CMD_VBLANK          equ ($DCBA)
CMD_START_DEMO      equ ($E1A8)

LISTENER_ADDR       equ (ROM4_ADDR + $5F8)
REMOTE_RESET        equ $1

    include inc/tos.s

; ─── Macros ─────────────────────────────────────────────────────────────────

vsync_wait      macro
                move.w #37,-(sp)
                trap #14
                addq.l #2,sp
                endm

get_rez         macro
                move.w #4,-(sp)
                trap #14
                addq.l #2,sp
                endm

; Load 16 palette words from ROM4 shared memory into ST hardware palette
; Trashes: d5, a4
load_palette    macro
                lea PALETTE_ADDR, a4
                lea HW_PALETTE.w, a5
                moveq #15, d5
.\@pal_loop:    move.w (a4)+, (a5)+
                dbf d5, .\@pal_loop
                endm

; Poll ACIA and update 16-byte scancode bitmap in ROM4 shared memory.
; Each byte in the bitmap covers 8 scancodes: bit set = key held.
; Make code (bit7=0) sets the bit; break code (bit7=1) clears it.
; Trashes: d5, d6, a4
poll_keyboard   macro
                lea KEY_BITMAP_ADDR, a4
.\@poll_loop:   btst #ACIA_RDRF, ACIA_STATUS.w ; data available?
                beq .\@poll_done
                move.b ACIA_DATA.w, d5       ; read scancode
                move.b d5, d6
                andi.w #$7F, d6              ; strip break bit → byte index
                move.w d6, d5                ; keep clean index in d5
                lsr.w #3, d5                 ; byte offset = scancode / 8
                moveq #7, d6
                sub.w d5, d6                 ; bit position within byte
                ; note: we need original to check break bit
                move.b ACIA_DATA.w, d5       ; re-read? No, already in d6
                ; Actually we already have the byte in d5 from the first read.
                ; Re-derive: scancode is already in d5 from above.
                ; Let's redo cleanly:
                bra .\@poll_done             ; placeholder: real impl below
.\@poll_done:
                endm

; Simplified keyboard polling (correct implementation)
; Reads all available ACIA bytes and updates the bitmap.
; Trashes d5, d6, d7, a4
poll_kb_impl    macro
                lea KEY_BITMAP_ADDR, a4
.\@kb_loop:     btst.b #0, ACIA_STATUS.w     ; RDRF: data ready?
                beq .\@kb_done
                move.b ACIA_DATA.w, d7       ; read scancode byte
                move.b d7, d5
                andi.b #$7F, d5              ; mask off break bit → raw scancode
                move.b d5, d6
                lsr.b #3, d6                 ; byte index (scancode/8)
                moveq #0, d5
                move.b d5, d5                ; clear d5
                move.b d7, d5
                andi.b #$7F, d5              ; raw scancode again
                andi.b #$07, d5              ; bit index = scancode & 7
                moveq #1, d6                 ; d6 = bit mask
                lsl.b d5, d6                 ; d6 = (1 << bit_index)
                ; byte offset
                move.b d7, d5
                andi.b #$7F, d5              ; raw scancode
                lsr.b #3, d5                 ; byte offset
                btst #7, d7                 ; break code? (btst on Dn has no size suffix)
                bne .\@break_key
.\@make_key:    or.b d6, (a4, d5.w)         ; set bit
                bra .\@kb_loop
.\@break_key:   not.b d6
                and.b d6, (a4, d5.w)         ; clear bit
                bra .\@kb_loop
.\@kb_done:
                endm

check_esc       macro
                btst.b #0, ACIA_STATUS.w
                beq .\@no_esc_key
                move.b ACIA_DATA.w, d5
                andi.b #$7F, d5
                cmp.b #$01, d5              ; ESC scancode
                bne .\@no_esc_key
                ; send CMD_BOOSTER to RP2040
                move.l #(ROMCMD_START_ADDR + $8000), a0
                move.w #CMD_BOOSTER, d7
                tst.b (a0, d7.w)
.\@no_esc_key:
                endm

check_commands  macro
                move.l (LISTENER_ADDR), d6
                cmp.l #REMOTE_RESET, d6
                beq .reset
                endm

; ─── ROM cartridge header ───────────────────────────────────────────────────

    section
    org ROM4_ADDR

    dc.l $abcdef42
first:
    dc.l 0
    dc.l $08000000 + pre_auto   ; After GEMDOS init
    dc.l 0
    dc.w GEMDOS_TIME
    dc.w GEMDOS_DATE
    dc.l end_pre_auto - pre_auto
    dc.b "PICO3D",0
    even

; ─── Pre-auto init ──────────────────────────────────────────────────────────

pre_auto:

.get_resolution:
    get_rez
    tst.w d0
    bne lowres_only

    ; Wait for RP2040 to write MOVEM.L copy code into ROM_IN_RAM
    move.l #(50 * 5), d7
wait_code:
    vsync_wait
    subq #1, d7
    beq boot_gem
    move.w #$070, $FFFF8240.w
    cmp.w #$4E75, (COPYCODE_A_SRCADDR + COPYCODE_SIZE)
    bne.s wait_code
    move.w #$007, $FFFF8240.w
    cmp.w #$4E75, (COPYCODE_B_SRCADDR + COPYCODE_SIZE)
    bne.s wait_code

start_demo:
    ; Copy ROM code below screen memory to avoid bus conflicts during copy
    lea (SCREEN_A_BASE_ADDR-COPIED_CODE_OFFSET), a2
    move.l #COPIED_CODE_SIZE, d6
    lea ROM4_ADDR, a1
    lsr.w #2, d6
    subq #1, d6
.copy_rom_code:
    move.l (a1)+, (a2)+
    dbf d6, .copy_rom_code

    lea SCREEN_A_BASE_ADDR - COPIED_CODE_OFFSET + (start_rom_code - ROM4_ADDR), a3
    jmp (a3)

start_rom_code:
    ; Signal RP2040 that the ST is ready
    move.l #(ROMCMD_START_ADDR + $8000), a0
    move.w #CMD_START_DEMO, d7
    tst.b (a0, d7.w)

    ; Detect ST vs STE
    move.l _p_cookies.w, d0
    beq .loop_low_st
    movea.l d0, a0
.loop_mch_cookie:
    move.l (a0)+, d0
    beq .loop_low_st
    cmp.l #'_MCH', d0
    beq.s .found_mch_cookie
    addq.w #4, a0
    bra.s .loop_mch_cookie
.found_mch_cookie:
    move.l (a0)+, d0
    cmp.l #COOKIE_JAR_MEGASTE, d0
    beq .loop_low_ste
    cmp.l #COOKIE_JAR_STE, d0
    beq .loop_low_ste

; ─── ST main loop ───────────────────────────────────────────────────────────
.loop_low_st:

    vsync_wait

    ; Signal VBLANK to RP2040
    move.l #(ROMCMD_START_ADDR + $8000), a0
    move.w #CMD_VBLANK, d7
    tst.b (a0, d7.w)

    ; Load palette from shared memory
    load_palette

    move.w sr, _dskbufp.w
    ori.w #$0700, sr            ; disable interrupts

    tst.l $FA05FC               ; check framebuffer index
    beq.s .fb_b_st
.fb_a_st:
    jsr COPYCODE_A_ADDR
    move.b #(SCREEN_B_BASE_ADDR >> 16), d0
    move.b #((SCREEN_B_BASE_ADDR >> 8) & $FF), d1
    bra.s .continue_st
.fb_b_st:
    jsr COPYCODE_B_ADDR
    move.b #(SCREEN_A_BASE_ADDR >> 16), d0
    move.b #((SCREEN_A_BASE_ADDR >> 8) & $FF), d1
.continue_st:
    move.b d0, VIDEO_BASE_ADDR_HIGH.w
    move.b d1, VIDEO_BASE_ADDR_MID.w

    move.w _dskbufp.w, sr       ; restore interrupts

    ; Poll keyboard and update key bitmap
    poll_kb_impl

    check_commands

    bra .loop_low_st

; ─── STE main loop (blitter) ────────────────────────────────────────────────
.loop_low_ste:

    vsync_wait

    ; Signal VBLANK to RP2040
    move.l #(ROMCMD_START_ADDR + $8000), a0
    move.w #CMD_VBLANK, d7
    tst.b (a0, d7.w)

    ; Load palette from shared memory
    load_palette

    move.w sr, _dskbufp.w
    ori.w #$0700, sr

    move.w #2,   BLT_SRC_XINC.w
    move.w #2,   BLT_DST_XINC.w
    clr.w        BLT_SRC_YINC.w
    clr.w        BLT_DST_YINC.w
    move.w #$FFFF, BLT_ENDMASK1.w
    move.w #$FFFF, BLT_ENDMASK2.w
    move.w #$FFFF, BLT_ENDMASK3.w
    clr.b        BLT_SKEW.w
    move.w #16000, BLT_XCNT.w
    move.w #1,   BLT_YCNT.w
    move.b #$2,  BLT_HOP.w
    move.b #$3,  BLT_OP.w

    tst.l $FA05FC
    bne.s .fb_b_ste
.fb_a_ste:
    move.l #SCREEN_A_BASE_ADDR, BLT_DST_ADDR.w
    move.l #FRAMEBUFFER_A_ADDR, BLT_SRC_ADDR
    move.b #(SCREEN_B_BASE_ADDR >> 16), d0
    move.b #((SCREEN_B_BASE_ADDR >> 8) & $FF), d1
    move.b #((SCREEN_B_BASE_ADDR) & $FF), d2
    bra.s .continue_ste
.fb_b_ste:
    move.l #SCREEN_B_BASE_ADDR, BLT_DST_ADDR.w
    move.l #FRAMEBUFFER_B_ADDR, BLT_SRC_ADDR
    move.b #(SCREEN_A_BASE_ADDR >> 16), d0
    move.b #((SCREEN_A_BASE_ADDR >> 8) & $FF), d1
    move.b #((SCREEN_A_BASE_ADDR) & $FF), d2
.continue_ste:
    move.b d0, VIDEO_BASE_ADDR_HIGH.w
    move.b d1, VIDEO_BASE_ADDR_MID.w
    move.b d2, VIDEO_BASE_ADDR_LOW.w
    move.b #BLT_HOG_MODE, BLT_CTRL.w

    move.w _dskbufp.w, sr

    ; Poll keyboard
    poll_kb_impl

    check_commands

    bra .loop_low_ste

; ─── Reset handler ──────────────────────────────────────────────────────────
.reset:
    move.l #PRE_RESET_WAIT, d6
.wait_me:
    subq.l #1, d6
    bne.s .wait_me
    clr.l $420.w
    clr.l $43A.w
    clr.l $51A.w
    move.l $4.w, a0
    jmp (a0)
    nop

lowres_only:
    print lowres_only_txt

boot_gem:
    move.l #SCREEN_B_BASE_ADDR, d0
    move.w #-1, -(sp)
    move.l d0, -(sp)
    move.l d0, -(sp)
    move.w #5, -(sp)
    trap #14
    lea 12(sp), sp
    rts

lowres_only_txt:
    dc.b "Pico3D: low res only",$d,$a,0
    even

end_rom_code:
end_pre_auto:
    even
    dc.l 0
