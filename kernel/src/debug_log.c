#include <stdint.h>
#include "rendering.h"

typedef struct {
    uint64_t x_start;
    uint64_t y_start;
    uint64_t x_head;
    uint64_t y_head;
} DebugLog;

DebugLog g_debug_log;

void init_global_debug_log(uint64_t x_start, uint64_t y_start) {
    g_debug_log.x_start = x_start;
    g_debug_log.y_start = y_start;
    g_debug_log.x_head = x_start;
    g_debug_log.y_head = y_start;
}

void db_print_string(char* str) {
    g_debug_log.x_head += put_string(str, g_debug_log.x_head, g_debug_log.y_head);
}

void db_print_hex(uint64_t value) {
    g_debug_log.x_head += put_hex(value, g_debug_log.x_head, g_debug_log.y_head);
}

void db_print_hex_len(uint64_t value, uint64_t len) {
    put_hex_len(value, g_debug_log.x_head, g_debug_log.y_head, len);
    g_debug_log.y_head += len;
}

void db_print_binary(uint64_t value) {
    g_debug_log.x_head += put_binary(value, g_debug_log.x_head, g_debug_log.y_head);
}

void db_print_int(uint64_t value) {
    g_debug_log.x_head += put_int(value, g_debug_log.x_head, g_debug_log.y_head);
}

// format using {0b}/{0x}/{0d} to print values as binary/hex/decimal
void db_print_format(char* str, uint64_t* values) {
    uint32_t i = 0;
    uint32_t j = 0;
    char c = str[i];
    while (c) {
        if (c == '{') {
            if (str[i + 2] == 'b') {
                db_print_binary(values[j]);
            }
            else if (str[i + 2] == 'x') {
                db_print_hex(values[j]);
            }
            else if (str[i + 2] == 'd') {
                db_print_int(values[j]);
            }
            j++;
            i += 3;
        }
        else {
            char temp[2] = {c, 0}; // THERE HAS TO BE A BETTER WAY!!
            db_print_string(temp);
        }
        i++;
        c = str[i];
    }
}

void db_newln() {
    g_debug_log.y_head++;
    g_debug_log.x_head = g_debug_log.x_start;
}

void db_println_string(char* str) {
    db_print_string(str);
    db_newln();
}

void db_println_hex(uint64_t value) {
    db_print_hex(value);
    db_newln();
}

void db_println_hex_len(uint64_t value, uint64_t len) {
    db_print_hex_len(value, len);
    db_newln();
}

void db_println_binary(uint64_t value) {
    db_print_binary(value);
    db_newln();
}

void db_println_int(uint64_t value) {
    db_print_int(value);
    db_newln();
}

void db_println_format(char* str, uint64_t* values) {
    db_print_format(str, values);
    db_newln();
}
