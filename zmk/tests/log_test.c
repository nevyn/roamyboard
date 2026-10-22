/*
 * Host tests for zmk/src/log: the ring, the frame encoder and reassembler, and the commands.
 * Run with zmk/tests/run.sh. With --fixture, prints the frames that roamy-log's tests decode
 * (zmk/tools/log/Tests/RoamyLogTests/Fixtures/log_frames.txt) instead.
 *
 * SPDX-License-Identifier: MIT
 */

#include "command.h"
#include "frame.h"
#include "ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed in %s: %s\n", __FILE__, __LINE__, __func__,      \
                    #cond);                                                                        \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        long long a_ = (long long)(a), b_ = (long long)(b);                                        \
        if (a_ != b_) {                                                                            \
            fprintf(stderr, "%s:%d: CHECK_EQ failed in %s: %s == %lld, expected %s == %lld\n",     \
                    __FILE__, __LINE__, __func__, #a, a_, #b, b_);                                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* Ring */

static void push_byte_entry(struct log_ring *r, uint8_t value, size_t len) {
    uint8_t data[64];
    memset(data, value, len);
    CHECK(log_ring_push(r, data, len, NULL));
}

static void test_ring_push_read(void) {
    uint8_t buf[64];
    struct log_ring r;
    log_ring_init(&r, buf, sizeof(buf));
    struct log_ring_cursor c;
    log_ring_cursor_oldest(&r, &c);
    uint8_t out[64];
    uint32_t index = 99, skipped = 99;
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 0);
    CHECK_EQ(skipped, 0);

    uint32_t pushed;
    CHECK(log_ring_push(&r, (const uint8_t *)"abc", 3, &pushed));
    CHECK_EQ(pushed, 0);
    CHECK(log_ring_push(&r, (const uint8_t *)"de", 2, &pushed));
    CHECK_EQ(pushed, 1);
    CHECK_EQ(log_ring_count(&r), 2);

    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 3);
    CHECK_EQ(index, 0);
    CHECK(memcmp(out, "abc", 3) == 0);
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 2);
    CHECK_EQ(index, 1);
    CHECK(memcmp(out, "de", 2) == 0);
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 0);

    CHECK(!log_ring_push(&r, out, 0, NULL));
    CHECK(!log_ring_push(&r, out, 63, NULL)); /* 63 + 2 > 64 */
    CHECK_EQ(log_ring_count(&r), 2);
}

/** Entries wrap around the buffer's end, and the oldest go to make room. */
static void test_ring_wrap_and_overflow(void) {
    uint8_t buf[32];
    struct log_ring r;
    log_ring_init(&r, buf, sizeof(buf));
    /* 10 bytes per entry: three fit, the fourth drops the first. */
    for (uint8_t i = 0; i < 3; i++) {
        push_byte_entry(&r, i, 8);
    }
    CHECK_EQ(log_ring_count(&r), 3);
    CHECK_EQ(r.oldest, 0);
    push_byte_entry(&r, 3, 8);
    CHECK_EQ(log_ring_count(&r), 3);
    CHECK_EQ(r.oldest, 1);

    struct log_ring_cursor c;
    log_ring_cursor_oldest(&r, &c);
    uint8_t out[16];
    uint32_t index, skipped;
    for (uint8_t i = 1; i <= 3; i++) {
        CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 8);
        CHECK_EQ(index, i);
        CHECK_EQ(skipped, 0);
        CHECK_EQ(out[0], i);
        CHECK_EQ(out[7], i); /* entry 3 lies across the buffer's end */
    }

    /* Many more pushes, of mixed lengths, keep every entry intact. */
    for (uint32_t i = 4; i < 200; i++) {
        push_byte_entry(&r, (uint8_t)i, 1 + i % 13);
    }
    log_ring_cursor_oldest(&r, &c);
    uint32_t expected = r.oldest;
    size_t len;
    while ((len = log_ring_read(&r, &c, out, sizeof(out), &index, &skipped)) > 0) {
        CHECK_EQ(index, expected);
        CHECK_EQ(len, 1 + expected % 13);
        CHECK_EQ(out[0], (uint8_t)expected);
        CHECK_EQ(out[len - 1], (uint8_t)expected);
        expected++;
    }
    CHECK_EQ(expected, 200);
}

