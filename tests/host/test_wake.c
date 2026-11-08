/* TPMS wake (wake.c) tests, with recording stubs for the LF RFID HAL and
 * FuriTimer, plus source checks that app.c wires the stop guarantees. */
#include "test_util.h"

/* ---------------------------- Recording stubs ---------------------------- */

static struct {
    int rfid_start, rfid_stop, pins_reset;
    float last_freq, last_duty;
    int hal_calls_in_cb; /* HAL calls made while the timer callback ran. */
    bool in_cb;
} hal;

struct FuriTimer {
    FuriTimerCallback cb;
    void* ctx;
    FuriTimerType type;
    bool running;
    int starts, stops;
    uint32_t last_ticks;
};
static int timers_alive;

static void hal_touch(void) {
    if(hal.in_cb) hal.hal_calls_in_cb++;
}
void furi_hal_rfid_tim_read_start(float freq, float duty_cycle) {
    hal_touch();
    hal.rfid_start++;
    hal.last_freq = freq;
    hal.last_duty = duty_cycle;
}
void furi_hal_rfid_tim_read_stop(void) {
    hal_touch();
    hal.rfid_stop++;
}
void furi_hal_rfid_pins_reset(void) {
    hal_touch();
    hal.pins_reset++;
}

FuriTimer* furi_timer_alloc(FuriTimerCallback cb, FuriTimerType type, void* ctx) {
    FuriTimer* t = calloc(1, sizeof(*t));
    t->cb = cb;
    t->type = type;
    t->ctx = ctx;
    timers_alive++;
    return t;
}
void furi_timer_free(FuriTimer* t) {
    timers_alive--;
    free(t);
}
FuriStatus furi_timer_start(FuriTimer* t, uint32_t ticks) {
    t->running = true;
    t->starts++;
    t->last_ticks = ticks;
    return FuriStatusOk;
}
FuriStatus furi_timer_stop(FuriTimer* t) {
    t->running = false;
    t->stops++;
    return FuriStatusOk;
}
const NotificationSequence sequence_single_vibro = {NULL};

/* Simulate the timer service firing the one-shot timer. */
static void fire_timer(FuriTimer* t) {
    t->running = false;
    hal.in_cb = true;
    t->cb(t->ctx);
    hal.in_cb = false;
}

static ProtoViewApp* wake_app_new(void) {
    memset(&hal, 0, sizeof(hal));
    ProtoViewApp* app = calloc(1, sizeof(ProtoViewApp));
    wake_init(app);
    return app;
}

static void wake_app_free(ProtoViewApp* app) {
    wake_deinit(app);
    free(app);
}

/* ------------------------------ Behaviour ------------------------------- */

static void test_start_then_expiry_stops(void) {
    ProtoViewApp* app = wake_app_new();
    FuriTimer* t = app->wake_timer;
    CHECK(t && t->type == FuriTimerTypeOnce, "one-shot timer allocated");
    CHECK(!app->wake_active, "inactive after init");

    CHECK(wake_start(app), "start succeeds");
    CHECK(app->wake_active, "active after start");
    CHECK(hal.rfid_start == 1, "carrier started once (%d)", hal.rfid_start);
    CHECK(hal.last_freq == 125000.0f, "freq %f", hal.last_freq);
    CHECK(hal.last_duty == 0.5f, "duty %f", hal.last_duty);
    CHECK(t->running && t->starts == 1, "timer armed");
    CHECK(t->last_ticks + WAKE_POLL_SLACK_MS <= WAKE_MAX_MS, "burst capped at %d ms", WAKE_MAX_MS);
    CHECK(t->last_ticks >= 2500, "burst not too short (%u)", (unsigned)t->last_ticks);

    wake_poll(app); /* Not expired yet: nothing happens. */
    CHECK(app->wake_active && hal.rfid_stop == 0, "poll before expiry is a no-op");

    fire_timer(t);
    CHECK(hal.hal_calls_in_cb == 0, "timer callback makes no HAL calls");
    CHECK(app->wake_expired, "callback flags expiry");
    wake_poll(app);
    CHECK(!app->wake_active, "inactive after expiry + poll");
    CHECK(hal.rfid_stop == 1 && hal.pins_reset == 1, "carrier stopped and pins reset");
    CHECK(!app->wake_expired, "expiry flag consumed");

    /* A new key press starts a fresh burst. */
    CHECK(wake_start(app) && hal.rfid_start == 2 && t->starts == 2, "restart after expiry");
    wake_app_free(app);
    CHECK(hal.rfid_stop == 2, "deinit stops restarted burst");
}

static void test_double_start(void) {
    ProtoViewApp* app = wake_app_new();
    CHECK(wake_start(app), "first start");
    CHECK(!wake_start(app), "second start refused");
    CHECK(hal.rfid_start == 1, "carrier started once (%d)", hal.rfid_start);
    CHECK(app->wake_timer->starts == 1, "timer started once");
    wake_app_free(app);
}

