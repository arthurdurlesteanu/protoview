/* Direct decoder tests for the VW / Audi (Huf) TPMS decoder. */
#include "test_util.h"
#include <math.h>

extern ProtoViewDecoder VWTPMSDecoder;
extern ProtoViewDecoder RenaultTPMSDecoder;
extern ProtoViewDecoder UnknownDecoder;
extern ProtoViewDecoder* Decoders[];

typedef struct {
    const char* name;
    const char* raw_hex; /* rtl_433 chip string, or NULL. */
    uint32_t nchips;
    const char* bytes_hex; /* Frame bytes (used when raw_hex is NULL). */
    const char* id;
    float kpa;
    int temp;
    unsigned brand;
    bool alert;
} VWVector;

static const VWVector vectors[] = {
    {"A raw", "aaaaaa5995955969aa556996aaa5556665aaa69a5565955a55aaff8", 217, NULL,
     "26F069FC", 12.25f, 27, 0x88, false},
    {"B bytes", NULL, 0, "8822a555bd6442e604c58c", "22A555BD", 245.0f, 14, 0x88, false},
    {"C raw", "2aaaaa5955555955a5556a65666a56aa65a65999fc", 166, NULL, "20C07457", 75.95f,
     25, 0x00, true},
    {"C bytes 1", NULL, 0, "0020c07457014da2", "20C07457", 2.45f, 25, 0x00, true},
    {"C bytes 2", NULL, 0, "0020c07457134c55", "20C07457", 46.55f, 24, 0x00, true},
    {"D raw 1", "555554b2aab4b2b552acb4d332accb32b552aaacd334d32ad334", 207, NULL, "23E136A1",
     181.3f, 10, 0x03, false},
    {"D raw 2", "555554b2aab4b2b552acb4d332acb4cab54caaacd4cad32b4b55e", 211, NULL,
     "23E136A1", 127.4f, 9, 0x03, false},
    {"E bytes", NULL, 0, "8036bcdadc44462601a24b", "36BCDADC", 166.6f, 18, 0x80, false},
};
#define NUM_VECTORS (sizeof(vectors) / sizeof(vectors[0]))

static uint32_t hex_to_bytes(const char* hex, uint8_t* out) {
    uint32_t n = strlen(hex) / 2;
    for(uint32_t j = 0; j < n; j++) {
        unsigned v;
        sscanf(hex + j * 2, "%2x", &v);
        out[j] = (uint8_t)v;
    }
    return n;
}

/* Bare chips for a vector (no idle padding). */
static char* vector_chips(const VWVector* v) {
    if(v->raw_hex) return chips_from_rtl433_hex(v->raw_hex, v->nchips);
    uint8_t b[16];
    uint32_t n = hex_to_bytes(v->bytes_hex, b);
    return chips_manchester_frame(b, n);
}

static void check_fields(const VWVector* v, ProtoViewMsgInfo* info, const char* pol) {
    ProtoViewFieldSet* fs = info->fieldset;
    CHECK(fs->numfields >= 4, "%s/%s numfields=%u", v->name, pol, (unsigned)fs->numfields);
    if(fs->numfields < 4) return;
    char buf[32];
    field_to_string(buf, sizeof(buf), fs->fields[0]);
    CHECK(strcmp(fs->fields[0]->name, "Tire ID") == 0, "%s field0 name", v->name);
    CHECK(strcmp(buf, v->id) == 0, "%s/%s id %s != %s", v->name, pol, buf, v->id);
    CHECK(fs->fields[1]->type == FieldTypeFloat, "%s pressure must be float", v->name);
    CHECK(
        fabs((double)(fs->fields[1]->fvalue - v->kpa)) < 0.01,
        "%s/%s kpa %.2f != %.2f",
        v->name,
        pol,
        (double)fs->fields[1]->fvalue,
        (double)v->kpa);
    CHECK(
        fs->fields[2]->value == v->temp,
        "%s/%s temp %lld != %d",
        v->name,
        pol,
        (long long)fs->fields[2]->value,
        v->temp);
    CHECK(
        strcmp(fs->fields[3]->name, "Brand") == 0 && fs->fields[3]->uvalue == v->brand,
        "%s/%s brand %llx",
        v->name,
        pol,
        (unsigned long long)fs->fields[3]->uvalue);
    CHECK(fs->numfields == 5, "%s numfields %u", v->name, (unsigned)fs->numfields);
    if(fs->numfields == 5) {
        const char* want = v->alert ? "Alert" : "Flags";
        CHECK(strcmp(fs->fields[4]->name, want) == 0, "%s field4 %s", v->name, fs->fields[4]->name);
    }
}

static void test_vectors(void) {
    ProtoViewMsgInfo info = {0};
    for(uint32_t k = 0; k < NUM_VECTORS; k++) {
        const VWVector* v = &vectors[k];
        char* bare = vector_chips(v);
        char* chips = chips_pad(bare, 40, 40);
        bool ok = run_decoder(&VWTPMSDecoder, chips, &info);
        CHECK(ok, "%s normal polarity not decoded", v->name);
        if(ok) {
            check_fields(v, &info, "normal");
            CHECK(info.pulses_count > 0, "%s pulses_count", v->name);
            CHECK(info.start_off + info.pulses_count <= strlen(chips), "%s bounds", v->name);
        }

        chips_invert(chips);
        ok = run_decoder(&VWTPMSDecoder, chips, &info);
        CHECK(ok, "%s inverted polarity not decoded", v->name);
        if(ok) check_fields(v, &info, "inverted");

        /* Other decoders (excluding the generic Unknown) must not claim it. */
        chips_invert(chips);
        for(int d = 0; Decoders[d]; d++) {
            if(Decoders[d] == &VWTPMSDecoder || Decoders[d] == &UnknownDecoder) continue;
            bool other = run_decoder(Decoders[d], chips, &info);
            CHECK(!other, "%s claimed by %s", v->name, Decoders[d]->name);
        }
        free(chips);
        free(bare);
    }
    info_reset(&info);
    fieldset_free(info.fieldset);
}

