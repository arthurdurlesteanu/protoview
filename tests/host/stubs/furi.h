#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#define UNUSED(x) (void)(x)
#define furi_assert(x) ((void)0)
/* Logging is swallowed, but arguments are still "used" (no unused warnings). */
static inline void furi_log_stub(const char* tag, const char* fmt, ...) {(void)tag;(void)fmt;}
#define FURI_LOG_E(t,...) furi_log_stub(t, __VA_ARGS__)
#define FURI_LOG_D(t,...) furi_log_stub(t, __VA_ARGS__)
#define FURI_LOG_I(t,...) furi_log_stub(t, __VA_ARGS__)
typedef struct FuriMutex FuriMutex; typedef struct FuriMessageQueue FuriMessageQueue;
typedef enum {FuriMutexTypeNormal} FuriMutexType;
#define FuriWaitForever 0xFFFFFFFFu
static inline FuriMutex* furi_mutex_alloc(FuriMutexType t){(void)t;return (FuriMutex*)1;}
static inline void furi_mutex_free(FuriMutex*m){(void)m;}
static inline int furi_mutex_acquire(FuriMutex*m,uint32_t w){(void)m;(void)w;return 0;}
static inline int furi_mutex_release(FuriMutex*m){(void)m;return 0;}
static inline uint32_t furi_get_tick(void){return 0;}
