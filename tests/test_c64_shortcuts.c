#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../src/bios/pocket_c64_shortcuts.h"

int main() {
    bool suppress = false;
    uint64_t mask = 0;

    printf("Testing C64 unmodified X -> Space, A -> Return, Y/B -> joystick only...\n");
    mask = pocket_c64_process_shortcuts(1 << 6, &suppress);
    assert(!suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_SPACE));

    mask = pocket_c64_process_shortcuts(1 << 4, &suppress);
    assert(!suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_RETURN));

    // Y is fire and B is jump; neither may inject a key
    mask = pocket_c64_process_shortcuts((1 << 7) | (1 << 5), &suppress);
    assert(!suppress);
    assert(mask == 0);

    printf("Testing C64 R alone (typed commands, no joystick/keys)...\n");
    mask = pocket_c64_process_shortcuts((1 << 9) | (1 << 4), &suppress);
    assert(suppress);
    assert(mask == 0);

    printf("Testing C64 L modifier...\n");
    // L held alone -> joystick suppressed, no keys
    mask = pocket_c64_process_shortcuts(1 << 8, &suppress);
    assert(suppress);
    assert(mask == 0);

    // L + A -> Space
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 4), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_SPACE));

    // L + B -> Return
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 5), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_RETURN));

    // L + X -> Run/Stop
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 6), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_RUNSTOP));

    // L + Y -> 1
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 7), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_ONE));

    // L + D-pad Up -> F1
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 0), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_F1));

    // L + D-pad Right -> F3
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 3), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_F3));

    // L + D-pad Down -> F5
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 1), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_F5));

    // L + D-pad Left -> F7
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 2), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_F7));

    printf("Testing C64 L + R modifier (F2, F4, F6, F8, 2, Y, N)...\n");
    // L + R + D-pad Up -> F2 (Shift + F1)
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 0), &suppress);
    assert(suppress);
    assert(mask == (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F1)));

    // L + R + D-pad Right -> F4 (Shift + F3)
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 3), &suppress);
    assert(suppress);
    assert(mask == (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F3)));

    // L + R + D-pad Down -> F6 (Shift + F5)
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 1), &suppress);
    assert(suppress);
    assert(mask == (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F5)));

    // L + R + D-pad Left -> F8 (Shift + F7)
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 2), &suppress);
    assert(suppress);
    assert(mask == (C64_KEYB_MASK_KEY(C64_KEY_LSHIFT) | C64_KEYB_MASK_KEY(C64_KEY_F7)));

    // L + R + Y -> 2
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 7), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_TWO));

    // L + R + X -> Y
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 6), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_Y));

    // L + R + B -> N
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 5), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_N));

    // L + R + Start -> C= (Commodore)
    mask = pocket_c64_process_shortcuts((1 << 8) | (1 << 9) | (1 << 15), &suppress);
    assert(suppress);
    assert(mask == C64_KEYB_MASK_KEY(C64_KEY_CBM));

    // No keys -> 0
    mask = pocket_c64_process_shortcuts(0, &suppress);
    assert(!suppress);
    assert(mask == 0);

    printf("All C64 shortcut tests passed!\n");
    return 0;
}
