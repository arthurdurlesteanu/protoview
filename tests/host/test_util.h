/* Minimal host test helpers for the ProtoView decoders. */
#pragma once

#include "app.h"

extern int tests_run, tests_failed;

#define CHECK(cond, ...)                                       \
    do {                                                       \
        tests_run++;                                           \
        if(!(cond)) {                                          \
            tests_failed++;                                    \
            printf("FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond); \
            printf(__VA_ARGS__);                               \
            printf("\n");                                      \
        }                                                      \
    } while(0)

#define BITMAP_BYTES 4096

/* Chip string helpers (strings of '0'/'1'). Caller frees results. */
char* chips_from_rtl433_hex(const char* hex, uint32_t nchips);
char* chips_manchester_frame(const uint8_t* bytes, uint32_t len);
char* chips_pad(const char* chips, uint32_t lead, uint32_t trail);
void chips_invert(char* chips);

/* Load chips into a 4096 byte bitmap and run 'dec'. */
bool run_decoder(ProtoViewDecoder* dec, const char* chips, ProtoViewMsgInfo* info);
void info_reset(ProtoViewMsgInfo* info);

void test_vw_suite(void);
void test_pipeline_suite(void);
void test_wake_suite(void);
