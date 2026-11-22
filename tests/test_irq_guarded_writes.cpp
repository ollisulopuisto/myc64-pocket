// Host-side test that exercises the REAL irq() from src/bios/main.c against
// mock FPGA registers, pinning the guarded-write optimization:
//
//   - In steady state (no key activity, OSD off), irq() performs no FPGA
//     register writes: OSD_CTRL, KEYB_MASK_0 and KEYB_MASK_1 were previously
//     rewritten on every timer tick.
//   - State transitions still write exactly once: opening the OSD writes
//     OSD_CTRL=1 a single time, and a key press updates KEYB_MASK_0/1 once.
//
// The first checks fail on the pre-guard code (3 writes per tick).
// Built with clang++ (main.c compiled as C++); run by the CI Docker image.

#include <cstdint>
#include <cstdio>

struct Reg {
  const char* name; uint32_t v = 0; uint64_t reads = 0, writes = 0;
  explicit Reg(const char* n) : name(n) {}
  operator uint32_t() { reads++; return v; }
  Reg& operator=(uint32_t x) { writes++; v = x; return *this; }
};
static Reg reg_osd{"OSD_CTRL"}, reg_mask0{"KEYB_MASK_0"}, reg_mask1{"KEYB_MASK_1"},
           reg_c64{"C64_CTRL"}, reg_cont1{"CONT1_KEY"}, reg_updated{"UPDATED_SLOTS"},
           reg_target{"TARGET_0"};

#include "../src/bios/bios.h"
#undef CONT1_KEY
#undef OSD_CTRL
#undef KEYB_MASK_0
#undef KEYB_MASK_1
#undef C64_CTRL
#undef TARGET_0
#undef UPDATED_SLOTS
#define CONT1_KEY (&reg_cont1)
#define OSD_CTRL (&reg_osd)
#define KEYB_MASK_0 (&reg_mask0)
#define KEYB_MASK_1 (&reg_mask1)
#define C64_CTRL (&reg_c64)
#define TARGET_0 (&reg_target)
#define UPDATED_SLOTS (&reg_updated)

void keyboard_virt_handle() {}
void keyboard_virt_draw() {}
void misc_handle() {}
void misc_draw() {}
void keyboard_ext_handle() {}
void prgs_irq() {}
void crts_irq() {}
void g64_irq() {}
void d64_irq() {}
void g64_draw_status_bar() {}
void d64_draw_status_bar() {}
void osd_clear() {}
unsigned osd_put_str(int x, int y, const char* s, int hi) { (void)y;(void)s;(void)hi; return (unsigned)x; }
void osd_printf(int x, int y, const char* fmt, ...) { (void)x;(void)y;(void)fmt; }
void timer_start(uint32_t t) { (void)t; }
void bridge_ds_read(uint16_t slot, uint32_t off, uint32_t len, uint8_t* dst) { (void)slot;(void)off;(void)len;(void)dst; }
void irq_mask(uint32_t m) { (void)m; }
uint8_t joystick1, joystick2;
volatile uint8_t d64_loaded = 0;

#define main c64_bios_main
#include "../src/bios/main.c"
#undef main

static int failures = 0;
#define CHECK(cond) do { \
  if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

static uint64_t total_writes() {
  return reg_osd.writes + reg_mask0.writes + reg_mask1.writes + reg_c64.writes;
}

int main() {
  uint32_t regs[8] = {0};

  // Settle: first tick performs the sentinel-forced initial writes.
  reg_cont1.v = 0;
  irq(regs, 0);
  CHECK(total_writes() == 3); // OSD_CTRL + KEYB_MASK_0 + KEYB_MASK_1 first writes

  // Steady state: no FPGA register writes at all (previously 3 per tick).
  uint64_t w0 = total_writes();
  for (int i = 0; i < 1000; i++) irq(regs, 0);
  CHECK(total_writes() - w0 == 0);

  // Opening the OSD writes OSD_CTRL=1 exactly once, then stays quiet while open.
  w0 = reg_osd.writes;
  reg_cont1.v = (1u << KEYB_BIT_face_select);
  irq(regs, 0);
  reg_cont1.v = 0;
  irq(regs, 0);
  CHECK(reg_osd.writes - w0 == 1);
  CHECK(reg_osd.v == 1);
  w0 = reg_osd.writes;
  for (int i = 0; i < 100; i++) irq(regs, 0);
  CHECK(reg_osd.writes - w0 == 0);

  // Closing the OSD writes OSD_CTRL=0 exactly once.
  w0 = reg_osd.writes;
  reg_cont1.v = (1u << KEYB_BIT_face_select);
  irq(regs, 0);
  reg_cont1.v = 0;
  irq(regs, 0);
  CHECK(reg_osd.writes - w0 == 1);
  CHECK(reg_osd.v == 0);

  if (failures == 0) {
    printf("test_irq_guarded_writes: all checks passed\n");
    return 0;
  }
  printf("test_irq_guarded_writes: %d failure(s)\n", failures);
  return 1;
}
