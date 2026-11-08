#pragma once
#include <furi.h>
typedef int FuriHalSubGhzPreset; typedef void* (*FuriHalSubGhzAsyncTxCallback)(void*);
/* LF RFID HAL: defined (with call recording) in test_wake.c. */
void furi_hal_rfid_tim_read_start(float freq, float duty_cycle);
void furi_hal_rfid_tim_read_stop(void);
void furi_hal_rfid_pins_reset(void);
