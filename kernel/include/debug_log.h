#pragma once
#include <stdint.h>

void init_global_debug_log(uint64_t x_start, uint64_t y_start);

void db_print_string(char* str);
void db_print_char(char c);

void db_print_hex(uint64_t value);
void db_print_hex_len(uint64_t value, uint64_t len);

void db_print_binary(uint64_t value);

void db_print_int(uint64_t value);

// format using {0b}/{0x}/{0d} to print values as binary/hex/decimal
void db_print_format(char* str, uint64_t values);
void db_newln();

void db_println_string(char* str);
void db_println_char(uint64_t c);

void db_println_hex(uint64_t value);

void db_println_hex_len(uint64_t value, uint64_t len);

void db_println_binary(uint64_t value);
void db_println_int(uint64_t value);

void db_println_format(char* str, uint64_t values);