/** A reader that falls behind skips the dropped entries and learns how many. */
static void test_ring_reader_overrun(void) {
    uint8_t buf[32];
    struct log_ring r;
    log_ring_init(&r, buf, sizeof(buf));
    struct log_ring_cursor c;
    log_ring_cursor_oldest(&r, &c);
    for (uint8_t i = 0; i < 7; i++) {
        push_byte_entry(&r, i, 8);
    }
    uint8_t out[16];
    uint32_t index, skipped;
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 8);
    CHECK_EQ(skipped, 4);
    CHECK_EQ(index, 4);
    CHECK_EQ(out[0], 4);

    /* A cursor at the end reads only what comes after. */
    log_ring_cursor_end(&r, &c);
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 0);
    push_byte_entry(&r, 7, 4);
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 4);
    CHECK_EQ(index, 7);

    /* An entry longer than the reader's buffer is cut. */
    push_byte_entry(&r, 8, 12);
    CHECK_EQ(log_ring_read(&r, &c, out, 5, &index, &skipped), 5);
    CHECK_EQ(index, 8);
}

static void test_ring_seek_and_clear(void) {
    uint8_t buf[64];
    struct log_ring r;
    log_ring_init(&r, buf, sizeof(buf));
    for (uint8_t i = 0; i < 10; i++) {
        push_byte_entry(&r, i, 8); /* six fit */
    }
    CHECK_EQ(r.oldest, 4);
    struct log_ring_cursor c;
    uint8_t out[8];
    uint32_t index, skipped;

    log_ring_cursor_seek(&r, &c, 7);
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 8);
    CHECK_EQ(index, 7);
    CHECK_EQ(out[0], 7);

    log_ring_cursor_seek(&r, &c, 2); /* dropped: the oldest instead */
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 8);
    CHECK_EQ(index, 4);

    log_ring_cursor_seek(&r, &c, 10); /* the next index: nothing yet */
    CHECK_EQ(log_ring_read(&r, &c, out, sizeof(out), &index, &skipped), 0);

    log_ring_cursor_seek(&r, &c, 500); /* from before a restart: the oldest */
    CHECK_EQ(c.index, 4);

    log_ring_clear(&r);
    CHECK_EQ(log_ring_count(&r), 0);
    uint32_t pushed;
    CHECK(log_ring_push(&r, out, 3, &pushed));
    CHECK_EQ(pushed, 10);
}

/* Frames */

static size_t body_of(uint8_t *out, uint32_t uptime, uint8_t level, const char *source,
                      const char *text) {
    return log_frame_body(out, LOG_FRAME_BODY_MAX, uptime, level, source, text, strlen(text));
}

static void test_body(void) {
    uint8_t body[LOG_FRAME_BODY_MAX];
    const size_t len = body_of(body, 0x01020304, 3, "zmk", "hi");
    CHECK_EQ(len, 6 + 3 + 2);
    const uint8_t expected[] = {0x04, 0x03, 0x02, 0x01, 3, 3, 'z', 'm', 'k', 'h', 'i'};
    CHECK(memcmp(body, expected, sizeof(expected)) == 0);

    struct log_frame_body_view view;
    CHECK(log_frame_body_parse(body, len, &view));
    CHECK_EQ(view.uptime_ms, 0x01020304);
    CHECK_EQ(view.level, 3);
    CHECK(!view.truncated);
    CHECK_EQ(view.source_len, 3);
    CHECK(memcmp(view.source, "zmk", 3) == 0);
    CHECK_EQ(view.text_len, 2);

    CHECK(!log_frame_body_parse(body, 5, &view));
    body[5] = 200; /* source longer than the body */
    CHECK(!log_frame_body_parse(body, len, &view));

    CHECK_EQ(log_frame_body(body, 8, 0, 1, "zmk", "x", 1), 0);
    CHECK_EQ(log_frame_body(body, sizeof(body), 0, 1, NULL, "", 0), 6);
}