/* Decoder order: VW must be registered, before the Unknown decoder (last). */
static void test_registration(void) {
    int vw = -1, unk = -1, n = 0;
    for(; Decoders[n]; n++) {
        if(Decoders[n] == &VWTPMSDecoder) vw = n;
        if(Decoders[n] == &UnknownDecoder) unk = n;
    }
    CHECK(vw >= 0, "VW decoder not registered");
    CHECK(unk == n - 1, "Unknown decoder must be last");
    CHECK(vw < unk, "VW must precede Unknown");
}

static void expect_no_vw(const char* what, const char* bare) {
    ProtoViewMsgInfo info = {0};
    char* chips = chips_pad(bare, 40, 40);
    CHECK(!run_decoder(&VWTPMSDecoder, chips, &info), "%s decoded as VW (normal)", what);
    chips_invert(chips);
    CHECK(!run_decoder(&VWTPMSDecoder, chips, &info), "%s decoded as VW (inverted)", what);
    free(chips);
    fieldset_free(info.fieldset);
}

static void test_negatives(void) {
    uint8_t b[11];
    hex_to_bytes("8822a555bd6442e604c58c", b);

    /* One flipped payload bit in each byte: CRC must reject it. */
    for(int byte = 0; byte < 11; byte++) {
        uint8_t m[11];
        memcpy(m, b, sizeof(m));
        m[byte] ^= 0x10;
        char* c = chips_manchester_frame(m, 11);
        char what[32];
        snprintf(what, sizeof(what), "flipped bit in b%d", byte);
        expect_no_vw(what, c);
        free(c);
    }

    /* Truncated frames: lose the tail of the 11 byte frame. */
    char* full = chips_manchester_frame(b, 11);
    size_t n = strlen(full);
    for(size_t cut = 2; cut <= 80; cut += 13) {
        full[n - cut] = 0;
        expect_no_vw("truncated frame", full);
    }
    free(full);

    /* Header only. */
    expect_no_vw("preamble+sync only", "10101010101010101010101001011001");

    /* Random noise. */
    srand(1234);
    char* noise = malloc(3001);
    for(int t = 0; t < 300; t++) {
        for(int j = 0; j < 3000; j++) noise[j] = (rand() & 1) ? '1' : '0';
        noise[3000] = 0;
        expect_no_vw("random noise", noise);
    }
    free(noise);

    /* A valid Renault frame (built like the Renault encoder does). */
    uint8_t r[9] = {0x6D, 0x45, 52, 0x7A, 0xD7, 0x79, 0xFF, 0xFF, 0};
    r[8] = crc8(r, 8, 0, 7);
    char rc[400];
    strcpy(rc, "01010101010101010101010101010110");
    size_t rl = strlen(rc);
    for(int j = 0; j < 72; j++) {
        bool bit = bitmap_get(r, 9, j);
        rc[rl++] = bit ? '1' : '0';
        rc[rl++] = bit ? '0' : '1';
    }
    rc[rl] = 0;
    ProtoViewMsgInfo info = {0};
    char* padded = chips_pad(rc, 40, 40);
    CHECK(run_decoder(&RenaultTPMSDecoder, padded, &info), "Renault sanity frame not decoded");
    free(padded);
    fieldset_free(info.fieldset);
    expect_no_vw("Renault frame", rc);

    /* A short frame whose b0 is not 0x00 must be rejected even with valid CRC. */
    uint8_t s[8];
    hex_to_bytes("0120c07457014d00", s);
    s[7] = crc8(s, 7, 0xAA, 0x2F);
    char* sc = chips_manchester_frame(s, 8);
    expect_no_vw("short frame with b0 != 0", sc);
    free(sc);
}

/* Preamble with a leading partial chip / glitch before the sync, and two
 * back to back frames where the first one is corrupted. */
static void test_robustness(void) {
    uint8_t good[11], bad[11];
    hex_to_bytes("8036bcdadc44462601a24b", good);
    memcpy(bad, good, sizeof(bad));
    bad[3] ^= 0x01;
    char* c1 = chips_manchester_frame(bad, 11);
    char* c2 = chips_manchester_frame(good, 11);
    size_t both_len = strlen(c1) + strlen(c2) + 41;
    char* both = malloc(both_len);
    snprintf(both, both_len, "%s%s%s", c1, "0000000000000000000000000000000000000000", c2);
    char* padded = chips_pad(both, 40, 40);
    ProtoViewMsgInfo info = {0};
    bool ok = run_decoder(&VWTPMSDecoder, padded, &info);
    CHECK(ok, "second (good) frame after a corrupted one not decoded");
    if(ok) {
        char id[16];
        field_to_string(id, sizeof(id), info.fieldset->fields[0]);
        CHECK(strcmp(id, "36BCDADC") == 0, "retry frame id %s", id);
    }
    fieldset_free(info.fieldset);
    free(padded);
    free(both);
    free(c1);
    free(c2);
}

void test_vw_suite(void) {
    int before = tests_failed;
    test_registration();
    test_vectors();
    test_negatives();
    test_robustness();
    printf("vw decoder suite: %s\n", tests_failed == before ? "ok" : "FAILED");
}
