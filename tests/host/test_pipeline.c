/* Full pipeline test: raw pulses -> scan_for_signal() -> decoded message. */
#include "test_util.h"

/* Feed a chip string to 'buf' as run-length pulses of 'chip_us' per chip,
 * with an optional deterministic +/- jitter on each pulse. */
static void feed_chips(RawSamplesBuffer* buf, const char* chips, uint32_t chip_us, int jitter) {
    size_t n = strlen(chips);
    int sign = 1;
    for(size_t j = 0; j < n;) {
        size_t run = 1;
        while(j + run < n && chips[j + run] == chips[j]) run++;
        int dur = (int)(run * chip_us) + sign * jitter;
        sign = -sign;
        raw_samples_add(buf, chips[j] == '1', (uint32_t)dur);
        j += run;
    }
}

static ProtoViewApp* app_new(void) {
    ProtoViewApp* app = calloc(1, sizeof(ProtoViewApp));
    RawSamples = raw_samples_alloc();
    DetectedSamples = raw_samples_alloc();
    return app;
}

static void app_free(ProtoViewApp* app) {
    free_msg_info(app->msg_info);
    raw_samples_free(RawSamples);
    raw_samples_free(DetectedSamples);
    free(app);
}

static char* vw_chips(const char* hex, bool invert) {
    uint8_t b[11];
    for(int j = 0; j < 11; j++) {
        unsigned v;
        sscanf(hex + j * 2, "%2x", &v);
        b[j] = (uint8_t)v;
    }
    char* frame = chips_manchester_frame(b, 11);
    if(invert) chips_invert(frame);
    return frame;
}

static void run_case(const char* label, bool invert, int jitter, uint32_t min_dur, bool expect) {
    ProtoViewApp* app = app_new();
    char* frame = vw_chips("8822a555bd6442e604c58c", invert);

    /* Long idle gap (in the opposite level of the first chip), the
     * frame, then another idle gap so the coherent run is bounded. */
    raw_samples_add(RawSamples, frame[0] == '1' ? false : true, 20000);
    feed_chips(RawSamples, frame, 25, jitter);
    raw_samples_add(RawSamples, frame[strlen(frame) - 1] == '1' ? false : true, 20000);

    scan_for_signal(app, RawSamples, min_dur);

    bool is_vw = app->signal_decoded && app->msg_info && app->msg_info->decoder &&
                 strcmp(app->msg_info->decoder->name, "VW TPMS") == 0;
    if(!expect) {
        CHECK(!is_vw, "%s: unexpected VW decode", label);
        free(frame);
        app_free(app);
        return;
    }
    CHECK(is_vw, "%s: displayed message is not a decoded VW frame", label);
    if(is_vw) {
        ProtoViewFieldSet* fs = app->msg_info->fieldset;
        char buf[32];
        CHECK(fs->numfields == 5, "%s: numfields %u", label, (unsigned)fs->numfields);
        field_to_string(buf, sizeof(buf), fs->fields[0]);
        CHECK(strcmp(buf, "22A555BD") == 0, "%s: id '%s'", label, buf);
        field_to_string(buf, sizeof(buf), fs->fields[1]);
        CHECK(strcmp(buf, "245.00") == 0, "%s: pressure '%s'", label, buf);
        field_to_string(buf, sizeof(buf), fs->fields[2]);
        CHECK(strcmp(buf, "14") == 0, "%s: temp '%s'", label, buf);
    }
    free(frame);
    app_free(app);
}

void test_pipeline_suite(void) {
    int before = tests_failed;
    /* min_duration 15 is the "FSK 40kBaud" preset duration filter. */
    run_case("normal", false, 0, 15, true);
    run_case("inverted", true, 0, 15, true);
    run_case("normal jitter 3us", false, 3, 15, true);
    run_case("inverted jitter 4us", true, 4, 15, true);
    /* With the 30us filter of the other presets (e.g. "TPMS 1"), 25us
     * chips are rejected: documents why the README asks for "FSK 40kBaud". */
    run_case("30us filter", false, 0, 30, false);
    printf("pipeline suite: %s\n", tests_failed == before ? "ok" : "FAILED");
}