static void test_body_truncation(void) {
    char text[300];
    memset(text, 'a', sizeof(text));
    uint8_t body[LOG_FRAME_BODY_MAX];
    size_t len = log_frame_body(body, sizeof(body), 0, 4, "zmk", text, sizeof(text));
    struct log_frame_body_view view;
    CHECK(log_frame_body_parse(body, len, &view));
    CHECK_EQ(view.text_len, LOG_FRAME_TEXT_MAX);
    CHECK(view.truncated);
    CHECK_EQ(view.level, 4);

    /* A cut never splits a character: "ä" is two bytes, and 191 + 2 > 192. */
    memset(text, 'a', 191);
    memcpy(text + 191, "\xc3\xa4", 2);
    len = log_frame_body(body, sizeof(body), 0, 4, "zmk", text, 193);
    CHECK(log_frame_body_parse(body, len, &view));
    CHECK_EQ(view.text_len, 191);
    CHECK(view.truncated);

    /* The source is cut to LOG_FRAME_SOURCE_MAX. */
    len = log_frame_body(body, sizeof(body), 0, 4,
                         "a_source_name_that_is_longer_than_thirty_two_bytes", "", 0);
    CHECK(log_frame_body_parse(body, len, &view));
    CHECK_EQ(view.source_len, LOG_FRAME_SOURCE_MAX);
}

/** Writes rec at payload size cap and feeds every fragment to r; returns the fragment count. */
static int round_trip(struct log_frame_reassembler *r, enum log_frame_half half, bool backlog,
                      uint32_t seq, const uint8_t *body, size_t len, size_t cap) {
    struct log_frame_writer w;
    log_frame_writer_init(&w, half, backlog, seq, body, len);
    uint8_t frag[256];
    size_t n;
    int count = 0;
    while ((n = log_frame_writer_next(&w, frag, cap)) > 0) {
        CHECK(n <= cap);
        CHECK_EQ(frag[1], count);
        CHECK_EQ(frag[0] >> 4, LOG_FRAME_VERSION);
        const bool last = log_frame_writer_done(&w);
        CHECK_EQ((frag[0] & LOG_FRAME_FLAG_LAST) != 0, last);
        CHECK_EQ(log_frame_reassemble(r, frag, n), last ? LOG_FRAME_COMPLETE : LOG_FRAME_PARTIAL);
        count++;
    }
    return count;
}

static void test_fragmentation(void) {
    char text[LOG_FRAME_TEXT_MAX];
    for (size_t i = 0; i < sizeof(text); i++) {
        text[i] = (char)('a' + i % 26);
    }
    uint8_t body[LOG_FRAME_BODY_MAX];
    const size_t len = log_frame_body(body, sizeof(body), 77, 3, "bt_hci_core", text, sizeof(text));
    const size_t caps[] = {20, 21, 23, 62, 100, 182, 244};
    for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        struct log_frame_reassembler r;
        log_frame_reassembler_init(&r);
        const size_t per = caps[i] - LOG_FRAME_HEADER_LEN;
        const int fragments = round_trip(&r, LOG_FRAME_HALF_RIGHT, true, 0xDEADBEEF, body, len,
                                         caps[i]);
        CHECK_EQ(fragments, (int)((len + per - 1) / per));
        CHECK_EQ(r.len, len);
        CHECK(memcmp(r.body, body, len) == 0);
        CHECK_EQ(r.seq, 0xDEADBEEF);
        CHECK_EQ(r.half, LOG_FRAME_HALF_RIGHT);
        CHECK(r.backlog);
    }

    /* A short record is one fragment, and a payload below the minimum writes nothing. */
    const size_t short_len = body_of(body, 1, 3, "zmk", "ok");
    struct log_frame_reassembler r;
    log_frame_reassembler_init(&r);
    CHECK_EQ(round_trip(&r, LOG_FRAME_HALF_LEFT, false, 5, body, short_len, 20), 1);
    struct log_frame_writer w;
    log_frame_writer_init(&w, LOG_FRAME_HALF_LEFT, false, 5, body, short_len);
    uint8_t frag[32];
    CHECK_EQ(log_frame_writer_next(&w, frag, LOG_FRAME_PAYLOAD_MIN - 1), 0);
}

