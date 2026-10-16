#include "bios.h"

// Wait for the host to answer a bridge command ("ok" in TARGET_0[31:16]). A slot
// whose file the host cannot deliver never answers; give up after ~3 s instead
// of hanging the BIOS (and with it the menus and the keyboard) for good.
static int bridge_wait_ok(void) {
  for (uint32_t n = 0; n < (1u << 26); n++) {
    if ((*TARGET_0 >> 16) == 0x6F6B)
      return 1;
  }
  return 0;
}

uint32_t bridge_ds_get_length(uint16_t slot_id) {
  volatile uint32_t *p = BRIDGE_DS_TABLE;

  for (unsigned idx = 0; idx < 32; idx++) {
    uint16_t ds_slot_id = p[idx * 2 + 0] & 0xffff;
    uint32_t ds_length = p[idx * 2 + 1];
    if (ds_slot_id == slot_id)
      return ds_length;
  }
  return 0;
}

uint16_t bridge_ds_get_uint16(uint16_t slot_id, uint32_t offset) {
  volatile uint16_t *p = (volatile uint16_t *)0x70000000;
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = offset;  // slot-offset
  *TARGET_28 = (uint32_t)p;
  *TARGET_2C = 2; // length
  *TARGET_0 = 0x636D0180;
  if (!bridge_wait_ok())
    return 0;
  return *p;
}

uint32_t bridge_ds_get_uint32(uint16_t slot_id, uint32_t offset) {
  volatile uint32_t *p = (volatile uint32_t *)0x70000000;
  *TARGET_20 = slot_id; // slot-id
  *TARGET_24 = offset;  // slot-offset
  *TARGET_28 = (uint32_t)p;
  *TARGET_2C = 4; // length
  *TARGET_0 = 0x636D0180;
  if (!bridge_wait_ok())
    return 0;
  return *p;
}

void bridge_ds_read(uint16_t slot_id, uint32_t offset, uint32_t length,
                    uint8_t *dst) {
  const uint32_t buf_size = BRIDGE_DPRAM_SIZE;

  while (length > 0) {
    uint32_t chunk_size = MIN(length, buf_size);
    *TARGET_20 = slot_id;
    *TARGET_24 = offset;
    *TARGET_28 = 0x70000000;
    *TARGET_2C = chunk_size;
    *TARGET_0 = 0x636D0180;
    if (!bridge_wait_ok())
      return;

    volatile uint8_t *p = (volatile uint8_t *)0x70000000;
    for (uint32_t idx = 0; idx < chunk_size; idx++) {
      *dst++ = p[idx];
    }

    length -= chunk_size;
    offset += chunk_size;
  }
}
