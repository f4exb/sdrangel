// Compilation and run:
//   g++ -std=c++17 -o test_correlator test_geoscanCorrelator.cpp geoscanCorrelator.cpp && ./test_correlator

#include "geoscanCorrelator.h"
#include <cstdio>
#include <vector>

// --- test helpers ---

static int failures = 0;
static bool callbackCalled = false;
static std::vector<uint8_t> receivedPacket;

static void onPacket(std::vector<uint8_t> packet) {
    callbackCalled = true;
    receivedPacket = packet;
}

static void check_bool(const char* name, bool got, bool expected) {
    if (got == expected)
        printf("  PASS  %s\n", name);
    else {
        printf("  FAIL  %s: got %s, expected %s\n",
               name, got ? "true" : "false", expected ? "true" : "false");
        failures++;
    }
}

static void check_int(const char* name, int got, int expected) {
    if (got == expected)
        printf("  PASS  %s: %d\n", name, got);
    else {
        printf("  FAIL  %s: got %d, expected %d\n", name, got, expected);
        failures++;
    }
}

// Send one bit as sampPerBit soft samples (255 = '1', 0 = '0')
static void sendBit(Correlator& corr, int sampPerBit, uint8_t bit) {
    uint8_t soft = (bit == 1) ? 255 : 0;
    for (int i = 0; i < sampPerBit; i++)
        corr.process(soft);
}

// Send the full SYNC_WORD as soft samples
static void sendSyncWord(Correlator& corr, int sampPerBit) {
    for (int i = 0; i < SYNC_WORD_LEN; i++)
        sendBit(corr, sampPerBit, SYNC_WORD[i]);
}

// Send one byte MSB-first as soft samples
static void sendByte(Correlator& corr, int sampPerBit, uint8_t byte) {
    for (int bit = 7; bit >= 0; bit--)
        sendBit(corr, sampPerBit, (byte >> bit) & 1);
}

// Send a full packet of PACKET_SIZE bytes
static void sendPacket(Correlator& corr, int sampPerBit, uint8_t fillByte) {
    for (int b = 0; b < PACKET_SIZE; b++)
        sendByte(corr, sampPerBit, fillByte);
}

// --- tests ---

// Midpoint samples (128) have sumCont ~ 4080, well below any reasonable threshold.
// Verifies correlator stays silent on unmodulated carrier.
static void test_no_false_trigger() {
    puts("test_no_false_trigger");
    callbackCalled = false;

    Correlator corr(10, 7500, onPacket);

    for (int i = 0; i < 1000; i++)
        corr.process(128);

    check_bool("callback not called on silence", callbackCalled, false);
}

// Perfect sync word gives sumCont = 32 * 255 = 8160, above threshold.
// After detection, send a known packet and verify the callback receives correct data.
static void test_sync_and_collect() {
    puts("test_sync_and_collect");
    callbackCalled = false;
    receivedPacket.clear();

    const int sampPerBit = 10;
    Correlator corr(sampPerBit, 7500, onPacket);

    sendSyncWord(corr, sampPerBit);
    sendPacket(corr, sampPerBit, 0xAA); // 0xAA = 10101010

    check_bool("callback called after packet", callbackCalled, true);
    check_int("packet size", (int)receivedPacket.size(), PACKET_SIZE);

    if (callbackCalled && (int)receivedPacket.size() == PACKET_SIZE) {
        bool correct = true;
        for (int i = 0; i < PACKET_SIZE; i++) {
            if (receivedPacket[i] != 0xAA) { correct = false; break; }
        }
        if (correct)
            printf("  PASS  packet data is 0xAA throughout\n");
        else {
            printf("  FAIL  packet data mismatch (expected all 0xAA)\n");
            failures++;
        }
    }
}

// Force the circular buffer to wrap several times before sending the sync word.
// Verifies that m_head wrap-around in contribution() doesn't break detection.
static void test_circular_wrap() {
    puts("test_circular_wrap");
    callbackCalled = false;
    receivedPacket.clear();

    const int sampPerBit = 10;
    const int bufLen = SYNC_WORD_LEN * sampPerBit; // 320
    Correlator corr(sampPerBit, 7500, onPacket);

    // Fill buffer twice with midpoint — forces m_head to wrap back to 0
    for (int i = 0; i < bufLen * 2; i++)
        corr.process(128);

    sendSyncWord(corr, sampPerBit);
    sendPacket(corr, sampPerBit, 0xFF); // all ones

    check_bool("callback called after buffer wrap", callbackCalled, true);
    check_int("packet size after wrap", (int)receivedPacket.size(), PACKET_SIZE);

    if (callbackCalled && (int)receivedPacket.size() == PACKET_SIZE) {
        bool correct = true;
        for (int i = 0; i < PACKET_SIZE; i++) {
            if (receivedPacket[i] != 0xFF) { correct = false; break; }
        }
        if (correct)
            printf("  PASS  packet data is 0xFF throughout\n");
        else {
            printf("  FAIL  packet data mismatch (expected all 0xFF)\n");
            failures++;
        }
    }
}

int main() {
    test_no_false_trigger();
    test_sync_and_collect();
    test_circular_wrap();

    if (failures == 0)
        puts("\nAll tests passed.");
    else
        printf("\nFailed: %d\n", failures);

    return failures ? 1 : 0;
}