static void test_reassembly_errors(void) {
    uint8_t body[LOG_FRAME_BODY_MAX];
    char text[100];
    memset(text, 'x', sizeof(text));
    const size_t len = log_frame_body(body, sizeof(body), 1, 3, "zmk", text, sizeof(text));
    uint8_t frags[9][20];
    size_t lens[9];
    struct log_frame_writer w;
    log_frame_writer_init(&w, LOG_FRAME_HALF_LEFT, false, 9, body, len);
    int n = 0;
    while ((lens[n] = log_frame_writer_next(&w, frags[n], 20)) > 0) {
        n++;
    }
    CHECK_EQ(n, 8); /* 109 bytes, 14 per fragment */

    struct log_frame_reassembler r;
    log_frame_reassembler_init(&r);
    CHECK_EQ(log_frame_reassemble(&r, frags[0], lens[0]), LOG_FRAME_PARTIAL);
    CHECK_EQ(log_frame_reassemble(&r, frags[2], lens[2]), LOG_FRAME_OUT_OF_ORDER);
    /* The rest of that record is out of order too, until a first fragment comes. */
    CHECK_EQ(log_frame_reassemble(&r, frags[3], lens[3]), LOG_FRAME_OUT_OF_ORDER);
    CHECK_EQ(log_frame_reassemble(&r, frags[0], lens[0]), LOG_FRAME_PARTIAL);
    /* A new first fragment cuts the record in progress short. */
    CHECK_EQ(log_frame_reassemble(&r, frags[0], lens[0]), LOG_FRAME_PARTIAL);
    CHECK_EQ(r.incomplete, 1);
    for (int i = 1; i < n; i++) {
        CHECK_EQ(log_frame_reassemble(&r, frags[i], lens[i]), i == n - 1 ? LOG_FRAME_COMPLETE : LOG_FRAME_PARTIAL);
    }

    CHECK_EQ(log_frame_reassemble(&r, frags[0], 5), LOG_FRAME_MALFORMED);
    uint8_t other_version[20];
    memcpy(other_version, frags[0], lens[0]);
    other_version[0] = (uint8_t)((other_version[0] & 0x0F) | 2 << 4);
    CHECK_EQ(log_frame_reassemble(&r, other_version, lens[0]), LOG_FRAME_BAD_VERSION);

    /* A continuation of another sequence number does not belong to the record. */
    CHECK_EQ(log_frame_reassemble(&r, frags[0], lens[0]), LOG_FRAME_PARTIAL);
    uint8_t wrong_seq[20];
    memcpy(wrong_seq, frags[1], lens[1]);
    wrong_seq[2]++;
    CHECK_EQ(log_frame_reassemble(&r, wrong_seq, lens[1]), LOG_FRAME_OUT_OF_ORDER);

    /* A record that never ends overflows the body. */
    log_frame_reassembler_init(&r);
    uint8_t endless[20];
    memcpy(endless, frags[1], lens[1]);
    CHECK_EQ(log_frame_reassemble(&r, frags[0], lens[0]), LOG_FRAME_PARTIAL);
    enum log_frame_result result = LOG_FRAME_PARTIAL;
    for (int i = 1; i < 40 && result == LOG_FRAME_PARTIAL; i++) {
        endless[1] = (uint8_t)i;
        result = log_frame_reassemble(&r, endless, lens[1]);
    }
    CHECK_EQ(result, LOG_FRAME_TOO_LONG);

    /* A complete record whose body does not parse. */
    log_frame_reassembler_init(&r);
    const uint8_t bad_body[] = {0, 0, 0, 0, 3, 9, 'z'};
    log_frame_writer_init(&w, LOG_FRAME_HALF_LEFT, false, 1, bad_body, sizeof(bad_body));
    uint8_t frag[32];
    const size_t frag_len = log_frame_writer_next(&w, frag, sizeof(frag));
    CHECK_EQ(log_frame_reassemble(&r, frag, frag_len), LOG_FRAME_MALFORMED);
}

/* Commands */

static bool parse(const char *text, struct log_cmd *cmd) {
    return log_cmd_parse(text, strlen(text), cmd);
}

