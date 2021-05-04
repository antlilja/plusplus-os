#include "rendering.h"
#include "ps2.h"
#include "pci.h"
#include "port_io.h"

#include <stdint.h>
#include <string.h>

#define PS2_KC_A 0x1C
#define PS2_KC_B 0x32
#define PS2_KC_C 0x21
#define PS2_KC_D 0x23
#define PS2_KC_E 0x24
#define PS2_KC_F 0x2B
#define PS2_KC_G 0x34
#define PS2_KC_H 0x33
#define PS2_KC_I 0x43
#define PS2_KC_J 0x3B
#define PS2_KC_K 0x42
#define PS2_KC_L 0x4B
#define PS2_KC_M 0x3A
#define PS2_KC_N 0x31
#define PS2_KC_O 0x44
#define PS2_KC_P 0x4D
#define PS2_KC_Q 0x15
#define PS2_KC_R 0x2D
#define PS2_KC_S 0x1B
#define PS2_KC_T 0x2C
#define PS2_KC_U 0x3C
#define PS2_KC_V 0x2A
#define PS2_KC_W 0x1D
#define PS2_KC_X 0x22
#define PS2_KC_Y 0x35
#define PS2_KC_Z 0x1A

void Ps2Init() {
    while (1) {
        uint8_t key_code = port_in_u8(0x60);

        put_hex_32(key_code & 0x7F, 10, 14);
        switch (key_code) {
            case PS2_KC_A: put_hex_32("A", 10, 16);
            case PS2_KC_C: put_hex_32("C", 10, 16);
        }
    }
}