static void test_stop_idempotent(void) {
    ProtoViewApp* app = wake_app_new();
    wake_stop(app);
    CHECK(hal.rfid_stop == 0 && hal.pins_reset == 0, "stop while idle is a no-op");
    wake_start(app);
    wake_stop(app);
    wake_stop(app);
    CHECK(hal.rfid_stop == 1 && hal.pins_reset == 1, "double stop stops once");
    CHECK(!app->wake_timer->running, "timer cancelled by stop");
    wake_poll(app);
    CHECK(hal.rfid_stop == 1, "poll after stop is a no-op");
    wake_app_free(app);
}

static void test_exit_while_active(void) {
    ProtoViewApp* app = wake_app_new();
    int alive = timers_alive;
    wake_start(app);
    wake_deinit(app);
    CHECK(!app->wake_active, "inactive after deinit");
    CHECK(hal.rfid_stop == 1 && hal.pins_reset == 1, "deinit stops carrier");
    CHECK(app->wake_timer == NULL && timers_alive == alive - 1, "timer freed");
    wake_deinit(app); /* Second exit path call is harmless. */
    CHECK(hal.rfid_stop == 1, "deinit idempotent");
    CHECK(!wake_start(app) && hal.rfid_start == 1, "no start without timer");
    free(app);
}

static void test_view_change_while_active(void) {
    /* app_switch_view() calls wake_stop() unconditionally (checked below). */
    ProtoViewApp* app = wake_app_new();
    wake_start(app);
    wake_stop(app);
    CHECK(!app->wake_active && hal.rfid_stop == 1, "view change stops carrier");
    fire_timer(app->wake_timer); /* Late expiry must not re-stop. */
    wake_poll(app);
    CHECK(hal.rfid_stop == 1, "late expiry after stop is harmless");
    wake_app_free(app);
}

/* ----------------------------- Source wiring ----------------------------- */

static char* slurp(const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* s = calloc(n + 1, 1);
    if(fread(s, 1, n, f) != (size_t)n) n = 0;
    fclose(f);
    return s;
}

/* Return a copy of the body (from its opening brace up to the first
 * column-0 closing brace) of the function whose definition starts with
 * 'sig'. Caller frees. */
static char* func_body(const char* src, const char* sig) {
    const char* a = src ? strstr(src, sig) : NULL;
    if(a) a = strchr(a, '{'); /* Skip the signature itself. */
    /* Closing brace at column 0; tolerate LF and CRLF sources. */
    const char* b = a;
    while(b && (b = strstr(b + 1, "\n}")) && b[2] != '\n' && b[2] != '\r') {
    }
    if(!b) return NULL;
    char* s = calloc(b - a + 1, 1);
    memcpy(s, a, b - a);
    return s;
}

static bool before(const char* s, const char* first, const char* second) {
    const char* a = s ? strstr(s, first) : NULL;
    const char* b = s ? strstr(s, second) : NULL;
    return a && b && a < b;
}

static void test_wiring(void) {
    char* app_c = slurp(APP_ROOT "/app.c");
    char* raw_c = slurp(APP_ROOT "/view_raw_signal.c");
    CHECK(app_c && raw_c, "sources readable");

    char* sw = func_body(app_c, "static void app_switch_view(");
    CHECK(before(sw, "wake_stop(app)", "app->current_view = switchto"), "view switch stops wake");
    char* fr = func_body(app_c, "void protoview_app_free(");
    CHECK(before(fr, "wake_deinit(app)", "radio_sleep(app)"), "free: wake off first");
    CHECK(before(fr, "wake_deinit(app)", "free("), "free: wake off before any free");
    char* al = func_body(app_c, "ProtoViewApp* protoview_app_alloc(");
    CHECK(al && strstr(al, "wake_init(app)"), "alloc initialises wake");
    char* en = func_body(app_c, "int32_t protoview_app_entry(");
    CHECK(before(en, "WAKE_POLL_SLACK_MS", "wake_poll(app)"), "queue timeout is the poll slack");
    CHECK(before(en, "wake_poll(app)", "if (qstat == FuriStatusOk)"), "main loop polls expiry");
    CHECK(before(en, "wake_stop(app)", "radio_rx_end(app)"), "exit: carrier off before radio");
    char* in = func_body(raw_c, "void process_input_raw_pulses(");
    CHECK(before(in, "InputTypeLong", "wake_start(app)"), "raw view: long press starts wake");
    CHECK(before(in, "input.key == InputKeyUp", "wake_start(app)"), "raw view: on the UP key");
    CHECK(before(in, "InputTypeRepeat", "InputTypeLong"), "raw view: panning stays on repeat");

    free(sw);
    free(fr);
    free(al);
    free(en);
    free(in);
    free(app_c);
    free(raw_c);
}

void test_wake_suite(void) {
    int before_failed = tests_failed;
    test_start_then_expiry_stops();
    test_double_start();
    test_stop_idempotent();
    test_exit_while_active();
    test_view_change_while_active();
    test_wiring();
    printf("wake suite: %s\n", tests_failed == before_failed ? "ok" : "FAILED");
}