static void test_commands(void) {
    struct log_cmd cmd;
    CHECK(parse("start", &cmd));
    CHECK_EQ(cmd.kind, LOG_CMD_START);
    CHECK(!cmd.has_from[0] && !cmd.has_from[1] && !cmd.has_from[2]);
    CHECK(parse("start L:12 R:4294967295", &cmd));
    CHECK(cmd.has_from[LOG_FRAME_HALF_LEFT] && cmd.has_from[LOG_FRAME_HALF_RIGHT]);
    CHECK(!cmd.has_from[LOG_FRAME_HALF_UNIBODY]);
    CHECK_EQ(cmd.from[LOG_FRAME_HALF_LEFT], 12);
    CHECK_EQ(cmd.from[LOG_FRAME_HALF_RIGHT], 4294967295u);
    CHECK(parse("start U:0", &cmd));
    CHECK(cmd.has_from[LOG_FRAME_HALF_UNIBODY]);
    CHECK(!parse("start R:4294967296", &cmd));
    CHECK(!parse("start X:1", &cmd));
    CHECK(!parse("start L:", &cmd));
    CHECK(!parse("start L:1a", &cmd));
    CHECK(!parse("started", &cmd));

    CHECK(parse("live", &cmd));
    CHECK_EQ(cmd.kind, LOG_CMD_LIVE);
    CHECK(parse("stop", &cmd));
    CHECK_EQ(cmd.kind, LOG_CMD_STOP);
    CHECK(parse("list", &cmd));
    CHECK_EQ(cmd.kind, LOG_CMD_LIST);

    CHECK(parse("bt_hci_core=dbg", &cmd));
    CHECK_EQ(cmd.kind, LOG_CMD_LEVEL);
    CHECK(strcmp(cmd.module, "bt_hci_core") == 0);
    CHECK_EQ(cmd.level, 4);
    CHECK(parse("all=off", &cmd));
    CHECK(strcmp(cmd.module, "all") == 0);
    CHECK_EQ(cmd.level, 0);
    CHECK(!parse("zmk=debug", &cmd));
    CHECK(!parse("=dbg", &cmd));
    CHECK(!parse("zmk=", &cmd));
    CHECK(!parse("z mk=dbg", &cmd));
    CHECK(!parse("a_source_name_that_is_longer_than_32=dbg", &cmd));
    CHECK(!parse("", &cmd));
    CHECK(!log_cmd_parse("live\0", 5, &cmd));

    CHECK(strcmp(log_cmd_level_name(3), "inf") == 0);
    CHECK(strcmp(log_cmd_level_name(7), "raw") == 0);
}

/* The fixture that roamy-log's tests decode; its records are spelled out there too. */

static void print_fragments(enum log_frame_half half, bool backlog, uint32_t seq,
                            const uint8_t *body, size_t len, size_t cap) {
    struct log_frame_writer w;
    log_frame_writer_init(&w, half, backlog, seq, body, len);
    uint8_t frag[256];
    size_t n;
    while ((n = log_frame_writer_next(&w, frag, cap)) > 0) {
        printf("%zu ", cap);
        for (size_t i = 0; i < n; i++) {
            printf("%02x", frag[i]);
        }
        printf("\n");
    }
}

static void print_fixture(void) {
    printf("# roamyboard log frames, written by `log_test --fixture` (zmk/tests/log_test.c);\n"
           "# zmk/tests/run.sh checks that this file matches. Each line: the notification\n"
           "# payload size, then one notification in hex.\n");
    char alphabet[9 * 26 + 1];
    for (size_t i = 0; i < 9 * 26; i++) {
        alphabet[i] = (char)('a' + i % 26);
    }
    alphabet[9 * 26] = '\0';
    const size_t caps[] = {20, 62, 244};
    for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        uint8_t body[LOG_FRAME_BODY_MAX];
        size_t len = body_of(body, 1234, 3, "zmk", "Welcome to ZMK!");
        print_fragments(LOG_FRAME_HALF_LEFT, true, 0, body, len, caps[i]);
        len = body_of(body, 1300, 4, "zmk", "kscan_165_chain_poll: 7 key modules");
        print_fragments(LOG_FRAME_HALF_LEFT, true, 1, body, len, caps[i]);
        len = body_of(body, 3723004, 2, "bt_hci_core", alphabet);
        print_fragments(LOG_FRAME_HALF_RIGHT, false, 7, body, len, caps[i]);
        len = body_of(body, 5000, 1, "roamy_log", "r\xc3\xa4ksm\xc3\xb6rg\xc3\xa5s \xe2\x9c\x93");
        print_fragments(LOG_FRAME_HALF_LEFT, false, 4, body, len, caps[i]);
        len = log_frame_body(body, sizeof(body), 0, 0, NULL, "", 0);
        print_fragments(LOG_FRAME_HALF_UNIBODY, false, 0, body, len, caps[i]);
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--fixture") == 0) {
        print_fixture();
        return 0;
    }
    test_ring_push_read();
    test_ring_wrap_and_overflow();
    test_ring_reader_overrun();
    test_ring_seek_and_clear();
    test_body();
    test_body_truncation();
    test_fragmentation();
    test_reassembly_errors();
    test_commands();
    if (failures) {
        fprintf(stderr, "log_test: %d failures\n", failures);
        return EXIT_FAILURE;
    }
    printf("log_test: all passed\n");
    return EXIT_SUCCESS;
}
