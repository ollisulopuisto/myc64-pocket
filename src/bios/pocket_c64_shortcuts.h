#ifndef POCKET_C64_SHORTCUTS_H
#define POCKET_C64_SHORTCUTS_H

#include <stdint.h>
#include <stdbool.h>

#define C64_KEY_SPACE     0x74
#define C64_KEY_RETURN    0x01
#define C64_KEY_RUNSTOP   0x77
#define C64_KEY_ONE       0x70
#define C64_KEY_TWO       0x73
#define C64_KEY_Y         0x31
#define C64_KEY_N         0x47
#define C64_KEY_F1        0x04
#define C64_KEY_F3        0x05
#define C64_KEY_F5        0x06
#define C64_KEY_F7        0x03
#define C64_KEY_LSHIFT    0x17
#define C64_KEY_CBM       0x75

#ifndef C64_KEYB_MASK_KEY
#define C64_KEYB_MASK_KEY(x) (1ULL << ((((x) >> 4) & 0xf) * 8 + ((x)&0xf)))
#endif

static inline uint64_t pocket_c64_process_shortcuts(uint32_t cont1_key, bool *suppress_joystick) {
    uint64_t mask = 0;
    bool up    = (cont1_key & (1 << 0)) != 0;
    bool down  = (cont1_key & (1 << 1)) != 0;
    bool left  = (cont1_key & (1 << 2)) != 0;
    bool right = (cont1_key & (1 << 3)) != 0;
    bool btn_a = (cont1_key & (1 << 4)) != 0;
    bool btn_b = (cont1_key & (1 << 5)) != 0;
    bool btn_x = (cont1_key & (1 << 6)) != 0;
    bool btn_y = (cont1_key & (1 << 7)) != 0;
    bool mod_l = (cont1_key & (1 << 8)) != 0;
    bool mod_r = (cont1_key & (1 << 9)) != 0;
    bool start = (cont1_key & (1 << 15)) != 0;

    if (mod_l) {
        if (suppress_joystick) *suppress_joystick = true;
        if (mod_r) {
            // L + R Layer: F2, F4, F6, F8, Space, N, Y, 2, Commodore (C=)
            if (up)    mask |= (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F1));
            if (right) mask |= (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F3));
            if (down)  mask |= (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F5));
            if (left)  mask |= (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F7));
            if (btn_a) mask |= C64_KEYB_MASK_KEY(C64_KEY_SPACE);
            if (btn_b) mask |= C64_KEYB_MASK_KEY(C64_KEY_N);
            if (btn_x) mask |= C64_KEYB_MASK_KEY(C64_KEY_Y);
            if (btn_y) mask |= C64_KEYB_MASK_KEY(C64_KEY_TWO);
            if (start) mask |= C64_KEYB_MASK_KEY(C64_KEY_CBM);
        } else {
            // L Layer: F1, F3, F5, F7, Space, Return, Run/Stop, 1
            if (up)    mask |= C64_KEYB_MASK_KEY(C64_KEY_F1);
            if (right) mask |= C64_KEYB_MASK_KEY(C64_KEY_F3);
            if (down)  mask |= C64_KEYB_MASK_KEY(C64_KEY_F5);
            if (left)  mask |= C64_KEYB_MASK_KEY(C64_KEY_F7);
            if (btn_a) mask |= C64_KEYB_MASK_KEY(C64_KEY_SPACE);
            if (btn_b) mask |= C64_KEYB_MASK_KEY(C64_KEY_RETURN);
            if (btn_x) mask |= C64_KEYB_MASK_KEY(C64_KEY_RUNSTOP);
            if (btn_y) mask |= C64_KEYB_MASK_KEY(C64_KEY_ONE);
        }
    } else {
        if (suppress_joystick) *suppress_joystick = false;
        // Unmodified: X -> Space, Y -> Return
        if (btn_x) mask |= C64_KEYB_MASK_KEY(C64_KEY_SPACE);
        if (btn_y) mask |= C64_KEYB_MASK_KEY(C64_KEY_RETURN);
    }

    return mask;
}

#endif // POCKET_C64_SHORTCUTS_H
