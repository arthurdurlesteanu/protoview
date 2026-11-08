/* Host test runner and the few app globals/UI hooks signal.c needs. */
#include "test_util.h"

const NotificationMessage message_green_255, message_green_0, message_red_255, message_red_0,
    message_vibro_on, message_vibro_off, message_delay_50, message_display_backlight_on;
RawSamplesBuffer *RawSamples, *DetectedSamples;

void adjust_raw_view_scale(ProtoViewApp* app, uint32_t short_pulse_dur) {
    (void)app;
    (void)short_pulse_dur;
}

int tests_run = 0, tests_failed = 0;

char* chips_from_rtl433_hex(const char* hex, uint32_t nchips) {
    char* s = calloc(nchips + 1, 1);
    for(uint32_t j = 0; j < nchips; j++) {
        char c = hex[j / 4];
        int v = (c >= '0' && c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10;
        s[j] = (v >> (3 - (j % 4))) & 1 ? '1' : '0';
    }
    return s;
}

/* Preamble AA AA AA + sync 59 (as chips), then Manchester "10"=1 "01"=0. */
char* chips_manchester_frame(const uint8_t* bytes, uint32_t len) {
    const char* hdr = "101010101010101010101010" "01011001";
    size_t hl = strlen(hdr);
    char* s = calloc(hl + len * 16 + 1, 1);
    memcpy(s, hdr, hl);
    for(uint32_t j = 0; j < len * 8; j++) {
        bool b = (bytes[j / 8] >> (7 - (j % 8))) & 1;
        s[hl + j * 2] = b ? '1' : '0';
        s[hl + j * 2 + 1] = b ? '0' : '1';
    }
    return s;
}

char* chips_pad(const char* chips, uint32_t lead, uint32_t trail) {
    size_t n = strlen(chips);
    char* s = malloc(lead + n + trail + 1);
    memset(s, '0', lead);
    memcpy(s + lead, chips, n);
    memset(s + lead + n, '0', trail);
    s[lead + n + trail] = 0;
    return s;
}

void chips_invert(char* chips) {
    for(; *chips; chips++) *chips = (*chips == '1') ? '0' : '1';
}

void info_reset(ProtoViewMsgInfo* info) {
    if(info->fieldset) fieldset_free(info->fieldset);
    memset(info, 0, sizeof(*info));
    info->fieldset = fieldset_new();
}

bool run_decoder(ProtoViewDecoder* dec, const char* chips, ProtoViewMsgInfo* info) {
    static uint8_t bits[BITMAP_BYTES];
    memset(bits, 0, sizeof(bits));
    bitmap_set_pattern(bits, sizeof(bits), 0, chips);
    info_reset(info);
    return dec->decode(bits, sizeof(bits), strlen(chips), info);
}

int main(void) {
    test_vw_suite();
    test_pipeline_suite();
    test_wake_suite();
    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
