///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

// Self test of the FLEX frame decoder. Frames of the transmitter of this directory are decoded and
// compared with the messages sent, frames of a reference transmitter (radiopagingflexdecoderdata.inc) are
// compared with the messages sent and with the messages a reference receiver decoded from them. Bit errors,
// erased codewords, lost fragments, duplicates and repeated subframes are tested with modified frames.
// Prints one line per check and returns non-zero if any check fails.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "radiopagingbch.h"
#include "radiopagingflex.h"
#include "radiopagingflexdecoder.h"
#include "radiopagingflexscheduler.h"

namespace {

int failures = 0;
int passes = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

typedef RadioPagingFlex Flex;
typedef RadioPagingFlexDecoder Dec;
typedef RadioPagingFlexEncoder Enc;

struct Exp
{
    unsigned long long m_cap;
    const char *m_type;
    const char *m_text;
    int m_number;
    int m_complete;
    int m_frags;
};

struct RefFrame
{
    const char *m_scenario;
    const char *m_name;
    int m_mode;
    int m_inverted;
    int m_cycle;
    int m_frame;
    const char *m_sync;
    const char *m_data;
    int m_nexp;
    Exp m_exp[16];
    int m_nref;
    const char *m_ref[64];
};

const RefFrame refFrames[] = {
#include "radiopagingflexdecoderdata.inc"
};

const size_t refFrameCount = sizeof(refFrames) / sizeof(refFrames[0]);

std::vector<uint8_t> fromHex(const char *hex)
{
    std::vector<uint8_t> out;

    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
    {
        unsigned v = 0;
        std::sscanf(hex + i, "%2x", &v);
        out.push_back((uint8_t) v);
    }

    return out;
}

// Slices the data part of a frame (second sync and phases) into the phases. This is the inverse of
// RadioPagingFlex::assemble
Dec::RxFrame toRxFrame(Flex::Mode mode, bool inverted, const std::vector<uint8_t>& sync, const std::vector<uint8_t>& data)
{
    static const int s2Bytes[4] = {5, 10, 10, 20};
    Dec::RxFrame frame;
    int phases = Flex::phaseCount(mode);
    size_t bit = 0;

    frame.m_mode = mode;
    frame.m_inverted = inverted;
    frame.m_hasFiw = (sync.size() >= 18);

    if (frame.m_hasFiw) {
        frame.m_fiw = ((uint32_t) sync[14] << 24) | ((uint32_t) sync[15] << 16) | ((uint32_t) sync[16] << 8) | sync[17];
    }

    frame.m_phases.resize(phases);

    for (int p = 0; p < phases; p++)
    {
        frame.m_phases[p].m_name = (phases == 1) ? 'A' : (phases == 2) ? (p ? 'C' : 'A') : (char) ('A' + p);
        frame.m_phases[p].m_words.assign(Flex::m_wordsPerFrame, 0);
    }

    size_t start = (size_t) s2Bytes[(int) mode] * 8;

    for (int w = 0; w < Flex::m_wordsPerFrame; w++)
    {
        for (int b = 31; b >= 0; b--)
        {
            for (int p = 0; p < phases; p++, bit++)
            {
                size_t pos = start + bit;

                if ((pos / 8 < data.size()) && ((data[pos / 8] >> (7 - pos % 8)) & 1)) {
                    frame.m_phases[p].m_words[w] |= 1u << b;
                }
            }
        }
    }

    return frame;
}

// Helpers working on deinterleaved codewords of a phase
std::vector<uint32_t> deinterleaved(const std::vector<uint32_t>& words)
{
    std::vector<uint32_t> out = words;

    for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
        Flex::deinterleaveBlock(&out[b * Flex::m_wordsPerBlock]);
    }

    return out;
}

std::vector<uint32_t> interleaved(const std::vector<uint32_t>& words)
{
    std::vector<uint32_t> out = words;

    for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
        Flex::interleaveBlock(&out[b * Flex::m_wordsPerBlock]);
    }

    return out;
}

void editWords(Dec::RxPhase& phase, const std::vector<std::pair<int, uint32_t>>& xors)
{
    std::vector<uint32_t> words = deinterleaved(phase.m_words);

    for (const auto& x : xors) {
        words[x.first] ^= x.second;
    }

    phase.m_words = interleaved(words);
}

std::vector<uint32_t> codewords(const Dec::RxPhase& phase)
{
    return deinterleaved(phase.m_words);
}

std::string numericText(const std::string& text)
{
    std::string out;

    for (char c : text)
    {
        if (c == '_') {
            out += '-';
        } else if (c == 'u') {
            out += 'U';
        } else {
            out += c;
        }
    }

    return out;
}

// Same message in the text of the reference receiver
std::string refRecord(const Dec::Message& m, bool& comparable)
{
    const char *tag = nullptr;
    std::string text = m.m_text;

    comparable = true;

    switch (m.m_type)
    {
    case Dec::TypeAlphanumeric: tag = "ALN"; break;
    case Dec::TypeSecure: tag = "SEC"; break;
    case Dec::TypeNumeric: tag = "NUM"; break;
    case Dec::TypeSpecialNumeric: tag = "SNUM"; break;
    case Dec::TypeNumberedNumeric: tag = "NNUM"; break;
    case Dec::TypeNumberedSpecialNumeric: tag = "NSNUM"; break;
    case Dec::TypeTone: tag = "TON"; break;
    case Dec::TypeShortMessage:
        tag = "SMSG";
        comparable = (m.m_shortType == 0);
        break;
    case Dec::TypeBinary:
        tag = "HEX";
        comparable = !m.m_fragmented;

        if (!text.empty())
        {
            char last = text.back();
            int nib = (last >= 'A') ? (last - 'A' + 10) : (last - '0');
            char fill = ((nib >> 3) & 1) ? '0' : 'F';

            text.append((text.size() % 2 == 0) ? 2 : 1, fill);
        }
        break;
    default:
        comparable = false;
        return "";
    }

    char head[32];
    std::snprintf(head, sizeof(head), "%010llu ", (unsigned long long) m.m_capcode);
    return std::string(head) + tag + "|" + text;
}

// Unused digit positions of the last word of a numeric message are spaces
std::string trimNumeric(const Dec::Message& m)
{
    std::string text = m.m_text;

    if ((m.m_type == Dec::TypeNumeric) || (m.m_type == Dec::TypeSpecialNumeric) || (m.m_type == Dec::TypeNumberedNumeric)
        || (m.m_type == Dec::TypeNumberedSpecialNumeric))
    {
        while (!text.empty() && (text.back() == ' ')) {
            text.pop_back();
        }
    }

    return text;
}

bool matches(const Dec::Message& m, const Exp& e)
{
    if ((m.m_capcode != e.m_cap) || (m.m_typeName != e.m_type) || (trimNumeric(m) != e.m_text)) {
        return false;
    }
    if ((e.m_number != -2) && (m.m_messageNumber != e.m_number)) {
        return false;
    }
    if (m.m_complete != (e.m_complete != 0)) {
        return false;
    }
    return (e.m_frags == 0) || (m.m_fragmentCount == e.m_frags);
}

std::string describe(const Dec::Message& m)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%llu/%s/n=%d/c=%d/f=%d/", (unsigned long long) m.m_capcode, m.m_typeName.c_str(),
        m.m_messageNumber, m.m_complete ? 1 : 0, m.m_fragmentCount);
    return std::string(buf) + m.m_text;
}

// ---------------------------------------------------------------------------
// Frames of the reference transmitter
// ---------------------------------------------------------------------------

void testReferenceFrames()
{
    Dec decoder;
    std::string scenario;
    int frames = 0;
    int expectOk = 0;
    int refOk = 0;
    int refCompared = 0;
    int refDifferent = 0;
    std::map<std::string, std::pair<int, int>> perScenario;

    for (size_t i = 0; i < refFrameCount; i++)
    {
        const RefFrame& f = refFrames[i];

        if (scenario != f.m_scenario)
        {
            scenario = f.m_scenario;
            decoder.reset();
            decoder.setReferenceYear(2026);
        }

        Dec::RxFrame rx = toRxFrame((Flex::Mode) f.m_mode, f.m_inverted != 0, fromHex(f.m_sync), fromHex(f.m_data));
        std::vector<Dec::Message> out = decoder.decodeFrame(rx);
        frames++;

        // The reference lists have no retransmissions: duplicates are output, marked, and left out here
        out.erase(std::remove_if(out.begin(), out.end(), [](const Dec::Message& m) { return m.m_duplicate; }), out.end());

        // Messages sent. The reference transmitter sends a tone only page as an address without a vector, which
        // the decoder does not decode: left out of the messages expected
        std::vector<const Exp*> expected;

        for (int e = 0; e < f.m_nexp; e++)
        {
            if (std::string(f.m_exp[e].m_type) != "tone") {
                expected.push_back(&f.m_exp[e]);
            }
        }

        bool ok = (out.size() == expected.size());
        // Incomplete messages are not reported by the reference receiver
        std::vector<Dec::Message> outComplete;

        for (const Dec::Message& m : out)
        {
            if (m.m_complete) {
                outComplete.push_back(m);
            }
        }
        std::vector<bool> used(out.size(), false);

        for (size_t e = 0; e < expected.size(); e++)
        {
            bool found = false;

            for (size_t k = 0; k < out.size(); k++)
            {
                if (!used[k] && matches(out[k], *expected[e]))
                {
                    used[k] = true;
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::printf("   no match for expected message %zu\n", e);
            }

            ok = ok && found;
        }

        if (ok) {
            expectOk++;
        } else {
            std::printf("FAIL: sent messages %s / %s: expected %zu got %zu\n", f.m_scenario, f.m_name, expected.size(), out.size());

            for (const Exp *e : expected)
            {
                std::printf("   expected %llu/%s/n=%d/c=%d/f=%d/%s\n", e->m_cap, e->m_type, e->m_number,
                    e->m_complete, e->m_frags, e->m_text);
            }
            for (const Dec::Message& m : out) {
                std::printf("   got      %s\n", describe(m).c_str());
            }
        }

        perScenario[f.m_scenario].first += ok ? 1 : 0;
        perScenario[f.m_scenario].second++;

        // Messages of the reference receiver
        std::vector<std::string> mine;

        for (const Dec::Message& m : outComplete)
        {
            bool comparable;
            std::string rec = refRecord(m, comparable);

            if (comparable) {
                mine.push_back(rec);
            }
        }

        std::vector<std::string> theirs;

        for (int r = 0; r < f.m_nref; r++)
        {
            // Hex messages in fragments are not compared (the reference pads each fragment)
            std::string rec = f.m_ref[r];

            if ((rec.find("|") != std::string::npos) && (rec.find(" HEX|") != std::string::npos) && (std::string(f.m_scenario) == "fragments")) {
                continue;
            }
            // A tone only address without a vector is not decoded
            if (rec.find(" TON|") != std::string::npos) {
                continue;
            }
            // The reference keeps the unused digits of the last word of a numeric message (spaces, fill): the decoder
            // leaves them out
            if ((rec.find(" NUM|") != std::string::npos) || (rec.find(" SNUM|") != std::string::npos)
                || (rec.find(" NNUM|") != std::string::npos) || (rec.find(" NSNUM|") != std::string::npos))
            {
                while (!rec.empty() && (rec.back() == ' ')) {
                    rec.pop_back();
                }
            }

            theirs.push_back(rec);
        }

        std::sort(mine.begin(), mine.end());
        std::sort(theirs.begin(), theirs.end());
        refCompared++;

        if (mine == theirs)
        {
            refOk++;
        }
        else
        {
            refDifferent++;
            std::printf("FAIL: reference receiver %s / %s\n", f.m_scenario, f.m_name);

            for (const std::string& s : theirs) {
                std::printf("   reference %s\n", s.c_str());
            }
            for (const std::string& s : mine) {
                std::printf("   decoder   %s\n", s.c_str());
            }
        }
    }

    for (const auto& s : perScenario)
    {
        std::string what = "reference frames, messages sent: scenario " + s.first;
        char buf[64];

        std::snprintf(buf, sizeof(buf), " (%d frames)", s.second.second);
        check(s.second.first == s.second.second, (what + buf).c_str());
    }

    char buf[128];

    std::snprintf(buf, sizeof(buf), "reference frames: %d of %d match the messages sent", expectOk, frames);
    check(expectOk == frames, buf);
    std::snprintf(buf, sizeof(buf), "reference frames: %d of %d match the reference receiver", refOk, refCompared);
    check(refDifferent == 0, buf);
}

// Network time and identification from the frames of the scenario "biw"
void testBiw()
{
    Dec decoder;
    bool any = false;

    decoder.setReferenceYear(2026);

    for (size_t i = 0; i < refFrameCount; i++)
    {
        const RefFrame& f = refFrames[i];

        if (std::string(f.m_scenario) != "biw") {
            continue;
        }

        any = true;
        decoder.decodeFrame(toRxFrame((Flex::Mode) f.m_mode, false, fromHex(f.m_sync), fromHex(f.m_data)));

        if (std::string(f.m_name) == "biw 0")
        {
            const Dec::FrameReport& r = decoder.lastFrame();
            check(r.m_fiwValid && (r.m_cycle == 3) && (r.m_frame == 8), "biw: frame information word");
            check(!r.m_blocks.empty() && r.m_blocks[0].m_valid && (r.m_blocks[0].m_extra.size() == 3),
                "biw: three extra block information words");

            if (!r.m_blocks.empty() && (r.m_blocks[0].m_extra.size() == 3))
            {
                const Dec::ExtraBiw& d = r.m_blocks[0].m_extra[0];
                const Dec::ExtraBiw& t = r.m_blocks[0].m_extra[1];
                const Dec::ExtraBiw& s = r.m_blocks[0].m_extra[2];

                check((d.m_kind == Dec::ExtraBiw::Date) && d.m_dateValid && (d.m_year == 2026) && (d.m_month == 11) && (d.m_day == 9),
                    "biw: date word 2026-11-09");
                check((t.m_kind == Dec::ExtraBiw::Time) && t.m_timeValid && (t.m_hour == 14) && (t.m_minute == 13) && (t.m_second7_5 == 2),
                    "biw: time word 14:13:15");
                check((s.m_kind == Dec::ExtraBiw::Ssid1) && (s.m_localId == 300) && (s.m_coverageZone == 17), "biw: SSID1 word");
            }
        }
    }

    Dec::NetworkTime t = decoder.networkTime();

    check(any, "biw: scenario present");
    check(t.m_dateValid && (t.m_year == 2026) && (t.m_month == 11) && (t.m_day == 9), "network time: date confirmed after repeated reception");
    check(t.m_timeValid && (t.m_hour == 14) && (t.m_minute == 13) && (t.m_second7_5 == 2), "network time: time confirmed");
    check(t.m_zoneValid && (t.m_zone == 2) && (t.m_zoneMinutes == 120) && (t.m_dst == 1) && (t.m_extendedSeconds == 1),
        "network time: time zone confirmed");
    check(t.m_ssid1Valid && (t.m_localId == 300) && (t.m_coverageZone == 17), "network identification: SSID1");
    check(t.m_ssid2Valid && (t.m_country == 262) && (t.m_tmf == 5), "network identification: SSID2");

    Dec::NetworkTime other = decoder.networkTime(true);
    check(!other.m_dateValid && !other.m_timeValid && !other.m_ssid1Valid, "network time: inverted polarity is a separate network");

    // Time is valid only after repeated reception
    Dec once;

    for (size_t i = 0; i < refFrameCount; i++)
    {
        const RefFrame& f = refFrames[i];

        if ((std::string(f.m_scenario) == "biw") && (std::string(f.m_name) == "biw 0")) {
            once.decodeFrame(toRxFrame((Flex::Mode) f.m_mode, false, fromHex(f.m_sync), fromHex(f.m_data)));
        }
    }

    check(!once.networkTime().m_dateValid && !once.networkTime().m_timeValid, "network time: one reception is not enough");
}

// ---------------------------------------------------------------------------
// Frames of the transmitter of this directory
// ---------------------------------------------------------------------------

struct Sent
{
    uint64_t m_capcode;
    Flex::MessageType m_type;
    std::string m_text;
};

std::string typeName(Flex::MessageType type)
{
    return (type == Flex::Tone) ? "tone" : (type == Flex::Numeric) ? "numeric" : "alphanumeric";
}

std::vector<Dec::Message> runEncoder(Enc& enc, Dec& dec, std::vector<Dec::RxFrame> *framesOut = nullptr, int *frameCount = nullptr)
{
    std::vector<Dec::Message> all;
    int frames = 0;

    for (uint64_t index = 0; (index < 4000) && (enc.pending() > 0); index++)
    {
        Enc::Frame f = enc.buildFrame(index);

        if (f.m_reserved) {
            continue;
        }

        Dec::RxFrame rx = toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data);
        std::vector<Dec::Message> out = dec.decodeFrame(rx);

        all.insert(all.end(), out.begin(), out.end());
        frames++;

        if (framesOut) {
            framesOut->push_back(rx);
        }
    }

    if (frameCount) {
        *frameCount = frames;
    }

    return all;
}

void testRoundTrip()
{
    std::mt19937 rng(12345);
    const Flex::Mode modes[4] = {Flex::Mode1600_2, Flex::Mode3200_2, Flex::Mode3200_4, Flex::Mode6400_4};
    int total = 0;
    int bad = 0;

    for (int m = 0; m < 4; m++)
    {
        for (int inv = 0; inv < 2; inv++)
        {
            for (int collapse = 0; collapse <= 7; collapse += 7)
            {
                Enc enc;
                Dec dec;
                std::string err;
                std::vector<Sent> sent;

                enc.setMode(modes[m]);
                enc.setInverted(inv != 0);
                enc.setCollapse(collapse, err);

                for (int i = 0; i < 40; i++)
                {
                    Enc::TxMessage tx;
                    int kind = rng() % 3;
                    bool isLong = (rng() % 4) == 0;

                    tx.m_capcode = isLong ? (2101249ULL + rng() % 1000000000ULL) : (1 + rng() % 1933311);
                    tx.m_priority = (rng() % 5) == 0;

                    if (kind == 0)
                    {
                        tx.m_type = Flex::Tone;
                    }
                    else if (kind == 1)
                    {
                        tx.m_type = Flex::Numeric;
                        int len = 1 + rng() % 41;

                        for (int c = 0; c < len; c++) {
                            tx.m_text += "0123456789 -[]U."[rng() % 15];
                        }
                    }
                    else
                    {
                        tx.m_type = Flex::Alphanumeric;
                        int len = 1 + rng() % ((i % 7 == 0) ? 248 : 60);

                        for (int c = 0; c < len; c++) {
                            tx.m_text += (char) (0x20 + rng() % 0x5f);
                        }
                    }

                    Enc::EnqueueResult r = enc.enqueue(tx);

                    if (r.m_ok) {
                        sent.push_back({tx.m_capcode, tx.m_type, tx.m_text});
                    }
                }

                std::vector<Dec::Message> got = runEncoder(enc, dec);
                std::vector<std::string> a;
                std::vector<std::string> b;

                for (const Sent& s : sent)
                {
                    std::string text = (s.m_type == Flex::Numeric) ? numericText(s.m_text) : (s.m_type == Flex::Tone) ? "" : s.m_text;
                    while ((s.m_type == Flex::Numeric) && !text.empty() && (text.back() == ' ')) {
                        text.pop_back();
                    }

                    a.push_back(std::to_string(s.m_capcode) + "/" + typeName(s.m_type) + "/" + text);
                }
                for (const Dec::Message& g : got)
                {
                    bool good = g.m_complete && (g.m_wordsUncorrectable == 0) && (g.m_wordsCorrected == 0);
                    b.push_back(std::to_string(g.m_capcode) + "/" + g.m_typeName + "/" + trimNumeric(g));

                    if (!good) {
                        bad++;
                    }
                }

                std::sort(a.begin(), a.end());
                std::sort(b.begin(), b.end());
                total++;

                if (a != b)
                {
                    int shown = 0;

                    for (const std::string& s : a)
                    {
                        if (!std::binary_search(b.begin(), b.end(), s) && (shown++ < 3)) {
                            std::printf("   sent but not decoded: %s (%zu)\n", s.c_str(), s.size());
                        }
                    }
                    for (const std::string& s : b)
                    {
                        if (!std::binary_search(a.begin(), a.end(), s) && (shown++ < 6)) {
                            std::printf("   decoded but not sent: %s (%zu)\n", s.c_str(), s.size());
                        }
                    }
                }

                char buf[128];
                std::snprintf(buf, sizeof(buf), "round trip %s %s collapse %d: %zu messages", Flex::modeName(modes[m]),
                    inv ? "inverted" : "normal", collapse, sent.size());
                check(a == b, buf);

                // The bit error rate of each phase of the mode, clean frames: no errors
                const int phases = (modes[m] == Flex::Mode1600_2) ? 1 : (modes[m] == Flex::Mode6400_4) ? 4 : 2;
                bool phaseBer = true;

                for (int p = 0; p < 4; p++)
                {
                    const RadioPagingBerWindow& ber = dec.status().m_phaseBer[p];
                    // The phases of a 2 phase mode are A and C (STD-43A 3.4)
                    bool used = (phases == 4) || ((phases == 2) && ((p == 0) || (p == 2))) || ((phases == 1) && (p == 0));
                    phaseBer = phaseBer && (ber.valid() == used) && (ber.m_errorBits == 0);
                }

                std::snprintf(buf, sizeof(buf), "phase BER %s %s collapse %d: the phases of the mode, no errors", Flex::modeName(modes[m]),
                    inv ? "inverted" : "normal", collapse);
                check(phaseBer, buf);
            }
        }
    }

    check(bad == 0, "round trip: no word errors reported on clean frames");
    (void) total;

    // Idle frames and collapse
    Enc enc;
    Dec dec;
    Enc::Frame f = enc.buildFrame(5);
    std::vector<Dec::Message> out = dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));

    check(f.m_idle && out.empty() && dec.lastFrame().m_fiwValid && dec.lastFrame().m_blocks.size() == 1 && dec.lastFrame().m_blocks[0].m_idle,
        "idle frame: no messages, block information word seen");

    std::string err;
    enc.setCollapse(5, err);
    out = dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
    Enc::Frame g = enc.buildFrame(6);
    dec.decodeFrame(toRxFrame(g.m_mode, g.m_inverted, g.m_bytes.m_sync, g.m_bytes.m_data));
    check(dec.lastFrame().m_blocks[0].m_collapse == 5 && dec.lastFrame().m_cycle == 0 && dec.lastFrame().m_frame == 6,
        "collapse and frame number from block and frame information words");
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

Dec::RxFrame encoderFrame(Enc& enc, const std::vector<Enc::TxMessage>& msgs, uint64_t index = 0)
{
    for (const Enc::TxMessage& m : msgs) {
        enc.enqueue(m);
    }

    for (; index < 4000; index++)
    {
        Enc::Frame f = enc.buildFrame(index);

        if (!f.m_idle && !f.m_reserved) {
            return toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data);
        }
    }

    return Dec::RxFrame();
}

// Frames of the two polarities in turn (two networks, or a transmitter alternating them): each polarity keeps the
// report of its own last frame (mode, frame, phases) and its own bit error rates. Was: one report and one set of phase
// windows for both, so the status showed the mode of the frame being received with the phases of the frame before it
// (an inverted 3200/4 frame shown with the one phase of the normal 1600/2 frame before it)
void testPolarityReports()
{
    Enc encInverted;
    Enc encNormal;
    Enc::TxMessage tx;

    encInverted.setMode(Flex::Mode3200_4);
    encInverted.setInverted(true);
    encNormal.setMode(Flex::Mode1600_2);
    encNormal.setInverted(false);
    tx.m_capcode = 1234567;
    tx.m_type = Flex::Alphanumeric;
    tx.m_text = "polarity reports";

    Dec::RxFrame inverted = encoderFrame(encInverted, {tx}, 94);
    Dec::RxFrame normal = encoderFrame(encNormal, {tx}, 95);
    Dec dec;
    dec.decodeFrame(inverted);
    dec.decodeFrame(normal);

    const Dec::FrameReport& inv = dec.lastFrame(true);
    const Dec::FrameReport& norm = dec.lastFrame(false);
    check(inv.m_fiwValid && inv.m_inverted && (inv.m_mode == Flex::Mode3200_4) && (inv.m_blocks.size() == 2),
        "polarity reports: inverted 3200/4 frame kept with its 2 phases after a normal frame");
    check(norm.m_fiwValid && !norm.m_inverted && (norm.m_mode == Flex::Mode1600_2) && (norm.m_blocks.size() == 1),
        "polarity reports: normal 1600/2 frame with its 1 phase");
    check(inv.m_frame != norm.m_frame, "polarity reports: each polarity its own frame number");
    check(dec.phaseBer(true, 0).valid() && dec.phaseBer(true, 2).valid(), "polarity reports: inverted phases A and C measured");
    check(dec.phaseBer(false, 0).valid() && !dec.phaseBer(false, 2).valid(),
        "polarity reports: normal phase A only, nothing of the inverted phase C");
    check(dec.ber(true).valid() && dec.ber(false).valid() && (dec.ber(true).m_bits != dec.ber(false).m_bits),
        "polarity reports: a bit error rate for each polarity");
    check(!dec.lastFrame().m_inverted && (dec.lastFrame().m_mode == Flex::Mode1600_2), "polarity reports: last frame of either");
}

void testErrors()
{
    Enc enc;
    Enc::TxMessage tx;

    tx.m_capcode = 4321;
    tx.m_type = Flex::Alphanumeric;
    tx.m_text = "Bit errors in every codeword of this message";

    Enc::TxMessage tx2;

    tx2.m_capcode = 4322;
    tx2.m_type = Flex::Numeric;
    tx2.m_text = "0123456789";

    Dec::RxFrame clean = encoderFrame(enc, {tx, tx2});
    std::vector<uint32_t> cw = codewords(clean.m_phases[0]);

    // Words in use: until the last word that is not idle
    int used = 0;
    {
        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(clean);
        check(out.size() == 2, "errors: clean frame decodes");
        const Dec::BlockInfo& b = d.lastFrame().m_blocks[0];
        used = (int) b.m_vectorStart + 8;
    }

    std::mt19937 rng(777);

    // One and two bit errors in every codeword of the used part (bits 1-31)
    for (int errors = 1; errors <= 2; errors++)
    {
        Dec::RxFrame bad = clean;
        std::vector<std::pair<int, uint32_t>> xors;

        for (int w = 0; w < used; w++)
        {
            uint32_t mask = 0;

            while (__builtin_popcount(mask) < errors) {
                mask |= 1u << (1 + rng() % 31);
            }

            xors.push_back({w, mask});
        }

        editWords(bad.m_phases[0], xors);

        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(bad);
        bool ok = (out.size() == 2);
        int corrected = 0;

        for (const Dec::Message& m : out)
        {
            ok = ok && m.m_complete && (m.m_wordsUncorrectable == 0) && (m.m_checksum == Dec::CheckOk);
            corrected += m.m_wordsCorrected;

            if (m.m_typeName == "alphanumeric") {
                ok = ok && (m.m_text == tx.m_text) && (m.m_signature == Dec::CheckOk);
            } else {
                ok = ok && (m.m_text == tx2.m_text);
            }
        }

        char buf[96];
        std::snprintf(buf, sizeof(buf), "errors: %d bit error(s) in every codeword corrected (%d words in the messages)", errors, corrected);
        check(ok && (corrected > 0) && (d.status().m_codewordsCorrected >= (uint32_t) used), buf);
    }

    // Wrong parity bit alone
    {
        Dec::RxFrame bad = clean;
        std::vector<std::pair<int, uint32_t>> xors;

        for (int w = 0; w < used; w++) {
            xors.push_back({w, 1u});
        }

        editWords(bad.m_phases[0], xors);

        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(bad);
        bool ok = (out.size() == 2);

        for (const Dec::Message& m : out) {
            ok = ok && (m.m_wordsUncorrectable == 0) && (m.m_parityCorrections == m.m_wordsCorrected) && (m.m_wordsCorrected > 0);
        }

        check(ok, "errors: wrong parity bit alone is corrected and counted as parity");
    }

    // Two bit errors, one of them the parity bit: within the correction power of the code (a wrong parity after
    // correcting one bit in bits 31-1 is the second error)
    {
        Dec::RxFrame bad = clean;
        std::vector<std::pair<int, uint32_t>> xors;

        for (int w = 0; w < used; w++) {
            xors.push_back({w, 1u | (1u << (1 + (w % 31)))});
        }

        editWords(bad.m_phases[0], xors);

        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(bad);
        bool ok = (out.size() == 2);

        for (const Dec::Message& m : out)
        {
            ok = ok && m.m_complete && (m.m_wordsUncorrectable == 0) && (m.m_checksum == Dec::CheckOk);
            ok = ok && (m.m_text == ((m.m_typeName == "alphanumeric") ? tx.m_text : tx2.m_text));
        }

        check(ok && (d.status().m_codewordsUncorrectable == 0), "errors: a bit error and a wrong parity bit in every codeword corrected");
    }

    // Three bit errors in one word: not corrected, the text shows lost characters
    {
        Dec::RxFrame bad = clean;
        int target = -1;
        const Dec::BlockInfo *blk;
        Dec probe;

        probe.decodeFrame(clean);
        blk = &probe.lastFrame().m_blocks[0];

        // Message field word: first word at or after the end of the vector field
        target = (int) blk->m_vectorStart + 2 + 1;

        // A pattern of bit errors that the code does not correct
        uint32_t mask = 0x00000e00u;

        while (true)
        {
            RadioPagingBCH::Result r = RadioPagingBCH::decode(cw[target] ^ mask);

            if (r.m_bitErrors < 0) {
                break;
            }

            mask += 0x00010400u;
        }

        Dec::RxFrame bad2 = clean;
        editWords(bad2.m_phases[0], {{target, mask}});

        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(bad2);
        bool found = false;

        for (const Dec::Message& m : out)
        {
            if (m.m_typeName == "alphanumeric")
            {
                found = true;
                // The characters of the word are its received bits, marked uncertain ('u')
                const uint32_t received = RadioPagingFlex::wordData(cw[target] ^ mask);
                const size_t first = m.m_charStatus.find('u');
                const size_t end = (first == std::string::npos) ? first : m.m_charStatus.find_first_not_of('u', first);
                const size_t count = (first == std::string::npos) ? 0 : (end == std::string::npos ? m.m_charStatus.size() : end) - first;
                bool asReceived = (count == 2) || (count == 3);

                for (size_t k = 0; asReceived && (k < count); k++)
                {
                    const int slot = 3 - (int) count + (int) k;
                    asReceived = (m.m_text[first + k] == (char) ((received >> (7 * slot)) & 0x7f));
                }

                check((m.m_wordsUncorrectable == 1) && asReceived && (m.m_signature != Dec::CheckOk)
                    && (m.m_checksum == Dec::CheckFailed) && (m.m_wordMap.find('X') != std::string::npos),
                    "errors: uncorrectable word reported, its characters as received, marked uncertain, checksum not ok");
            }
        }

        check(found && (d.status().m_codewordsUncorrectable == 1), "errors: uncorrectable counter");

        // The same word with one of its error bits the least reliable of the frame: soft decision gives the word
        // sent, its characters still marked uncertain and the word counted uncorrectable
        {
            Dec::RxFrame soft = bad2;
            const int block = target / 8;
            const int k = target % 8;
            int errorBit = 0;

            while (((mask >> errorBit) & 1) == 0) {
                errorBit++;
            }

            const int j = 31 - errorBit;
            soft.m_phases[0].m_reliability.assign(32 * 88, 200);
            soft.m_phases[0].m_reliability[(block * 8 + j / 4) * 32 + 8 * (j % 4) + k] = 5;

            Dec ds;
            bool softOk = false;

            for (const Dec::Message& m : ds.decodeFrame(soft))
            {
                if (m.m_typeName == "alphanumeric") {
                    softOk = (m.m_text == tx.m_text) && (m.m_charStatus.find('u') != std::string::npos) && (m.m_wordsUncorrectable == 1);
                }
            }

            check(softOk, "errors: soft decision of an uncorrectable word gives the word sent, marked uncertain");
        }

        // Erased codewords
        Dec::RxFrame erased = clean;
        erased.m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);
        erased.m_phases[0].m_erased[target] = 1;

        Dec e;
        out = e.decodeFrame(erased);
        found = false;

        for (const Dec::Message& m : out)
        {
            if (m.m_typeName == "alphanumeric") {
                found = (m.m_wordsUncorrectable == 1) && (m.m_text.find('?') != std::string::npos);
            }
        }

        check(found && (e.status().m_codewordsErased == 1), "errors: erased codeword handled like a lost one");

        // Lost alphanumeric header: the first slot is the signature when it checks, else a lost character.
        // The header is the first word of the message field
        int header = (int) blk->m_vectorStart + 2;

        for (bool charLost : {false, true})
        {
            Dec::RxFrame hdr = clean;
            hdr.m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);
            hdr.m_phases[0].m_erased[header] = 1;
            if (charLost) {
                hdr.m_phases[0].m_erased[header + 3] = 1;
            }

            Dec h;
            out = h.decodeFrame(hdr);
            found = false;

            for (const Dec::Message& m : out)
            {
                if ((m.m_typeName != "alphanumeric") || (m.m_messageNumber != -1)) {
                    continue;
                }
                found = true;
                if (!charLost) {
                    check((m.m_text == tx.m_text) && (m.m_signature == Dec::CheckOk),
                        "errors: lost alphanumeric header, signature slot taken as the signature it checks as");
                } else {
                    check((m.m_text.size() == tx.m_text.size() + 1) && (m.m_text[0] == '?') && (m.m_charStatus[0] == 'X')
                        && (m.m_text.substr(1, 2) == tx.m_text.substr(0, 2)),
                        "errors: lost alphanumeric header and a character word, signature slot shown as lost");
                }
            }

            check(found, charLost ? "errors: lost header and character word, message decoded" : "errors: lost header, message decoded");
        }

        // Erased address word: the message is lost, the others stay
        Dec::RxFrame addr = clean;
        addr.m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);
        addr.m_phases[0].m_erased[1] = 1;

        Dec a;
        out = a.decodeFrame(addr);
        check(out.size() == 1 && out[0].m_capcode == 4322, "errors: lost address word loses only that message");

        // Lost block information word
        Dec::RxFrame biw = clean;
        biw.m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);
        biw.m_phases[0].m_erased[0] = 1;

        Dec b;
        out = b.decodeFrame(biw);
        check(out.empty() && !b.lastFrame().m_blocks[0].m_valid, "errors: lost first block information word, no messages");
    }

    // Frame information word
    {
        Dec::RxFrame bad = clean;
        Dec d;

        bad.m_fiw ^= 0x00000600u;
        check(d.decodeFrame(bad).size() == 2 && d.lastFrame().m_fiwValid, "errors: two bit errors in the frame information word corrected");

        bad.m_fiw = clean.m_fiw ^ 0x0420a000u;
        check(d.decodeFrame(bad).empty() && (d.status().m_fiwFailures == 1), "errors: bad frame information word, frame dropped");

        // Valid codeword with a wrong checksum
        bad.m_fiw = Flex::encodeWord(0x12340);
        check(d.decodeFrame(bad).empty() && (d.status().m_fiwFailures == 2), "errors: frame information word checksum");

        Dec::RxFrame noFiw = clean;
        noFiw.m_hasFiw = false;
        noFiw.m_cycle = 14;
        noFiw.m_frame = 127;
        check(d.decodeFrame(noFiw).size() == 2 && d.lastFrame().m_cycle == 14 && d.lastFrame().m_frame == 127,
            "frame number given without frame information word");
    }
}

// Fragments, duplicates and timeouts with the frames of the reference transmitter
const RefFrame *findFrame(const char *scenario, const char *name)
{
    for (size_t i = 0; i < refFrameCount; i++)
    {
        if (!std::strcmp(refFrames[i].m_scenario, scenario) && !std::strcmp(refFrames[i].m_name, name)) {
            return &refFrames[i];
        }
    }

    return nullptr;
}

Dec::RxFrame rxOf(const char *scenario, const char *name, int cycleShift = 0)
{
    const RefFrame *f = findFrame(scenario, name);
    Dec::RxFrame rx = toRxFrame((Flex::Mode) f->m_mode, false, std::vector<uint8_t>(), fromHex(f->m_data));

    rx.m_hasFiw = false;
    rx.m_cycle = (f->m_cycle + cycleShift) % 15;
    rx.m_frame = f->m_frame;
    return rx;
}

void testFragments()
{
    // Lost middle fragment: gap detected, signature fails
    {
        Dec d;
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        d.decodeFrame(rxOf("fragments", "fragment 1"));
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("fragments", "fragment 2"));
        check(out.size() == 1 && out[0].m_complete && (out[0].m_fragmentCount == 3) && (out[0].m_signature == Dec::CheckOk)
            && (out[0].m_fragmented) && (out[0].m_fragment == Dec::FragmentLast) && (out[0].m_messageNumber == 9),
            "fragments: three fragments reassembled, signature of the whole message ok");
    }
    {
        Dec d;
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("fragments", "fragment 2"));
        check(out.size() == 1 && !out[0].m_complete && out[0].m_fragmentGap && (out[0].m_fragmentCount == 2) && (out[0].m_signature == Dec::CheckFailed),
            "fragments: lost fragment shows as gap, message incomplete");
    }
    {
        Dec d;
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        d.decodeFrame(rxOf("fragments", "fragment 1"));
        std::vector<Dec::Message> out = d.flush();
        check(out.size() == 1 && !out[0].m_complete && (out[0].m_fragmentCount == 2) && !out[0].m_text.empty(),
            "fragments: flush returns the partial message marked incomplete");
        check(d.flush().empty(), "fragments: flush twice returns nothing");
    }
    {
        Dec d;
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        // 100 frames later (another cycle): timeout
        Dec::RxFrame later = rxOf("fragments", "long fragment 0");
        later.m_frame = 120;
        std::vector<Dec::Message> out = d.decodeFrame(later);
        check(out.size() == 1 && !out[0].m_complete && (out[0].m_capcode == 2500), "fragments: timeout releases the partial message");
    }
    {
        Dec d;
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("fragments", "fragment 2"));
        check(out.size() == 1 && !out[0].m_complete && out[0].m_fragmented, "fragments: last fragment without the first is an incomplete message");
    }
    {
        Dec d;
        d.setReportFragments(true);
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        std::vector<Dec::Message> a = d.decodeFrame(rxOf("fragments", "fragment 1"));
        std::vector<Dec::Message> b = d.decodeFrame(rxOf("fragments", "fragment 2"));
        check(a.size() == 1 && a[0].m_isFragment && b.size() == 2 && b[0].m_isFragment && !b[1].m_isFragment, "fragments: report fragments option");
    }
    {
        // A middle fragment lost: its place in the text is a lost fragment of unknown length
        Dec d;
        d.decodeFrame(rxOf("fragments", "fragment 0"));
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("fragments", "fragment 2"));
        size_t gap = out.empty() ? std::string::npos : out[0].m_text.find(Dec::m_gapCode);
        check((out.size() == 1) && !out[0].m_complete && (gap != std::string::npos) && (out[0].m_charStatus.size() == out[0].m_text.size())
            && (out[0].m_charStatus[gap] == 'G'), "fragments: a lost middle fragment is marked in the text");
    }
    {
        // A complete message of an address with an open stream: the address may have one while it sends a fragmented
        // message (STD-43A 3.8.5 (3), 4.2.3), the stream stays open
        Dec d;
        d.decodeFrame(rxOf("fragments", "open stream"));
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("fragments", "complete during stream"));
        std::vector<Dec::Message> rest = d.flush();
        check(out.size() == 1 && out[0].m_complete && (out[0].m_text == "complete message") && (rest.size() == 1) && !rest[0].m_complete
            && (rest[0].m_text == std::string("stream start, ") + Dec::m_gapCode) && (rest[0].m_charStatus.back() == 'G')
            && (rest[0].m_charStatus.size() == rest[0].m_text.size()),
            "fragments: a complete message of the address does not abandon an open stream");
    }
}

void testDuplicates()
{
    {
        Dec d;
        d.decodeFrame(rxOf("duplicates", "original"));
        std::vector<Dec::Message> out = d.decodeFrame(rxOf("duplicates", "retransmission"));
        bool marked = (out.size() == 3);
        for (const Dec::Message& m : out) {
            marked = marked && m.m_duplicate && !m.m_recovered;
        }
        check(marked && (d.status().m_duplicates == 3), "duplicates: retransmission (R = 0) of complete messages output again, marked duplicate");
        out = d.decodeFrame(rxOf("duplicates", "same number other message"));
        check(out.size() == 1, "duplicates: same message number with other content is a new message");
    }
    {
        // First copy with a lost word, retransmission repairs it
        Dec::RxFrame first = rxOf("duplicates", "original");
        Dec probe;
        probe.decodeFrame(first);
        std::vector<Dec::Message> clean;
        Dec::RxFrame bad = first;
        bad.m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);

        int body = (int) probe.lastFrame().m_blocks[0].m_vectorStart + 3 + 2;

        bad.m_phases[0].m_erased[body] = 1;

        Dec d;
        std::vector<Dec::Message> a = d.decodeFrame(bad);
        bool lost = false;

        for (const Dec::Message& m : a) {
            lost = lost || (m.m_wordsUncorrectable > 0);
        }

        std::vector<Dec::Message> b = d.decodeFrame(rxOf("duplicates", "retransmission"));
        bool repaired = false;

        for (const Dec::Message& m : b) {
            repaired = repaired || (m.m_recovered && (m.m_wordsUncorrectable == 0) && (m.m_text == "retransmitted message"));
        }

        check(lost && repaired && (d.status().m_recovered == 1), "duplicates: retransmission repairs lost words of the first copy");
        (void) clean;
    }
    {
        // Polarities are separate networks
        Dec d;
        d.decodeFrame(rxOf("duplicates", "original"));
        Dec::RxFrame inv = rxOf("duplicates", "retransmission");
        inv.m_inverted = true;
        std::vector<Dec::Message> out = d.decodeFrame(inv);
        bool fresh = (out.size() == 3);
        for (const Dec::Message& m : out) {
            fresh = fresh && !m.m_duplicate;
        }
        check(fresh, "duplicates: history is kept per polarity (same messages on the other polarity are not duplicates)");
    }
    {
        // A network without message numbering: every message has N = 0 and R = 0. Another message of the length of
        // the one before it is not a retransmission; the same message sent again is
        Enc enc;
        std::string error;
        Enc::Numbering numbering;
        numbering.m_enabled = false;
        enc.setNumbering(numbering, error);
        Dec dec;
        std::vector<Dec::Message> out;
        uint64_t index = 0;
        for (const char *text : {"message one", "message two", "message two"})
        {
            // One message a frame
            Enc::TxMessage m;
            m.m_capcode = 4096;
            m.m_text = text;
            enc.enqueue(m);
            for (; (index < 400) && (enc.pending() > 0); index++)
            {
                Enc::Frame f = enc.buildFrame(index);
                Dec::RxFrame rx = toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data);
                std::vector<Dec::Message> o = dec.decodeFrame(rx);
                out.insert(out.end(), o.begin(), o.end());
            }
        }
        bool ok = (out.size() == 3) && (out[0].m_messageNumber == 0) && !out[0].m_retrieval && !out[0].m_duplicate
            && !out[1].m_duplicate && out[2].m_duplicate && (out[2].m_text == "message two");
        std::string why;
        for (const Dec::Message& m : out) {
            why += " [" + m.m_text + " N" + std::to_string(m.m_messageNumber) + " d" + std::to_string(m.m_duplicate) + "]";
        }
        check(ok, ("duplicates: N = 0 R = 0 of a network without numbering, another message of the same length is new, the same one a duplicate"
            + why).c_str());
    }
}

void testGroup()
{
    Dec d;
    std::vector<Dec::Message> setup = d.decodeFrame(rxOf("group", "setup"));
    bool ok = (setup.size() == 5);

    for (const Dec::Message& m : setup) {
        ok = ok && (m.m_type == Dec::TypeShortInstruction) && (m.m_instructionType == 0) && m.m_groupTracked
            && ((m.m_instructionSlot == 5) || (m.m_instructionSlot == 6)) && (m.m_instructionFrame == ((m.m_instructionSlot == 5) ? 20 : 18));
    }

    check(ok, "group: assignments decoded as short instructions with slot and frame");

    std::vector<Dec::Message> delivery = d.decodeFrame(rxOf("group", "delivery alpha"));
    check(delivery.size() == 1 && delivery[0].m_groupTracked && (delivery[0].m_tempSlot == 5) && (delivery[0].m_groupMembers.size() == 3)
        && (delivery[0].m_groupMembers[0] == 3000) && (delivery[0].m_groupMembers[2] == 3002) && (delivery[0].m_addressType == Dec::AddressTemporary),
        "group: delivery message carries the members of the group");

    delivery = d.decodeFrame(rxOf("group", "delivery numeric"));
    check(delivery.size() == 1 && delivery[0].m_groupTracked && delivery[0].m_groupMembers.size() == 2, "group: second group");

    // Assignment expires without delivery
    Dec e;
    e.decodeFrame(rxOf("group", "setup"));
    Dec::RxFrame late = rxOf("group", "delivery alpha");
    late.m_cycle = (late.m_cycle + 2) % 15;
    delivery = e.decodeFrame(late);
    check(delivery.size() == 1 && !delivery[0].m_groupTracked, "group: assignment expires after 128 frames");

    // Delivery torn down: the next one is not tracked
    delivery = d.decodeFrame(rxOf("group", "delivery alpha"));
    check(delivery.size() == 1 && !delivery[0].m_groupTracked, "group: assignment ends with the group message");

    // The other polarity is another network with its own frame numbering: its frames do not
    // expire the assignments of this one
    Dec g;
    Dec::RxFrame invertedSetup = rxOf("group", "setup");
    invertedSetup.m_inverted = true;
    g.decodeFrame(invertedSetup);
    Dec::RxFrame otherNetwork = rxOf("group", "delivery numeric");
    otherNetwork.m_cycle = (otherNetwork.m_cycle + 2) % 15;
    g.decodeFrame(otherNetwork);
    Dec::RxFrame invertedDelivery = rxOf("group", "delivery alpha");
    invertedDelivery.m_inverted = true;
    delivery = g.decodeFrame(invertedDelivery);
    check(delivery.size() == 1 && delivery[0].m_groupTracked && (delivery[0].m_groupMembers.size() == 3) && delivery[0].m_inverted,
        "group: a frame of the other polarity 256 frames later does not expire the assignments of this one");

    // The state of each slot: assigning in the frame of the assignments, delivered after the group message, missed when
    // the assignment expired without it
    Dec s;
    s.decodeFrame(rxOf("group", "setup"));
    std::vector<Dec::GroupSlotState> states = s.groupStates(false);
    check((states.size() == 16) && (states[5].m_state == Dec::GroupAssigning) && (states[5].m_members == 3) && (states[5].m_targetFrame == 20)
        && (states[6].m_state == Dec::GroupAssigning) && (states[0].m_state == Dec::GroupFree),
        "group states: the slots assigned in the frame are assigning, the others free");
    s.decodeFrame(rxOf("group", "delivery alpha"));
    states = s.groupStates(false);
    check((states[5].m_state == Dec::GroupDelivered) && (states[5].m_members == 3) && (states[6].m_state != Dec::GroupFree)
        && (states[6].m_state != Dec::GroupDelivered), "group states: delivered after the group message, the other still waiting");
    check(s.groupStates(true)[5].m_state == Dec::GroupFree, "group states: the other polarity is another network");

    Dec x;
    x.decodeFrame(rxOf("group", "setup"));
    Dec::RxFrame after = rxOf("group", "delivery numeric");
    after.m_cycle = (after.m_cycle + 2) % 15;
    x.decodeFrame(after);
    states = x.groupStates(false);
    check(states[5].m_state == Dec::GroupMissed, "group states: missed when the assignment expires without its group message");
    check(std::string(Dec::groupStateName(Dec::GroupDiscarded)) == "discarded", "group states: names");
}

// Multiple transmission (STD-43A 3.4.2, Fig. 3.4.2-3), 2x with collapse 1: repeat interval 2
// frames. Subframe 1 of frame f is the first transmission of a content, subframe 2 of frame
// f + 2 its second transmission; the two subframes of one frame are different contents
void testSubframes()
{
    std::string error;
    Enc enc;
    Enc encB;
    Enc::TxMessage tx;
    Enc::TxMessage tx2;
    Enc::TxMessage txB;

    enc.setCollapse(1, error);
    encB.setCollapse(1, error);
    tx.m_capcode = 5000;
    tx.m_type = Flex::Alphanumeric;
    tx.m_text = "Subframe copies repair each other";
    tx2.m_capcode = 5001;
    tx2.m_type = Flex::Numeric;
    tx2.m_text = "424242";
    txB.m_capcode = 6000;
    txB.m_type = Flex::Alphanumeric;
    txB.m_text = "Another page";

    // Content A (two messages) and content B (one message), each fits a 44 word subframe
    Dec::RxFrame base = encoderFrame(enc, {tx, tx2});
    std::vector<uint32_t> a = codewords(base.m_phases[0]);
    std::vector<uint32_t> b = codewords(encoderFrame(encB, {txB}).m_phases[0]);
    std::vector<uint32_t> idle(44, Flex::encodeWord(0));

    a.resize(44);
    b.resize(44);
    idle[0] = Flex::blockInfoWord(0, 0, 1, 0, 1);   // No addresses

    const int target = 12;                          // A message word of content A

    // Frame of 2 subframes; erased word positions are frame positions
    auto frameOf = [&](unsigned frame, const std::vector<uint32_t>& sub1, const std::vector<uint32_t>& sub2,
        const std::vector<int>& erasedWords) {
        Dec::RxFrame f = base;
        std::vector<uint32_t> words(sub1);
        std::vector<uint8_t> erased(88, 0);

        words.insert(words.end(), sub2.begin(), sub2.end());
        for (int i : erasedWords) {
            erased[i] = 1;
        }
        f.m_hasFiw = false;
        f.m_cycle = 2;
        f.m_frame = frame;
        f.m_repeat = true;
        f.m_traffic = 1;                            // r = 1, t1 t0 = 01: 2 transmissions
        f.m_phases[0].m_words = interleaved(words);
        f.m_phases[0].m_erased = erased;
        return f;
    };
    auto count = [](const std::vector<Dec::Message>& out, uint64_t capcode) {
        int n = 0;
        for (const Dec::Message& m : out) {
            n += (m.m_capcode == capcode) ? 1 : 0;
        }
        return n;
    };
    auto lost = [](const std::vector<Dec::Message>& out) {
        int n = 0;
        for (const Dec::Message& m : out) {
            n += m.m_wordsUncorrectable;
        }
        return n;
    };

    {
        // A in frame 8, its second transmission in frame 10 next to the first of B
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, idle, {}));
        std::vector<Dec::Message> second = d.decodeFrame(frameOf(10, b, a, {}));
        check((first.size() == 2) && (lost(first) == 0) && (d.lastFrame().m_transmissions == 2)
            && (second.size() == 1) && (count(second, 6000) == 1),
            "subframes: a clean first transmission decodes at once, its second transmission is not output again");
    }
    {
        // Damaged A in subframe 1 and a clean B (second transmission, first one missed) in
        // subframe 2: they are not combined; B decodes from its copy, A waits for its own
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, b, {target}));
        bool waiting = d.lastFrame().m_blocks[0].m_waiting;
        std::vector<Dec::Message> second = d.decodeFrame(frameOf(10, idle, a, {}));
        check((first.size() == 1) && (count(first, 6000) == 1) && (lost(first) == 0) && waiting,
            "subframes: the two subframes of a frame are different contents, a content is decoded from its second transmission");
        check((second.size() == 2) && (count(second, 5000) == 1) && (lost(second) == 0),
            "subframes: the second transmission repairs the first, two frames later");
        bool frameOk = !second.empty();
        for (const Dec::Message& m : second) {
            frameOk = frameOk && (m.m_frame == 8);
        }
        check(frameOk, "subframes: the content is given the frame of its first transmission");
        bool marked = !first.empty() && !second.empty();
        for (const Dec::Message& m : first) {
            marked = marked && (m.m_transmissions == 2) && (m.m_copies == 1);
        }
        for (const Dec::Message& m : second) {
            marked = marked && (m.m_transmissions == 2) && (m.m_copies == 2);
        }
        check(marked, "subframes: a message tells it is a subframe of 2 transmissions, and the copies combined");
    }
    {
        // Words lost in each transmission are repaired by the other
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, idle, {target}));
        std::vector<Dec::Message> second = d.decodeFrame(frameOf(10, idle, a, {44 + target + 1}));
        check(first.empty() && (second.size() == 2) && (lost(second) == 0),
            "subframes: words lost in different transmissions are repaired by the other");
    }
    {
        // The same word lost in both transmissions: decoded at the last one with the loss
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, idle, {target}));
        std::vector<Dec::Message> second = d.decodeFrame(frameOf(10, idle, a, {44 + target}));
        check(first.empty() && (second.size() == 2) && (lost(second) == 1),
            "subframes: the same word lost in both transmissions stays lost");
    }
    {
        // Last transmission not received: decoded when it is due
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, idle, {target}));
        std::vector<Dec::Message> later = d.decodeFrame(frameOf(12, idle, idle, {}));
        check(first.empty() && (later.size() == 2) && (lost(later) == 1),
            "subframes: a content whose next transmission is missed is decoded with the words received");
    }
    {
        // Signal lost: flush decodes what was received
        Dec d;
        std::vector<Dec::Message> first = d.decodeFrame(frameOf(8, a, idle, {target}));
        std::vector<Dec::Message> f = d.flush();
        check(first.empty() && (f.size() == 2), "subframes: flush decodes the transmissions received");
    }
}

void testMisc()
{
    Dec d;

    check(std::string(Dec::messageTypeName(Dec::TypeNumberedSpecialNumeric)) == "numbered_special_numeric", "type names");
    check(std::string(Dec::addressTypeName(Dec::AddressOperator)) == "operator", "address type names");

    // Raw payload packing of a numeric message
    Enc enc;
    Enc::TxMessage tx;

    tx.m_capcode = 600;
    tx.m_type = Flex::Numeric;
    tx.m_text = "1234";

    std::vector<Dec::Message> out = d.decodeFrame(encoderFrame(enc, {tx}));

    check(out.size() == 1 && (out[0].m_rawBits == 21) && (out[0].m_rawData.size() == 3) && (out[0].m_address == "0000000600"),
        "raw payload: one numeric word, 21 bits in 3 bytes, address of 10 digits");

    if (out.size() == 1)
    {
        // Bits d0 first in the most significant bit: the digits 1234 are sent as 4 bit groups after 2 check bits
        uint32_t word = 0;

        for (int b = 0; b < 21; b++) {
            word |= (uint32_t) ((out[0].m_rawData[b / 8] >> (7 - b % 8)) & 1) << b;
        }

        check(((word >> 2) & 0xf) == 1 && ((word >> 6) & 0xf) == 2 && ((word >> 10) & 0xf) == 3 && ((word >> 14) & 0xf) == 4,
            "raw payload: bit order d0 first");
    }

    // Binary message
    const RefFrame *f = findFrame("binary", "hex 8 digits");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_blocking == 8) && (out[0].m_dataBits == 32) && out[0].m_signature == Dec::CheckOk
        && out[0].m_checksum == Dec::CheckOk, "binary: blocking, data bits, K checksum and signature");

    f = findFrame("binary", "hex 21 digits");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_blocking == 7) && out[0].m_displayRtl && out[0].m_headerMessage && !out[0].m_statusInfo,
        "binary: display direction and header flags");

    f = findFrame("types", "alpha mail drop");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && out[0].m_mailDrop && out[0].m_retrieval && out[0].m_checksum == Dec::CheckOk && out[0].m_signature == Dec::CheckOk,
        "alphanumeric: mail drop and retrieval flags, checksum and signature");

    f = findFrame("types", "numbered numeric");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && out[0].m_retrieval && !out[0].m_special && (out[0].m_checksum == Dec::CheckOk), "numbered numeric: flags and checksum");

    f = findFrame("short", "short numbered");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_shortType == 2) && (out[0].m_shortSource == 3) && out[0].m_retrieval && (out[0].m_messageNumber == 17),
        "short message: source, number and retrieval flag");

    f = findFrame("short", "instruction event");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_instructionType == 1) && (out[0].m_eventFlags == 3) && (out[0].m_addressType == Dec::AddressOperator)
        && (out[0].m_operatorSubtype == "system_event"), "short instruction: system event flags on an operator address");

    f = findFrame("special", "operator alpha");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_addressType == Dec::AddressOperator) && (out[0].m_operatorSubtype == "all")
        && (out[0].m_operatorCategory == "system_message"), "operator address: subtype and category");

    f = findFrame("special", "info service alpha");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_addressType == Dec::AddressInfoService), "information service address");

    f = findFrame("special", "network alpha");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.size() == 1 && (out[0].m_addressType == Dec::AddressNetwork) && (out[0].m_type == Dec::TypeAlphanumeric)
        && out[0].m_nidSystemMessage && !out[0].m_nidChange && !out[0].m_payloadUnknown,
        "network address: alphanumeric system message of the NID");

    f = findFrame("special", "reserved short");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.empty(), "reserved short address ignored");

    // A tone only address without a vector, as noise or the end of a damaged address field gives, is not decoded
    f = findFrame("types", "tone");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));
    check(out.empty(), "tone only address without a vector: no message");

    f = findFrame("mixed", "mixed priority");
    out = d.decodeFrame(toRxFrame((Flex::Mode) f->m_mode, false, fromHex(f->m_sync), fromHex(f->m_data)));

    int priority = 0;
    for (const Dec::Message& m : out) {
        priority += m.m_priority ? 1 : 0;
    }
    // Six addresses, two of them tone only addresses without a vector (not decoded)
    check(out.size() == 4 && priority == 2 && d.lastFrame().m_blocks[0].m_priorityWords == 2 && d.lastFrame().m_blocks[0].m_collapse == 2,
        "priority addresses flagged, collapse from the block information word");

    f = findFrame("mode 6400/4", "inverted");
    Dec::RxFrame rx = toRxFrame((Flex::Mode) f->m_mode, true, fromHex(f->m_sync), fromHex(f->m_data));
    out = d.decodeFrame(rx);
    check(out.size() == 1 && out[0].m_inverted && out[0].m_mode == Flex::Mode6400_4 && out[0].m_baud == 3200 && out[0].m_levels == 4
        && out[0].m_bitRate == 6400 && out[0].m_phase == 'A' && out[0].m_cycle == 5, "reception fields: mode, baud, levels, polarity, phase, frame");

    rx.m_quality = 0.25;
    out = d.decodeFrame(rx);
    check(!out.empty() && out[0].m_quality == 0.25, "frame quality copied to messages");

    d.reset();
    check(d.status().m_frames == 0 && d.status().m_codewords == 0, "reset clears the counters");
}

} // namespace

// ---------------------------------------------------------------------------
// Roaming (system ID words)
// ---------------------------------------------------------------------------

void testRoaming()
{
    // Bit layout of STD-43A table 3.7.2-1: the check of the words is the same as
    // any information word, so compare the data bits after decoding the codeword
    uint32_t w1 = Flex::ssid1Word(0x155, 0x0b);
    uint32_t w2 = Flex::ssid2Word(0x2a5, 0x9);
    uint32_t d1 = Flex::wordData(w1);
    uint32_t d2 = Flex::wordData(w2);
    bool ok1 = true, ok2 = true;

    check(ok1 && ((d1 >> 4) & 7) == 0 && ((d1 >> 7) & 0x1f) == 0x0b && ((d1 >> 12) & 0x1ff) == 0x155, "ssid1 word layout");
    check(ok2 && ((d2 >> 4) & 7) == 7 && ((d2 >> 7) & 0xf) == 0x9 && ((d2 >> 11) & 0x3ff) == 0x2a5, "ssid2 word layout");

    const Flex::Mode modes[] = {Flex::Mode1600_2, Flex::Mode3200_2, Flex::Mode3200_4, Flex::Mode6400_4};

    for (Flex::Mode mode : modes)
    {
        Enc enc;
        Dec dec;
        Enc::Roaming r;
        r.m_enabled = true;
        r.m_localId = 0x155;
        r.m_coverageZone = 0x0b;
        r.m_countryCode = 0x2a5;
        r.m_trafficFlags = 0x9;
        enc.setMode(mode);
        enc.setRoaming(r);

        // Messages in every frame so that the capacity with the extra words is exercised
        std::vector<Enc::TxMessage> sent;
        for (int i = 0; i < 12; i++)
        {
            Enc::TxMessage m;
            m.m_capcode = 1000000 + i * 7;
            m.m_type = Flex::Alphanumeric;
            m.m_text = "roaming message " + std::to_string(i);
            sent.push_back(m);
            enc.enqueue(m);
        }

        std::vector<Dec::Message> all;
        int phases = Flex::phaseCount(mode);
        bool layoutOk = true;
        bool flagOk = true;

        for (uint64_t index = 0; index < 12 || enc.pending() > 0; index++)
        {
            if (index > 4000) {
                break;
            }

            Enc::Frame f = enc.buildFrame(index);

            if (f.m_reserved) {
                continue;
            }

            std::vector<Dec::Message> out = dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
            all.insert(all.end(), out.begin(), out.end());
            const Dec::FrameReport& rep = dec.lastFrame();
            flagOk = flagOk && rep.m_roaming;

            unsigned frame = rep.m_frame;
            int expect = Flex::ssidPhase(mode, frame);
            int seen1 = 0, seen2 = 0;

            for (size_t b = 0; b < rep.m_blocks.size(); b++)
            {
                for (const Dec::ExtraBiw& e : rep.m_blocks[b].m_extra)
                {
                    bool here = ((int) b == expect);

                    if (e.m_kind == Dec::ExtraBiw::Ssid1)
                    {
                        seen1++;
                        layoutOk = layoutOk && here && (e.m_localId == 0x155) && (e.m_coverageZone == 0x0b);
                    }
                    else if (e.m_kind == Dec::ExtraBiw::Ssid2)
                    {
                        seen2++;
                        layoutOk = layoutOk && here && (e.m_country == 0x2a5) && (e.m_tmf == 0x9);
                    }
                    else
                    {
                        layoutOk = false;
                    }
                }
            }

            int want2 = (index <= 3) ? 1 : 0;
            layoutOk = layoutOk && (seen1 == 1) && (seen2 == want2) && ((int) rep.m_blocks.size() == phases);
        }

        char buf[128];
        std::snprintf(buf, sizeof(buf), "roaming %s: frame information flag", Flex::modeName(mode));
        check(flagOk, buf);
        std::snprintf(buf, sizeof(buf), "roaming %s: system ID words in frames and phases", Flex::modeName(mode));
        check(layoutOk, buf);

        std::vector<std::string> a, b;
        for (const Enc::TxMessage& m : sent) {
            a.push_back(std::to_string(m.m_capcode) + m.m_text);
        }
        for (const Dec::Message& m : all) {
            b.push_back(std::to_string(m.m_capcode) + m.m_text);
        }
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        std::snprintf(buf, sizeof(buf), "roaming %s: messages still delivered (%zu)", Flex::modeName(mode), all.size());
        check(a == b, buf);
    }

    // Low traffic flags (STD-43A 3.6): the address field ends in block 0. Frame 0 of 1600/2 with 6 tone only
    // addresses: BIW1 + 6 = 7 words fit in block 0; with roaming BIW1 + SSID1 + SSID2 + 6 = 9 do not
    for (bool roam : {false, true})
    {
        Enc enc;
        Dec dec;
        Enc::Roaming r;
        r.m_enabled = roam;
        enc.setRoaming(r);
        for (int i = 0; i < 6; i++)
        {
            Enc::TxMessage m;
            m.m_capcode = 1000 + i;
            m.m_type = Flex::Tone;
            enc.enqueue(m);
        }
        Enc::Frame f = enc.buildFrame(0);
        dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
        bool ok = (f.m_sentIds.size() == 6) && (dec.lastFrame().m_traffic == (roam ? 0u : 0xfu));
        check(ok, roam ? "low traffic flags: SSID words count in block 0, flags clear"
                       : "low traffic flags: 6 tone addresses in block 0, flags set");
    }

    // Off by default: no flag, no extra words
    Enc enc;
    Dec dec;
    Enc::Frame f = enc.buildFrame(0);
    dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
    check(!dec.lastFrame().m_roaming && dec.lastFrame().m_blocks[0].m_extra.empty(), "roaming off: no flag and no extra words");
}

// ---------------------------------------------------------------------------
// Short message vector (STD-43A 3.9.2): digits in the vector, no digits = tone only payload
// ---------------------------------------------------------------------------

// A message vector of no words (length 0): no header is read (the words at its start are another message's), the
// record is a message of no words whose raw data is the vector
void testVectorOfNoWords()
{
    for (Flex::MessageType type : {Flex::Secure, Flex::Alphanumeric, Flex::Binary})
    {
        Enc enc;
        Dec dec;
        Enc::TxMessage m;
        m.m_capcode = 924693;
        m.m_type = type;
        m.m_text = (type == Flex::Binary) ? "ABCD" : "SOME TEXT";
        Dec::RxFrame frame = encoderFrame(enc, {m});
        std::vector<uint32_t> words = codewords(frame.m_phases[0]);
        const uint32_t biw = Flex::wordData(words[0]);
        const int vectorIndex = (int) ((biw >> 10) & 0x3f);
        const uint32_t vector = Flex::wordData(words[vectorIndex]);
        // Length (bits 14-20) 0, the checksum again
        const uint32_t zeroLength = Flex::addChecksum((vector & 0x3fff) & ~0xfu);
        words[vectorIndex] = Flex::encodeWord(zeroLength);
        frame.m_phases[0].m_words = interleaved(words);
        std::vector<Dec::Message> out = dec.decodeFrame(frame);

        bool ok = (out.size() == 1);

        if (ok)
        {
            const Dec::Message& r = out[0];
            ok = (r.m_capcode == 924693) && r.m_noWords && r.m_text.empty() && (r.m_messageNumber < 0) && (r.m_rawBits == 21)
                && (r.m_type == ((type == Flex::Secure) ? Dec::TypeSecure : (type == Flex::Alphanumeric) ? Dec::TypeAlphanumeric : Dec::TypeBinary));
        }

        char buf[160];
        std::snprintf(buf, sizeof(buf), "a %s vector of no words: a message of no words, its vector as raw data, no header read",
            (type == Flex::Secure) ? "secure" : (type == Flex::Alphanumeric) ? "alphanumeric" : "binary");
        check(ok, buf);
    }

    // A vector word whose checksum fails is in error (an all zero word passes the code): no message from it; the
    // message of the next vector is decoded
    for (uint32_t bad : {0u, 0x00010u})
    {
        Enc enc;
        Dec dec;
        Enc::TxMessage first;
        first.m_capcode = 924693;
        first.m_type = Flex::Alphanumeric;
        first.m_text = "FIRST";
        Enc::TxMessage second = first;
        second.m_capcode = 924694;
        second.m_text = "SECOND";
        Dec::RxFrame frame = encoderFrame(enc, {first, second});
        std::vector<uint32_t> words = codewords(frame.m_phases[0]);
        const int vectorStart = (int) ((Flex::wordData(words[0]) >> 10) & 0x3f);
        const int addressStart = (int) ((Flex::wordData(words[0]) >> 8) & 3) + 1;
        // The vector of the first address
        int index = -1;

        for (int a = addressStart; a < vectorStart; a++)
        {
            if (Flex::wordData(words[a]) == 924693 + 32768) {
                index = vectorStart + (a - addressStart);
            }
        }

        words[index >= 0 ? index : vectorStart] = Flex::encodeWord(bad);
        frame.m_phases[0].m_words = interleaved(words);
        std::vector<Dec::Message> out = dec.decodeFrame(frame);
        check((index >= 0) && (out.size() == 1) && (out[0].m_capcode == 924694) && (out[0].m_text == "SECOND")
            && (dec.status().m_invalidVectors >= 1),
            bad ? "a vector word whose checksum fails: no message from it, the next one decoded"
                : "an all zero vector word (it passes the code, not the checksum): no message from it, the next one decoded");
    }
}

void testShortMessage()
{
    // Layout of Fig. 3.9.2-1: type 010, t = 00, digits a b c in d0-d11 (bits 9-20); second word d12-d31, d32 = 0
    uint32_t vw[2];
    Flex::shortMessageVector(Flex::ShortNumeric, "12", 0, 0, false, false, vw);
    uint32_t d = Flex::wordData(vw[0]);
    check(((d >> 4) & 7) == 2 && ((d >> 7) & 3) == 0 && ((d >> 9) & 0xfff) == 0xc21 && Flex::checksumValid(d),
        "short message vector layout: 1 2 space");
    Flex::shortMessageVector(Flex::ShortNumeric, "", 0, 0, false, true, vw);
    check(((Flex::wordData(vw[0]) >> 9) & 0xfff) == 0xccc && Flex::wordData(vw[1]) == 0xccccc,
        "short message vector, long address, no digits: all spaces, d32 = 0");
    Flex::shortMessageVector(Flex::ShortSource, "", 5, 0, false, true, vw);
    d = Flex::wordData(vw[0]);
    check(((d >> 7) & 3) == 1 && ((d >> 9) & 0xfff) == 5 && Flex::checksumValid(d) && (Flex::wordData(vw[1]) == 0),
        "short message vector, source: t = 01, S in d0-d2, not used bits 0");
    Flex::shortMessageVector(Flex::ShortNumbered, "", 6, 45, true, false, vw);
    d = Flex::wordData(vw[0]);
    check(((d >> 7) & 3) == 2 && ((d >> 9) & 0xfff) == (6u | (45u << 3) | (1u << 9)) && Flex::checksumValid(d),
        "short message vector, numbered: t = 10, S d0-d2, N d3-d8, R d9");

    struct Case { uint64_t m_capcode; const char *m_text; bool m_toneOnly; const char *m_digits; };
    const Case cases[] = {
        { 1234567, "", true, "   " },
        { 1234567, "42", false, "42 " },
        { 1234567, "020", false, "020" },     // bits 7-20 read as a start and a length give 0 and 0
        { 2101300, "", true, "        " },
        { 2101300, "12345678", false, "12345678" },
    };
    const Flex::Mode modes[] = {Flex::Mode1600_2, Flex::Mode6400_4};

    for (Flex::Mode mode : modes)
    {
        for (const Case& c : cases)
        {
            Enc enc;
            Dec dec;
            Enc::TxMessage m;
            m.m_capcode = c.m_capcode;
            m.m_type = Flex::ShortMessage;
            m.m_text = c.m_text;
            enc.setMode(mode);
            Enc::EnqueueResult er = enc.enqueue(m);
            std::vector<Dec::Message> out = runEncoder(enc, dec);
            bool ok = er.m_ok && (out.size() == 1);

            if (ok)
            {
                const Dec::Message& r = out[0];
                // The tone only payload is a tone only page
                ok = (r.m_capcode == c.m_capcode) && (r.m_type == (c.m_toneOnly ? Dec::TypeTone : Dec::TypeShortMessage))
                    && (r.m_shortType == 0) && (r.m_toneOnly == c.m_toneOnly) && (r.m_text == (c.m_toneOnly ? std::string() : c.m_digits));
            }

            char buf[128];
            std::snprintf(buf, sizeof(buf), "short message %s capcode %llu \"%s\": decoded, tone only %d", Flex::modeName(mode),
                (unsigned long long) c.m_capcode, c.m_text, c.m_toneOnly ? 1 : 0);
            check(ok, buf);
        }
    }

    // Source and numbered types, short and long address: the number is the one of the message numbering
    for (uint64_t capcode : {(uint64_t) 1234567, (uint64_t) 2101300})
    {
        for (Flex::ShortType type : {Flex::ShortSource, Flex::ShortNumbered})
        {
            Enc enc;
            Dec dec;
            Enc::TxMessage m;
            m.m_capcode = capcode;
            m.m_type = Flex::ShortMessage;
            m.m_shortType = type;
            m.m_source = 3;
            bool ok = enc.enqueue(m).m_ok;
            std::vector<Dec::Message> out;
            int number = -1;

            for (uint64_t index = 0; ok && (index < 200) && out.empty(); index++)
            {
                Enc::Frame f = enc.buildFrame(index);
                if (f.m_reserved) {
                    continue;
                }
                if (!f.m_sentNumbers.empty()) {
                    number = (int) f.m_sentNumbers[0];
                }
                out = dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
            }

            ok = ok && (out.size() == 1) && (out[0].m_type == Dec::TypeShortMessage) && (out[0].m_shortType == (int) type)
                && (out[0].m_shortSource == 3) && out[0].m_text.empty() && !out[0].m_toneOnly;
            if (ok && (type == Flex::ShortNumbered)) {
                ok = (out[0].m_messageNumber == number) && out[0].m_retrieval;
            }

            char buf[128];
            std::snprintf(buf, sizeof(buf), "short message %s, capcode %llu: decoded", (type == Flex::ShortSource) ? "source" : "numbered",
                (unsigned long long) capcode);
            check(ok, buf);
        }
    }

    // Network address with a short message vector t = 00: the network ID F3-F0 M2-M0 A4-A0 (Table 3.9.2-1 (1), 6.1.2)
    {
        uint32_t words[Flex::m_wordsPerFrame];
        Flex::fillIdle(words, 0, Flex::Mode1600_2);
        const uint32_t sa = 21, mult = 5, tmf = 0xa;
        words[0] = Flex::blockInfoWord(0, 0, 2, 0, 0);
        words[1] = Flex::encodeWord(0x1F6800u + 7);
        words[2] = Flex::encodeWord(Flex::addChecksum((Flex::m_vectorTone << 4) | ((sa | (mult << 5) | (tmf << 8)) << 9)));
        for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
            Flex::interleaveBlock(words + b * Flex::m_wordsPerBlock);
        }
        Flex::FrameParams fp;
        Flex::FrameBytes bytes = Flex::assemble(fp, {std::vector<uint32_t>(words, words + Flex::m_wordsPerFrame)});
        Dec dec;
        std::vector<Dec::Message> out = dec.decodeFrame(toRxFrame(Flex::Mode1600_2, false, bytes.m_sync, bytes.m_data));
        bool ok = (out.size() == 1) && (out[0].m_type == Dec::TypeShortMessage) && out[0].m_networkId
            && (out[0].m_nidServiceArea == (int) sa) && (out[0].m_nidMultiplier == (int) mult)
            && (out[0].m_nidTrafficFlags == (int) tmf) && out[0].m_text.empty() && !out[0].m_toneOnly;
        check(ok, "short message on a network address: network ID, service area, multiplier, traffic flags");
    }

    // The network address twice (FLEX-93001 3.9.2 p.44): the first with the NID, the second with a secure message
    // t = 01, the System NID Change Instruction (3.9.7 p.48). Its layout is not published: the data words as hex
    // On a short address the secure type 01 (data defined separately, STD-43A 3.10.1.4) is hex as well, not an NID
    // change. Type 10 is binary: the termination word (the inverse of the last data bit) is not data. The hex is the
    // bit stream, d0 of each word first, 4 bits a digit with the first bit the least significant
    struct SecureCase { bool m_network; uint32_t m_t; uint32_t m_data2; const char *m_hex; int m_bits; };
    const SecureCase secureCases[] = {
        { true, 1, 0x0abcde, "65432DB9751", 42 },
        { false, 1, 0x0abcde, "65432DB9751", 42 },
        { false, 2, 0x000000, "654321", 21 },
    };
    for (const SecureCase& sc : secureCases)
    {
        const bool network = sc.m_network;
        uint32_t words[Flex::m_wordsPerFrame];
        Flex::fillIdle(words, 0, Flex::Mode1600_2);
        const uint32_t net = 0x1F6800u + 7;
        const uint32_t data1 = 0x123456, data2 = sc.m_data2;
        uint32_t hdr = (3u << 11) | (sc.m_t << 19);     // C = 0, F = 3, N = 0, t
        auto sums = [](uint32_t w) { return (w & 0xff) + ((w >> 8) & 0xff) + ((w >> 16) & 0x1f); };
        hdr |= (~(sums(hdr) + sums(data1) + sums(data2))) & 0x3ff;
        words[0] = Flex::blockInfoWord(0, 0, 3, 0, 0);
        words[1] = Flex::encodeWord(net);
        words[2] = Flex::encodeWord(network ? net : (1234567u + 32768u));
        words[3] = Flex::encodeWord(Flex::addChecksum((Flex::m_vectorTone << 4) | ((21u | (5u << 5) | (0xau << 8)) << 9)));
        words[4] = Flex::encodeWord(Flex::addChecksum((0u << 4) | (5u << 7) | (3u << 14)));
        words[5] = Flex::encodeWord(hdr);
        words[6] = Flex::encodeWord(data1);
        words[7] = Flex::encodeWord(data2);
        for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
            Flex::interleaveBlock(words + b * Flex::m_wordsPerBlock);
        }
        Flex::FrameParams fp;
        Flex::FrameBytes bytes = Flex::assemble(fp, {std::vector<uint32_t>(words, words + Flex::m_wordsPerFrame)});
        Dec dec;
        std::vector<Dec::Message> out = dec.decodeFrame(toRxFrame(Flex::Mode1600_2, false, bytes.m_sync, bytes.m_data));
        bool ok = (out.size() == 2) && out[0].m_networkId && !out[0].m_nidSystemMessage
            && (out[1].m_type == Dec::TypeSecure) && (out[1].m_nidSystemMessage == network) && (out[1].m_nidChange == network)
            && (out[1].m_addressType == (network ? Dec::AddressNetwork : Dec::AddressShort))
            && (out[1].m_payloadUnknown == (sc.m_t != 2)) && (out[1].m_secureType == (int) sc.m_t)
            && (out[1].m_text == sc.m_hex) && (out[1].m_dataBits == sc.m_bits) && (out[1].m_hexNibbleBits == 4)
            && out[1].m_hexLsbFirst && (out[1].m_checksum == Dec::CheckOk) && (out[1].m_signature == Dec::CheckNone);
        check(ok, network ? "network address: NID, then the NID change instruction with its payload as hex"
                 : (sc.m_t == 1) ? "secure type 01 on a short address: payload as hex, no NID change"
                                 : "secure type 10: binary as hex, termination word removed");
    }

    // Fragments of a secure type 01 message: one bit stream (a fragment need not end on a digit); a lost fragment is '|'
    {
        auto secureFrame = [](uint32_t c, uint32_t f, uint32_t data) {
            uint32_t words[Flex::m_wordsPerFrame];
            Flex::fillIdle(words, 0, Flex::Mode1600_2);
            uint32_t hdr = (c << 10) | (f << 11) | (5u << 13) | (1u << 19);     // N = 5, t = 01
            auto sums = [](uint32_t w) { return (w & 0xff) + ((w >> 8) & 0xff) + ((w >> 16) & 0x1f); };
            hdr |= (~(sums(hdr) + sums(data))) & 0x3ff;
            words[0] = Flex::blockInfoWord(0, 0, 2, 0, 0);
            words[1] = Flex::encodeWord(1234567u + 32768u);
            words[2] = Flex::encodeWord(Flex::addChecksum((0u << 4) | (3u << 7) | (2u << 14)));
            words[3] = Flex::encodeWord(hdr);
            words[4] = Flex::encodeWord(data);
            for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
                Flex::interleaveBlock(words + b * Flex::m_wordsPerBlock);
            }
            Flex::FrameParams fp;
            Flex::FrameBytes bytes = Flex::assemble(fp, {std::vector<uint32_t>(words, words + Flex::m_wordsPerFrame)});
            return toRxFrame(Flex::Mode1600_2, false, bytes.m_sync, bytes.m_data);
        };

        for (bool lost : {false, true})
        {
            Dec dec;
            std::vector<Dec::Message> first = dec.decodeFrame(secureFrame(1, 3, 0x123456));
            std::vector<Dec::Message> last = dec.decodeFrame(secureFrame(0, lost ? 1 : 0, 0x0abcde));
            bool ok = first.empty() && (last.size() == 1) && (last[0].m_fragmentCount == 2)
                && (last[0].m_text == (lost ? "654321|EDCBA0" : "65432DB9751")) && (last[0].m_dataBits == 42)
                && (last[0].m_fragmentGap == lost) && (last[0].m_complete == !lost);
            check(ok, lost ? "secure hex fragments: a lost fragment is marked" : "secure hex fragments: joined as one bit stream");
        }
    }

    // Too many digits, characters that are not numeric, digits in the source type, source out of range
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::ShortMessage;
        m.m_shortType = Flex::ShortSource;
        m.m_text = "1";
        bool digitsRefused = !enc.enqueue(m).m_ok;
        m.m_text.clear();
        m.m_source = 8;
        bool sourceRefused = !enc.enqueue(m).m_ok;
        check(digitsRefused && sourceRefused, "short message source: digits and source 8 refused");
    }

    Enc enc;
    Enc::TxMessage m;
    m.m_capcode = 1234567;
    m.m_type = Flex::ShortMessage;
    m.m_text = "1234";
    bool tooLong = !enc.enqueue(m).m_ok;
    m.m_text = "1A";
    bool notNumeric = !enc.enqueue(m).m_ok;
    check(tooLong && notNumeric, "short message: 4 digits on a short address and letters refused");
}

// ---------------------------------------------------------------------------
// HEX/binary data bits, termination, K and signature (STD-43A 3.10.1.2)
// ---------------------------------------------------------------------------

namespace {

uint32_t testGroupSum(uint32_t w)
{
    return (w & 0xff) + ((w >> 8) & 0xff) + ((w >> 16) & 0x1f);
}

// Words of a HEX/binary fragment: header 1 (K, C, F, N), header 2 of a first fragment (B, S), data words
std::vector<uint32_t> hexFragmentWords(int c, int f, int n, int b, uint32_t signature, bool first, const std::vector<int>& dataBits)
{
    std::vector<uint32_t> words;
    uint32_t hdr1 = ((uint32_t) c << 12) | ((uint32_t) f << 13) | ((uint32_t) n << 15);

    words.push_back(hdr1);
    if (first) {
        words.push_back(((uint32_t) (b & 0xf) << 4) | (1u << 12) | (signature << 13));  // I = 1 as in the capture
    }
    for (size_t i = 0; i < dataBits.size(); i += 21)
    {
        uint32_t w = 0;
        for (size_t k = 0; (k < 21) && (i + k < dataBits.size()); k++) {
            w |= (uint32_t) dataBits[i + k] << k;
        }
        words.push_back(w);
    }

    uint32_t sum = 0;
    for (uint32_t w : words) {
        sum += testGroupSum(w);
    }
    words[0] |= (~sum) & 0xfff;
    return words;
}

Flex::FrameBytes hexFrameBytes(unsigned cycle, unsigned frame, const std::vector<uint32_t>& body)
{
    uint32_t words[Flex::m_wordsPerFrame];
    Flex::fillIdle(words, 0, Flex::Mode1600_2);
    words[0] = Flex::blockInfoWord(0, 0, 2, 0, 0);
    words[1] = Flex::encodeWord(1234567u + 32768u);
    words[2] = Flex::encodeWord(Flex::addChecksum((6u << 4) | (3u << 7) | ((uint32_t) body.size() << 14)));
    for (size_t i = 0; i < body.size(); i++) {
        words[3 + i] = Flex::encodeWord(body[i]);
    }
    for (int b = 0; b < Flex::m_blocksPerFrame; b++) {
        Flex::interleaveBlock(words + b * Flex::m_wordsPerBlock);
    }
    Flex::FrameParams fp;
    fp.m_cycle = cycle;
    fp.m_frame = frame;
    return Flex::assemble(fp, {std::vector<uint32_t>(words, words + Flex::m_wordsPerFrame)});
}

// Bits of bytes, first bit the least significant of a byte
std::vector<int> byteBits(const std::vector<uint8_t>& bytes)
{
    std::vector<int> bits;
    for (uint8_t v : bytes) {
        for (int k = 0; k < 8; k++) {
            bits.push_back((v >> k) & 1);
        }
    }
    return bits;
}

uint32_t testSignature(const std::vector<uint8_t>& bytes)
{
    uint32_t sum = 0;
    for (uint8_t v : bytes) {
        sum += v;
    }
    return (~sum) & 0xff;
}

// Data bits followed by the termination of 3.10.1.2: the inverse of the last bit to the end of the word, and a
// word of the inverse if the data end exactly at a word end and the last character is uniform (last fragment)
std::vector<int> terminated(std::vector<int> bits, int b, bool last)
{
    int inv = 1 - bits.back();
    bool charUniform = true;
    for (int k = 1; k <= b; k++) {
        charUniform = charUniform && (bits[bits.size() - k] == bits.back());
    }
    if ((bits.size() % 21) != 0)
    {
        while ((bits.size() % 21) != 0) {
            bits.push_back(inv);
        }
    }
    else if (last && charUniform)
    {
        bits.insert(bits.end(), 21, inv);
    }
    return bits;
}

} // namespace

void testHexBits()
{
    // One fragment, 8 bit characters; data that end mid-word, exactly at the end of a word with a
    // non-uniform last character (nothing added) and with a uniform one (an extra word)
    struct Case { const char *m_name; std::vector<uint8_t> m_data; };
    std::vector<uint8_t> exact(21, 0x5a);       // 168 bits: 8 words exactly
    std::vector<uint8_t> exactUniform(21, 0x5a);
    exactUniform.back() = 0xff;
    const Case cases[] = {
        { "mid-word end", {0x11, 0xc2, 0x10, 0x2c, 0x23} },
        { "exact word end, last character mixed", exact },
        { "exact word end, last character 0xff", exactUniform },
        { "exact word end, last character 0x00", std::vector<uint8_t>(20, 0x5a) },
    };

    for (const Case& c : cases)
    {
        std::vector<uint8_t> data = c.m_data;
        if (std::string(c.m_name).find("0x00") != std::string::npos) {
            data.insert(data.end(), 1, 0x00);
        }
        std::vector<int> bits = terminated(byteBits(data), 8, true);
        std::vector<uint32_t> body = hexFragmentWords(0, 3, 0, 8, testSignature(data), true, bits);
        Flex::FrameBytes bytes = hexFrameBytes(2, 10, body);
        Dec dec;
        std::vector<Dec::Message> out = dec.decodeFrame(toRxFrame(Flex::Mode1600_2, false, bytes.m_sync, bytes.m_data));
        bool ok = (out.size() == 1) && (out[0].m_type == Dec::TypeBinary) && (out[0].m_dataBits == (int) data.size() * 8)
            && (out[0].m_checksum == Dec::CheckOk) && (out[0].m_signature == Dec::CheckOk) && (out[0].m_fillState == 0)
            && (out[0].m_blocking == 8) && out[0].m_statusInfo;
        char buf[128];
        std::snprintf(buf, sizeof(buf), "hex bits: %s: data end, K and signature", c.m_name);
        check(ok, buf);
    }

    // Two fragments: the signature covers the data of both, the fragment ends need not be on a digit
    for (int lost = 0; lost < 2; lost++)
    {
        std::vector<uint8_t> data;
        for (int i = 0; i < 30; i++) {
            data.push_back((uint8_t) (0x13 * i + 7));
        }
        std::vector<int> all = byteBits(data);
        // The first fragment ends on a character boundary: 15 characters
        std::vector<int> frag1(all.begin(), all.begin() + 15 * 8);
        std::vector<int> frag2(all.begin() + 15 * 8, all.end());
        frag1 = terminated(frag1, 8, false);
        frag2 = terminated(frag2, 8, true);
        Dec dec;
        std::vector<Dec::Message> first = dec.decodeFrame(toRxFrame(Flex::Mode1600_2, false,
            hexFrameBytes(2, 20, hexFragmentWords(1, 3, 9, 8, testSignature(data), true, frag1)).m_sync,
            hexFrameBytes(2, 20, hexFragmentWords(1, 3, 9, 8, testSignature(data), true, frag1)).m_data));
        // F = 0 follows 3; with a fragment lost the next one is F = 1
        Flex::FrameBytes b2 = hexFrameBytes(2, 21, hexFragmentWords(0, lost ? 1 : 0, 9, 8, 0, false, frag2));
        std::vector<Dec::Message> last = dec.decodeFrame(toRxFrame(Flex::Mode1600_2, false, b2.m_sync, b2.m_data));
        bool ok = first.empty() && (last.size() == 1) && (last[0].m_fragmentCount == 2) && (last[0].m_dataBits == 240)
            && (last[0].m_signature == (lost ? Dec::CheckIncomplete : Dec::CheckOk))
            && (last[0].m_checksum == (lost ? Dec::CheckFailed : Dec::CheckOk))
            && (last[0].m_fragmentGap == (lost != 0));
        check(ok, lost ? "hex bits: fragments with a lost one: signature not checked, the checksum of the lost one not ok"
                       : "hex bits: two fragments, signature over both");
    }
}

// HEX/binary and secure messages of the encoder, decoded: text, data bits, header fields, K and signature
// The hex digits of HEX/binary and secure binary messages have a status, as characters do: . received, c in a
// corrected word, X lost ('?'). In one frame and over fragments
void testHexStatus()
{
    struct Case
    {
        const char *m_label;
        Flex::MessageType m_type;
        Flex::BinaryParams m_binary;
        std::string m_text;
    };

    Flex::BinaryParams hex;
    hex.m_blocking = 8;
    Flex::BinaryParams sec;
    sec.m_secureType = Flex::SecureBinary;
    std::string digits40;
    std::string digits600;

    for (int i = 0; i < 40; i++) {
        digits40 += "0123456789ABCDEF"[(i * 7) % 16];
    }
    for (int i = 0; i < 600; i++) {
        digits600 += "0123456789ABCDEF"[(i * 5) % 16];
    }

    const Case cases[] = {
        {"hex", Flex::Binary, hex, digits40},
        {"secure binary", Flex::Secure, sec, digits40},
        {"hex in fragments", Flex::Binary, hex, digits600},
    };

    for (const Case& c : cases)
    {
        Enc enc;
        Dec clean;
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = c.m_type;
        m.m_text = c.m_text;
        m.m_binary = c.m_binary;
        enc.enqueue(m);
        std::vector<Dec::RxFrame> frames;
        std::vector<Dec::Message> out = runEncoder(enc, clean, &frames);
        const std::string label = std::string("hex status, ") + c.m_label;

        check((out.size() == 1) && (out[0].m_text == c.m_text) && (out[0].m_charStatus == std::string(c.m_text.size(), '.')),
            (label + ": every digit received").c_str());

        // Each message word of each frame in turn: a bit error (corrected) or the word lost
        bool corrected = false;
        bool lost = false;
        bool consistent = true;

        for (size_t f = 0; f < frames.size(); f++)
        {
            for (int w = 0; w < Flex::m_wordsPerFrame; w++)
            {
                for (bool erase : {false, true})
                {
                    std::vector<Dec::RxFrame> damaged = frames;

                    if (erase)
                    {
                        damaged[f].m_phases[0].m_erased.assign(Flex::m_wordsPerFrame, 0);
                        damaged[f].m_phases[0].m_erased[w] = 1;
                    }
                    else
                    {
                        editWords(damaged[f].m_phases[0], {{w, 0x00000400u}});
                    }

                    Dec d;
                    std::vector<Dec::Message> r;

                    for (const Dec::RxFrame& rx : damaged)
                    {
                        std::vector<Dec::Message> o = d.decodeFrame(rx);
                        r.insert(r.end(), o.begin(), o.end());
                    }
                    for (const Dec::Message& msg : r)
                    {
                        if ((msg.m_type != Dec::TypeBinary) && (msg.m_type != Dec::TypeSecure)) {
                            continue;
                        }

                        consistent = consistent && (msg.m_charStatus.size() == msg.m_text.size());

                        for (size_t i = 0; consistent && (i < msg.m_text.size()); i++)
                        {
                            // A lost digit is '?' and X; a lost fragment '|' and G; the others the digit sent
                            const char s = msg.m_charStatus[i];
                            consistent = (msg.m_text[i] == '?') ? (s == 'X') : (msg.m_text[i] == '|') ? (s == 'G') : (s != 'X');
                        }

                        const size_t cCount = (size_t) std::count(msg.m_charStatus.begin(), msg.m_charStatus.end(), 'c');

                        if (!erase && (msg.m_text == c.m_text) && (cCount >= 5) && (cCount <= 7)) {
                            corrected = true;
                        }
                        if (erase && (msg.m_text.find('?') != std::string::npos)) {
                            lost = true;
                        }
                    }
                }
            }
        }

        check(corrected, (label + ": the digits of a corrected word are c, the text unchanged").c_str());
        check(lost, (label + ": the digits of a lost word are X").c_str());
        check(consistent, (label + ": a status for each digit, X for each '?' and only for it").c_str());
    }
}

void testBinaryTx()
{
    struct Case
    {
        const char *m_label;
        uint64_t m_capcode;
        Flex::MessageType m_type;
        const char *m_text;
        Flex::BinaryParams m_binary;
        int m_words;            // Message words, headers and termination included
        int m_dataBits;
    };

    auto hexParams = [](int blocking, bool m, bool d, bool h, bool i) {
        Flex::BinaryParams p;
        p.m_blocking = blocking;
        p.m_mailDrop = m;
        p.m_rightToLeft = d;
        p.m_header = h;
        p.m_statusInfo = i;
        return p;
    };
    auto secParams = [](Flex::SecureType t) {
        Flex::BinaryParams p;
        p.m_secureType = t;
        return p;
    };

    const Case cases[] = {
        // 64 bits end in word 4: fill of 20 bits
        {"hex B=8 fill", 1234567, Flex::Binary, "0123456789ABCDEF", hexParams(8, false, false, false, false), 2 + 4, 64},
        // 84 bits end exactly at a word end, last character 1111: padding word
        {"hex B=4 exact end, uniform last character: padding word", 1234567, Flex::Binary, "123456789ABCDEF12345F",
            hexParams(4, false, false, false, false), 2 + 4 + 1, 84},
        // 84 bits end exactly at a word end, last character 1010: nothing added
        {"hex B=4 exact end, last character not uniform", 1234567, Flex::Binary, "123456789ABCDEF123455",
            hexParams(4, false, false, false, false), 2 + 4, 84},
        // Last character all 0s, ended in the middle of a word: fill of 1s longer than a character
        {"hex B=16 last character all 0s, fill", 1234567, Flex::Binary, "ABCD1234FFFF0000", hexParams(16, false, false, false, false),
            2 + 4, 64},
        {"hex B=1 M H I", 1234567, Flex::Binary, "A5", hexParams(1, true, false, true, true), 2 + 1, 8},
        {"hex B=7 right to left", 1234567, Flex::Binary, "1234567", hexParams(7, false, true, false, false), 2 + 2, 28},
        {"hex long address", 2101300, Flex::Binary, "DEADBEEF", hexParams(8, false, false, false, false), 2 + 2, 32},
        {"secure alphanumeric", 1234567, Flex::Secure, "Hello", secParams(Flex::SecureAlphanumeric), 1 + 2, 0},
        {"secure alphanumeric, last word full", 1234567, Flex::Secure, "ABC", secParams(Flex::SecureAlphanumeric), 1 + 1, 0},
        {"secure alphanumeric long address", 2101300, Flex::Secure, "secure text", secParams(Flex::SecureAlphanumeric), 1 + 4, 0},
        {"secure binary fill", 1234567, Flex::Secure, "DEADBEEF", secParams(Flex::SecureBinary), 1 + 2, 32},
        {"secure binary exact end: termination word", 1234567, Flex::Secure, "123456789ABCDEF123455",
            secParams(Flex::SecureBinary), 1 + 4 + 1, 84},
        {"secure data defined separately", 1234567, Flex::Secure, "0123", secParams(Flex::SecureSeparate), 1 + 1, 21},
    };

    for (const Case& c : cases)
    {
        Enc enc;
        Dec dec;
        Enc::TxMessage m;
        m.m_capcode = c.m_capcode;
        m.m_type = c.m_type;
        m.m_text = c.m_text;
        m.m_binary = c.m_binary;

        Flex::PhaseMessage pm;
        pm.m_capcode = m.m_capcode;
        pm.m_type = m.m_type;
        pm.m_text = m.m_text;
        pm.m_binary = m.m_binary;
        bool isLong = Flex::isLongAddress(c.m_capcode);
        int cost = Flex::messageCost(pm);
        // Address and vector words; the first message word of a long address is in the second vector word
        int expectedCost = (isLong ? 4 : 2) + c.m_words - (isLong ? 1 : 0);

        Enc::EnqueueResult er = enc.enqueue(m);
        std::vector<Dec::Message> out = runEncoder(enc, dec);
        bool ok = er.m_ok && (cost == expectedCost) && (out.size() == 1);
        std::string why = er.m_ok ? "" : er.m_error;

        if (ok)
        {
            const Dec::Message& r = out[0];
            // The first number of the sequence is 0; a secure message is not numbered: the lowest number that is
            // not the next one of the sequence
            ok = (r.m_capcode == c.m_capcode) && (r.m_checksum == Dec::CheckOk) && (r.m_messageNumber == ((c.m_type == Flex::Secure) ? 1 : 0))
                && (r.m_fragment == Dec::FragmentComplete);

            if (c.m_type == Flex::Binary)
            {
                int b = c.m_binary.m_blocking & 0xf;
                ok = ok && (r.m_type == Dec::TypeBinary) && (r.m_text == c.m_text) && (r.m_dataBits == c.m_dataBits)
                    && (r.m_signature == Dec::CheckOk) && (r.m_fillState == 0) && (r.m_blocking == b)
                    && r.m_retrieval && (r.m_mailDrop == c.m_binary.m_mailDrop) && (r.m_displayRtl == c.m_binary.m_rightToLeft)
                    && (r.m_headerMessage == c.m_binary.m_header) && (r.m_statusInfo == c.m_binary.m_statusInfo)
                    && (r.m_reservedBits == 0);
            }
            else
            {
                ok = ok && (r.m_type == Dec::TypeSecure) && (r.m_secureType == (int) c.m_binary.m_secureType);

                if (c.m_binary.m_secureType == Flex::SecureAlphanumeric) {
                    ok = ok && (r.m_text == c.m_text);
                } else if (c.m_binary.m_secureType == Flex::SecureBinary) {
                    ok = ok && (r.m_text == c.m_text) && (r.m_dataBits == c.m_dataBits) && (r.m_fillState == 0);
                } else {
                    // Not terminated: all the bits of the word, the unused ones 0
                    ok = ok && (r.m_text.compare(0, 4, c.m_text) == 0) && (r.m_dataBits == c.m_dataBits) && r.m_payloadUnknown;
                }
            }

            if (!ok)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "type %d text %s bits %d K %d sig %d fill %d B %d N %d R %d M %d D %d H %d I %d t %d",
                    (int) r.m_type, r.m_text.c_str(), r.m_dataBits, (int) r.m_checksum, (int) r.m_signature, r.m_fillState,
                    r.m_blocking, r.m_messageNumber, r.m_retrieval, r.m_mailDrop, r.m_displayRtl, r.m_headerMessage,
                    r.m_statusInfo, r.m_secureType);
                why = buf;
            }
        }
        else if (er.m_ok)
        {
            why = "cost " + std::to_string(cost) + " expected " + std::to_string(expectedCost) + ", messages "
                + std::to_string(out.size());
        }

        std::string label = std::string("tx binary: ") + c.m_label + (why.empty() ? "" : " (" + why + ")");
        check(ok, label.c_str());
    }

    // Rejected messages
    struct Bad { const char *m_label; Flex::MessageType m_type; std::string m_text; Flex::BinaryParams m_binary; };
    Flex::BinaryParams reserved = secParams(Flex::SecureAlphanumeric);
    reserved.m_secureType = (Flex::SecureType) 3;
    const Bad bad[] = {
        {"bits not a multiple of the blocking length", Flex::Binary, "123", hexParams(8, false, false, false, false)},
        {"right to left with a blocking length of 1", Flex::Binary, "12", hexParams(1, false, true, false, false)},
        {"blocking length 0", Flex::Binary, "12", hexParams(0, false, false, false, false)},
        {"not a hex digit", Flex::Binary, "12G4", hexParams(4, false, false, false, false)},
        {"empty", Flex::Binary, "", hexParams(4, false, false, false, false)},
        // B=4, short address (85 message words): 435 digits in the first fragment (83 data words, 3 bits left),
        // 441 in the others (84 data words, none left): 64 fragments and one digit more
        {"more than 64 fragments", Flex::Binary, std::string(435 + 63 * 441 + 1, 'A'), hexParams(4, false, false, false, false)},
        {"secure type 11 reserved", Flex::Secure, "1234", reserved},
        {"secure binary not hex", Flex::Secure, "xyz", secParams(Flex::SecureBinary)},
        {"secure alphanumeric more than 64 fragments", Flex::Secure, std::string(64 * 252 + 1, 'a'), secParams(Flex::SecureAlphanumeric)},
        // 251 characters in the first fragment, 252 in the others
        {"alphanumeric more than 64 fragments", Flex::Alphanumeric, std::string(251 + 63 * 252 + 1, 'a'), Flex::BinaryParams()},
    };

    for (const Bad& b : bad)
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = b.m_type;
        m.m_text = b.m_text;
        m.m_binary = b.m_binary;
        Enc::EnqueueResult er = enc.enqueue(m);
        std::string label = std::string("tx binary rejected: ") + b.m_label + (er.m_ok ? "" : " (" + er.m_error + ")");
        check(!er.m_ok, label.c_str());
    }

    // Longest messages in one frame (STD-43A 4.1): 85 message words with a short address, 84 with a long one
    {
        struct Longest { uint64_t m_capcode; Flex::MessageType m_type; std::string m_text; Flex::BinaryParams m_binary; const char *m_label; };
        const Longest longest[] = {
            // 2 header words and 83 data words: 1743 bits, 434 digits of 8-bit characters (1736 bits)
            {1234567, Flex::Binary, std::string(434, '5'), hexParams(8, false, false, false, false),
                "tx binary: 434 digits (83 data words) in one frame, short address"},
            // Long address: 82 data words, 1722 bits, 430 digits (1720 bits)
            {2101300, Flex::Binary, std::string(430, '5'), hexParams(8, false, false, false, false),
                "tx binary: 430 digits (82 data words) in one frame, long address"},
            {1234567, Flex::Secure, std::string(252, 'z'), secParams(Flex::SecureAlphanumeric),
                "tx secure: 252 characters (84 data words) in one frame, short address"},
            {1234567, Flex::Alphanumeric, std::string(251, 'y'), Flex::BinaryParams(),
                "tx alphanumeric: 251 characters (85 message words) in one frame, short address"},
            {2101300, Flex::Alphanumeric, std::string(248, 'y'), Flex::BinaryParams(),
                "tx alphanumeric: 248 characters (84 message words) in one frame, long address"},
        };

        for (const Longest& l : longest)
        {
            Enc enc;
            Enc::TxMessage m;
            m.m_capcode = l.m_capcode;
            m.m_type = l.m_type;
            m.m_text = l.m_text;
            m.m_binary = l.m_binary;
            Enc::EnqueueResult er = enc.enqueue(m);
            size_t queued = enc.pending();
            Dec dec;
            std::vector<Dec::Message> out = runEncoder(enc, dec);
            bool ok = er.m_ok && (queued == 1) && (out.size() == 1) && (out[0].m_text == m.m_text) && (out[0].m_checksum == Dec::CheckOk)
                && ((l.m_type == Flex::Secure) || (out[0].m_signature == Dec::CheckOk));
            check(ok, l.m_label);
        }
    }

    // Messages longer than a frame: fragments packed full in successive frames, the message reassembled
    struct Frag
    {
        const char *m_label;
        uint64_t m_capcode;
        Flex::MessageType m_type;
        std::string m_text;
        Flex::BinaryParams m_binary;
        std::vector<int> m_bits;    // Data bits of each fragment (alphanumeric: characters of each fragment)
        Flex::Mode m_mode;
    };

    std::string digits;
    for (int i = 0; i < 300; i++) {
        digits += "1A2B3C4D5E6F7890"[i % 16];
    }
    std::string chars;
    for (int i = 0; i < 600; i++) {
        chars += (char) ('a' + (i % 26));
    }
    const std::string hex600 = digits.substr(0, 300) + digits.substr(0, 300);
    const std::string hex1000 = hex600 + hex600.substr(0, 400);

    const Frag frags[] = {
        // Short address: 85 message words (STD-43A 4.1), the first HEX fragment has 83 data words, the others 84.
        // B=8: 83 words are 1743 bits, 1736 bits (7 left, fewer than 8) then the rest
        {"hex B=8 600 digits", 1234567, Flex::Binary, hex600, hexParams(8, false, false, false, false), {1736, 664}, Flex::Mode1600_2},
        // B=7: a multiple of 28 bits with fewer than 7 left: 80 words (1680 bits)
        {"hex B=7 595 digits", 1234567, Flex::Binary, hex600.substr(0, 595), hexParams(7, false, true, false, false), {1680, 700},
            Flex::Mode1600_2},
        // B=16: 1728 bits (15 left of 1743), then 84 words: 1760 bits (4 left of 1764), then the rest
        {"hex B=16 1000 digits", 1234567, Flex::Binary, hex1000, hexParams(16, true, false, false, false), {1728, 1760, 512},
            Flex::Mode1600_2},
        // B=4: 1740 bits (3 left); last fragment of 84 bits ending exactly at a word end, last character 1111:
        // padding word. The bits left in the first fragment are 0 like the last data bits: still not data
        {"hex B=4 last fragment exact end, padding word", 1234567, Flex::Binary, hex600.substr(0, 435) + "123456789ABCDEF12345F",
            hexParams(4, false, false, false, false), {1740, 84}, Flex::Mode1600_2},
        // Long address: 84 message words, the first fragment has 82 data words: 1720 bits
        {"hex long address 600 digits", 2101300, Flex::Binary, hex600, hexParams(8, false, false, false, false), {1720, 680},
            Flex::Mode1600_2},
        {"hex 6400/4 600 digits", 1234567, Flex::Binary, hex600, hexParams(8, false, false, false, false), {1736, 664}, Flex::Mode6400_4},
        // Secure alphanumeric: 252 characters a fragment (84 full words, no ETX)
        {"secure alphanumeric 600 characters", 1234567, Flex::Secure, chars, secParams(Flex::SecureAlphanumeric), {}, Flex::Mode1600_2},
        // Secure binary: whole words of 1-bit characters, a multiple of 4: 84 words (1764 bits)
        {"secure binary 600 digits", 1234567, Flex::Secure, hex600, secParams(Flex::SecureBinary), {1764, 636}, Flex::Mode1600_2},
        // Last fragment ending exactly at a word end: termination word
        {"secure binary last fragment exact end", 1234567, Flex::Secure, hex600.substr(0, 441) + "123456789ABCDEF123455",
            secParams(Flex::SecureBinary), {1764, 84}, Flex::Mode1600_2},
        // 1764 bits, then 236 bits in 12 words read whole (no termination)
        {"secure data defined separately 500 digits", 1234567, Flex::Secure, hex600.substr(0, 500), secParams(Flex::SecureSeparate),
            {1764, 252}, Flex::Mode1600_2},
        // Alphanumeric (STD-43A 4.1): the first fragment 251 characters after the signature slot (84 full words),
        // the others 252, no ETX; the signature of the whole message in the first fragment
        {"alphanumeric 600 characters", 1234567, Flex::Alphanumeric, chars, Flex::BinaryParams(), {251, 252, 97}, Flex::Mode1600_2},
        // Long address: 83 character words: 248, then 249
        {"alphanumeric long address 600 characters", 2101300, Flex::Alphanumeric, chars, Flex::BinaryParams(), {248, 249, 103},
            Flex::Mode1600_2},
        // The last fragment ends exactly at a word end: no ETX
        {"alphanumeric last fragment exact end", 1234567, Flex::Alphanumeric, chars.substr(0, 503), Flex::BinaryParams(), {251, 252},
            Flex::Mode1600_2},
        {"alphanumeric 6400/4 600 characters", 1234567, Flex::Alphanumeric, chars, Flex::BinaryParams(), {251, 252, 97}, Flex::Mode6400_4},
    };

    for (const Frag& c : frags)
    {
        Enc enc;
        enc.setMode(c.m_mode);
        Enc::TxMessage m;
        m.m_capcode = c.m_capcode;
        m.m_type = c.m_type;
        m.m_text = c.m_text;
        m.m_binary = c.m_binary;
        Enc::EnqueueResult er = enc.enqueue(m);
        size_t queued = enc.pending();

        Dec dec;
        dec.setReportFragments(true);
        std::vector<Dec::RxFrame> rxFrames;
        int frames = 0;
        std::vector<Dec::Message> out = runEncoder(enc, dec, &rxFrames, &frames);

        std::vector<const Dec::Message *> parts;
        const Dec::Message *whole = nullptr;
        for (const Dec::Message& r : out)
        {
            if (r.m_isFragment) {
                parts.push_back(&r);
            } else {
                whole = &r;
            }
        }

        bool binary = (c.m_type == Flex::Binary);
        bool alphanumeric = (c.m_type == Flex::Alphanumeric);
        bool hexData = binary || (!alphanumeric && (c.m_binary.m_secureType != Flex::SecureAlphanumeric));
        size_t expected = c.m_bits.empty() ? (c.m_text.size() + 251) / 252 : c.m_bits.size();
        bool ok = er.m_ok && (queued == expected) && whole && (parts.size() == expected) && (frames >= (int) expected);
        std::string why;

        for (size_t k = 0; ok && (k < parts.size()); k++)
        {
            const Dec::Message& p = *parts[k];
            ok = (p.m_checksum == Dec::CheckOk) && (p.m_messageNumber == ((c.m_type == Flex::Secure) ? 1 : 0))
                && (p.m_fragment == ((k == 0) ? Dec::FragmentFirst : (k + 1 == parts.size()) ? Dec::FragmentLast : Dec::FragmentMiddle));
            if (ok && hexData && !c.m_bits.empty()) {
                ok = (p.m_dataBits == c.m_bits[k]) && (p.m_fillState == 0);
            }
            // Alphanumeric: the characters of the fragment, U0 V0 = 00 in the fragments after the first
            if (ok && alphanumeric)
            {
                int start = 0;
                for (size_t i = 0; i < k; i++) {
                    start += c.m_bits[i];
                }
                ok = (p.m_text == c.m_text.substr(start, c.m_bits[k])) && (p.m_fragmentControl == ((k == 0) ? -1 : 0));
            }
            if (!ok)
            {
                why = "fragment " + std::to_string(k) + ": K " + std::to_string((int) p.m_checksum) + " N " + std::to_string(p.m_messageNumber)
                    + " kind " + std::to_string((int) p.m_fragment) + " bits " + std::to_string(p.m_dataBits) + " fill "
                    + std::to_string(p.m_fillState);
            }
        }
        if (ok)
        {
            // Data defined separately are not terminated: the last word of the last fragment is read whole
            bool separate = !binary && (c.m_binary.m_secureType == Flex::SecureSeparate);
            ok = whole->m_complete && (separate ? (whole->m_text.compare(0, c.m_text.size(), c.m_text) == 0) : (whole->m_text == c.m_text))
                && (whole->m_fragmentCount == (int) expected) && !whole->m_fragmentGap;
            if (ok && separate) {
                int bits = 0;
                for (int b : c.m_bits) {
                    bits += b;
                }
                ok = (whole->m_dataBits == bits);
            }
            if (ok && binary) {
                ok = (whole->m_signature == Dec::CheckOk) && (whole->m_dataBits == (int) c.m_text.size() * 4)
                    && (whole->m_blocking == (c.m_binary.m_blocking & 0xf)) && (whole->m_displayRtl == c.m_binary.m_rightToLeft)
                    && (whole->m_mailDrop == c.m_binary.m_mailDrop);
            }
            if (ok && alphanumeric) {
                ok = (whole->m_signature == Dec::CheckOk) && whole->m_retrieval;
            }
            if (!ok && whole)
            {
                why = "whole: complete " + std::to_string(whole->m_complete) + " fragments " + std::to_string(whole->m_fragmentCount)
                    + " sig " + std::to_string((int) whole->m_signature) + " bits " + std::to_string(whole->m_dataBits)
                    + " text " + whole->m_text.substr(0, 40);
            }
        }
        if (!er.m_ok) {
            why = er.m_error;
        } else if (!why.size() && !ok) {
            why = "queued " + std::to_string(queued) + " fragments " + std::to_string(parts.size()) + " frames " + std::to_string(frames)
                + (whole ? "" : " no message");
        }

        std::string label = std::string("tx fragments: ") + c.m_label + (why.empty() ? "" : " (" + why + ")");
        check(ok, label.c_str());
    }

    // The id is sent with the last fragment; removing it drops the fragments still queued
    {
        Enc enc;
        std::string error;
        enc.setCollapse(0, error);  // The capcode listens to every frame
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::Binary;
        m.m_text = hex1000;
        m.m_binary = hexParams(16, false, false, false, false);
        Enc::EnqueueResult er = enc.enqueue(m);
        uint64_t index = RadioPagingFlexScheduler::capcodeSlot(1234567).m_frame;
        bool ok = er.m_ok && (enc.pending() == 3);
        Enc::Frame f1 = enc.buildFrame(index);
        ok = ok && f1.m_sentIds.empty() && !f1.m_idle && (enc.pending() == 2);
        Enc::Frame f2 = enc.buildFrame(index + 1);
        ok = ok && f2.m_sentIds.empty() && (enc.pending() == 1);
        Enc::Frame f3 = enc.buildFrame(index + 2);
        ok = ok && (f3.m_sentIds.size() == 1) && (f3.m_sentIds[0] == er.m_id) && (enc.pending() == 0);
        std::string label = "tx fragments: one fragment a frame, the id sent with the last one";
        if (!ok) {
            label += " (sent " + std::to_string(f1.m_sentIds.size()) + " " + std::to_string(f2.m_sentIds.size()) + " "
                + std::to_string(f3.m_sentIds.size()) + " idle " + std::to_string(f1.m_idle) + " pending " + std::to_string(enc.pending()) + ")";
        }
        check(ok, label.c_str());

        er = enc.enqueue(m);
        enc.buildFrame(index);
        check(enc.remove(er.m_id) && (enc.pending() == 0), "tx fragments: remove drops the fragments still queued");
    }
}

// Scheduling of fragments (STD-43A 4.2.1, 3.8.5): the first fragment in a frame of the pager, the others in the
// frames that follow whatever the collapse, in the phase of the first; one fragmented message per address at a
// time; while it is sent, one unfragmented message per frame to that address
// Fragments cut to the room of the frame they go in (STD-43A 4.1: a fragment is what the frame has room for). The
// block information words of roaming and of the time take up to 3 words of a phase: a fragment of the 85 words of 4.1
// never fitted and stayed queued for ever
void testFragmentsFitTheFrame()
{
    std::string text;
    for (int i = 0; i < 700; i++) {
        text += (char) ('A' + i % 26);
    }
    std::string digits;
    for (int i = 0; i < 1200; i++) {
        digits += "1A2B3C4D5E6F7890"[i % 16];
    }

    auto setup = [](Enc& enc, bool extras) {
        std::string error;
        Enc::Roaming r;
        r.m_enabled = extras;
        r.m_localId = 10;
        r.m_coverageZone = 1;
        enc.setRoaming(r);
        Enc::TimeService ts;
        ts.m_enabled = extras;
        ts.m_sendZone = true;
        ts.m_zoneMinutes = 120;
        enc.setTimeService(ts, error);
    };

    // The fragments and the whole message decoded from the frames of a run
    struct Result { std::vector<Dec::Message> m_fragments; std::vector<Dec::Message> m_wholes; };
    auto run = [](Enc& enc, Dec& dec, uint64_t from, uint64_t to, Result& result) {
        for (uint64_t index = from; (index < to) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            if (f.m_reserved) {
                continue;
            }
            for (const Dec::Message& m : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data))) {
                (m.m_isFragment ? result.m_fragments : result.m_wholes).push_back(m);
            }
        }
    };
    auto samePhase = [](const Result& r) {
        return !r.m_fragments.empty() && std::all_of(r.m_fragments.begin(), r.m_fragments.end(),
            [&r](const Dec::Message& m) { return m.m_phase == r.m_fragments.front().m_phase; });
    };

    // A short and a long address (2 address words, 2 vector words, the second one carrying the first message word)
    for (uint64_t capcode : {1234567ull, 2101300ull})
    for (Flex::Mode mode : {Flex::Mode1600_2, Flex::Mode6400_4})
    {
        const bool isLong = Flex::isLongAddress(capcode);

        // Roaming and time on from the start: alphanumeric, then HEX/binary
        for (int binary = 0; binary < 2; binary++)
        {
            Enc enc;
            enc.setMode(mode);
            setup(enc, true);
            Enc::TxMessage m;
            m.m_capcode = capcode;
            m.m_type = binary ? Flex::Binary : Flex::Alphanumeric;
            m.m_text = binary ? digits : text;
            m.m_binary.m_blocking = 8;
            bool queued = enc.enqueue(m).m_ok;
            Dec dec;
            dec.setReportFragments(true);
            Result r;
            run(enc, dec, 0, 200, r);

            bool whole = (r.m_wholes.size() == 1) && r.m_wholes[0].m_complete && (r.m_wholes[0].m_text == m.m_text);
            // Shorter than the characters of a whole frame (STD-43A 4.1: 251 with a short address, 248 with a long
            // one): cut to the room left by the block words
            bool cut = binary || (!r.m_fragments.empty() && (r.m_fragments.front().m_text.size() < (isLong ? 248u : 251u)));
            std::string label = std::string("fragments fit the frame ") + (isLong ? "long address " : "short address ") + Flex::modeName(mode)
                + (binary ? ", HEX/binary" : ", alphanumeric") + ": roaming and time on, all sent, the message whole"
                + (binary ? "" : ", the first fragment cut to the room") + ", one phase";
            check(queued && (enc.pending() == 0) && whole && cut && samePhase(r), label.c_str());
        }

        // Roaming and time turned on after the first fragment: the fragments still queued are cut again
        {
            Enc enc;
            enc.setMode(mode);
            setup(enc, false);
            Enc::TxMessage m;
            m.m_capcode = capcode;
            m.m_type = Flex::Alphanumeric;
            m.m_text = text;
            enc.enqueue(m);
            Dec dec;
            dec.setReportFragments(true);
            Result r;
            run(enc, dec, 0, 1, r);
            bool firstSent = !r.m_fragments.empty();
            setup(enc, true);
            run(enc, dec, 1, 200, r);

            bool whole = (r.m_wholes.size() == 1) && r.m_wholes[0].m_complete && (r.m_wholes[0].m_text == text);
            std::string label = std::string("fragments fit the frame ") + (isLong ? "long address " : "short address ")
                + Flex::modeName(mode) + ": roaming and time turned on after the first fragment, the rest cut again, the message whole";
            check(firstSent && (enc.pending() == 0) && whole && samePhase(r), label.c_str());
        }
    }
}

void testFragmentScheduling()
{
    std::string digits;
    for (int i = 0; i < 1200; i++) {
        digits += "1A2B3C4D5E6F7890"[i % 16];
    }
    Flex::BinaryParams b8;
    b8.m_blocking = 8;

    // Frames of a run of the encoder, each decoded; returns the frame index of every fragment and message
    struct Seen { uint64_t m_index; Dec::Message m_message; };
    auto run = [](Enc& enc, Dec& dec, uint64_t from, uint64_t to) {
        std::vector<Seen> seen;
        for (uint64_t index = from; (index < to) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            if (f.m_reserved) {
                continue;
            }
            Dec::RxFrame rx = toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data);
            for (const Dec::Message& m : dec.decodeFrame(rx)) {
                seen.push_back(Seen{index, m});
            }
        }
        return seen;
    };

    // Collapse 4: the pager listens to one frame in 16; the fragments after the first follow at once
    for (Flex::Mode mode : {Flex::Mode1600_2, Flex::Mode6400_4})
    {
        Enc enc;
        std::string error;
        enc.setMode(mode);
        enc.setCollapse(4, error);
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::Binary;
        m.m_text = digits;          // 4800 bits: 3 fragments
        m.m_binary = b8;
        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        dec.setReportFragments(true);
        std::vector<Seen> seen = run(enc, dec, 0, 400);

        std::vector<uint64_t> frames;
        std::vector<char> phases;
        const Dec::Message *whole = nullptr;
        for (const Seen& s : seen)
        {
            if (s.m_message.m_isFragment)
            {
                frames.push_back(s.m_index);
                phases.push_back(s.m_message.m_phase);
            }
            else
            {
                whole = &s.m_message;
            }
        }

        bool ok = er.m_ok && (frames.size() == 3) && whole && whole->m_complete && (whole->m_text == digits)
            && (whole->m_signature == Dec::CheckOk);
        ok = ok && RadioPagingFlexScheduler::frameEligible(1234567, RadioPagingFlexScheduler::frameTime(frames[0]).m_frame, 4)
            && (frames[1] == frames[0] + 1) && (frames[2] == frames[0] + 2);
        ok = ok && (phases[1] == phases[0]) && (phases[2] == phases[0]);
        std::string label = std::string("fragment scheduling ") + Flex::modeName(mode)
            + ": first fragment in a frame of the collapse, the others in the next frames, same phase";
        check(ok, label.c_str());
    }

    // Two fragmented messages to one address: the second starts once the first is sent entirely
    {
        Enc enc;
        std::string error;
        enc.setCollapse(0, error);
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::Binary;
        m.m_text = digits.substr(0, 600);   // 2 fragments
        m.m_binary = b8;
        Enc::EnqueueResult a = enc.enqueue(m);
        m.m_text = digits.substr(16, 600);
        Enc::EnqueueResult b = enc.enqueue(m);
        Dec dec;
        dec.setReportFragments(true);
        std::vector<Seen> seen = run(enc, dec, 0, 50);

        std::vector<std::pair<uint64_t, int>> frags;    // frame, message number
        int complete = 0;
        for (const Seen& s : seen)
        {
            if (s.m_message.m_isFragment) {
                frags.push_back(std::make_pair(s.m_index, s.m_message.m_messageNumber));
            } else if (s.m_message.m_complete && (s.m_message.m_signature == Dec::CheckOk)) {
                complete++;
            }
        }

        bool ok = a.m_ok && b.m_ok && (frags.size() == 4) && (complete == 2)
            && (frags[0].second == frags[1].second) && (frags[2].second == frags[3].second) && (frags[0].second != frags[2].second)
            && (frags[2].first > frags[1].first);
        check(ok, "fragment scheduling: a second fragmented message to the address waits for the end of the first (3.8.5 (2))");
    }

    // While a fragmented message is sent, one unfragmented message per frame to the same address (3.8.5 (3));
    // otherwise two (3.8.5 (1))
    {
        Enc enc;
        std::string error;
        enc.setCollapse(0, error);
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::Binary;
        m.m_text = digits;          // 3 fragments
        m.m_binary = b8;
        enc.enqueue(m);
        Enc::TxMessage u;
        u.m_capcode = 1234567;
        u.m_type = Flex::Alphanumeric;
        for (int i = 0; i < 4; i++)
        {
            u.m_text = "short " + std::to_string(i);
            enc.enqueue(u);
        }
        Dec dec;
        dec.setReportFragments(true);
        std::vector<Seen> seen = run(enc, dec, 0, 50);

        std::map<uint64_t, int> unfragmented;   // frame -> unfragmented messages of the address
        std::map<uint64_t, bool> fragment;
        int shorts = 0;
        for (const Seen& s : seen)
        {
            if (s.m_message.m_isFragment) {
                fragment[s.m_index] = true;
            } else if (s.m_message.m_type == Dec::TypeAlphanumeric) {
                unfragmented[s.m_index]++;
                shorts++;
            }
        }

        bool ok = (shorts == 4) && (fragment.size() == 3);
        for (const auto& f : unfragmented)
        {
            if (fragment.count(f.first) ? (f.second > 1) : (f.second > 2)) {
                ok = false;
            }
        }
        check(ok, "fragment scheduling: one unfragmented message per frame to an address sending a fragmented one, two otherwise");
    }

    // Reserved frames: the fragments skip them and go on in the next frame that is sent
    {
        Enc enc;
        std::string error;
        enc.setCollapse(0, error);
        uint32_t frame = RadioPagingFlexScheduler::capcodeSlot(1234567).m_frame;
        std::vector<uint8_t> table(128, 0);
        table[(frame + 1) % 128] = 1;
        table[(frame + 2) % 128] = 1;
        bool set = enc.setReservedFrames(table, error);
        Enc::TxMessage m;
        m.m_capcode = 1234567;
        m.m_type = Flex::Binary;
        m.m_text = digits.substr(0, 600);
        m.m_binary = b8;
        enc.enqueue(m);
        Dec dec;
        dec.setReportFragments(true);
        std::vector<Seen> seen = run(enc, dec, frame, frame + 20);
        std::vector<uint64_t> frames;
        for (const Seen& s : seen)
        {
            if (s.m_message.m_isFragment) {
                frames.push_back(s.m_index);
            }
        }
        check(set && (frames.size() == 2) && (frames[0] == frame) && (frames[1] == frame + 3),
            "fragment scheduling: reserved frames are skipped, the next fragment goes in the next frame sent");
    }
}

// Date and time (STD-43A 3.7.2): word layout, zone table, civil date, the encoder time service decoded
// Message numbering of the network (STD-43A 8.4, 3.10.1.1-3.10.1.4 N and R)
void testNumbering()
{
    auto msg = [](uint64_t capcode, Flex::MessageType type, const std::string& text) {
        Enc::TxMessage m;
        m.m_capcode = capcode;
        m.m_type = type;
        m.m_text = text;
        return m;
    };

    // Numbering on: one sequence from 0 over the numbered types of the address, R = 1. Tone and numeric
    // messages have no number and leave the sequence alone; a secure message is not numbered
    {
        Enc enc;
        enc.enqueue(msg(4096, Flex::Alphanumeric, "first"));
        enc.enqueue(msg(4096, Flex::Tone, ""));
        enc.enqueue(msg(4096, Flex::Numeric, "123"));
        Enc::TxMessage bin = msg(4096, Flex::Binary, "A5A5");
        bin.m_binary.m_blocking = 8;
        enc.enqueue(bin);
        Enc::TxMessage shortNumbered = msg(4096, Flex::ShortMessage, "");
        shortNumbered.m_shortType = Flex::ShortNumbered;
        shortNumbered.m_source = 2;
        enc.enqueue(shortNumbered);
        Enc::TxMessage secure = msg(4096, Flex::Secure, "secret");
        enc.enqueue(secure);
        enc.enqueue(msg(4096, Flex::Alphanumeric, "last"));
        Dec dec;
        std::vector<Dec::Message> out = runEncoder(enc, dec);
        std::string seen;

        for (const Dec::Message& m : out) {
            seen += std::to_string((int) m.m_type) + ":" + std::to_string(m.m_messageNumber) + (m.m_retrieval ? "R " : " ");
        }

        bool ok = (out.size() == 7);
        for (const Dec::Message& m : out)
        {
            if ((m.m_type == Dec::TypeAlphanumeric) && (m.m_text == "first")) {
                ok = ok && (m.m_messageNumber == 0) && m.m_retrieval;
            } else if (m.m_type == Dec::TypeBinary) {
                ok = ok && (m.m_messageNumber == 1) && m.m_retrieval;
            } else if (m.m_type == Dec::TypeShortMessage) {
                ok = ok && (m.m_messageNumber == 2) && m.m_retrieval;
            } else if (m.m_type == Dec::TypeSecure) {
                ok = ok && (m.m_messageNumber != 3);
            } else if ((m.m_type == Dec::TypeAlphanumeric) && (m.m_text == "last")) {
                ok = ok && (m.m_messageNumber == 3) && m.m_retrieval;
            }
        }
        check(ok, ("numbering on: one sequence 0, 1, 2, 3 over alphanumeric, binary and numbered short messages, R = 1; "
            "tone, numeric and secure messages do not take numbers of it (" + seen + ")").c_str());
    }

    // The largest number of the network: 2, then 0 again
    {
        Enc enc;
        std::string error;
        Enc::Numbering numbering;
        numbering.m_maxNumber = 2;
        bool set = enc.setNumbering(numbering, error);
        numbering.m_maxNumber = 64;
        bool refused = !enc.setNumbering(numbering, error) && (enc.numbering().m_maxNumber == 2);
        for (int i = 0; i < 5; i++) {
            enc.enqueue(msg(4096, Flex::Alphanumeric, "m" + std::to_string(i)));
        }
        Dec dec;
        std::vector<Dec::Message> out = runEncoder(enc, dec);
        std::string numbers;
        for (const Dec::Message& m : out) {
            numbers += std::to_string(m.m_messageNumber);
        }
        check(set && refused && (numbers == "01201"), ("numbering: wraps after the largest number of the network, 64 refused (" + numbers + ")").c_str());
    }

    // A header message and the data message after it share the number; the sequence goes on after them
    {
        Enc enc;
        Enc::TxMessage header = msg(4096, Flex::Binary, "48454144");
        header.m_binary.m_blocking = 8;
        header.m_binary.m_header = true;
        Enc::TxMessage data = msg(4096, Flex::Binary, "0123456789ABCDEF");
        data.m_binary.m_blocking = 1;
        enc.enqueue(header);
        enc.enqueue(data);
        enc.enqueue(msg(4096, Flex::Alphanumeric, "after"));
        Dec dec;
        std::vector<Dec::Message> out = runEncoder(enc, dec);
        bool ok = (out.size() == 3) && (out[0].m_messageNumber == 0) && out[0].m_headerMessage && (out[1].m_messageNumber == 0)
            && !out[1].m_headerMessage && (out[2].m_messageNumber == 1);
        check(ok, "numbering: header message and its data message have one number (3.10.1.2 H)");
    }

    // Numbering off: R = 0, N = 0 when the address has no other message in progress. The short message goes in
    // the frame of the last fragment of the message sent in fragments (N = 0): it takes 1
    {
        Enc enc;
        std::string error;
        Enc::Numbering numbering;
        numbering.m_enabled = false;
        enc.setNumbering(numbering, error);
        std::string chars;
        for (int i = 0; i < 261; i++) {
            chars += (char) ('a' + (i % 26));
        }
        Enc::TxMessage shortNumbered = msg(4096, Flex::ShortMessage, "");
        shortNumbered.m_shortType = Flex::ShortNumbered;
        bool refused = !enc.enqueue(shortNumbered).m_ok;
        enc.enqueue(msg(4096, Flex::Alphanumeric, chars));
        enc.enqueue(msg(4096, Flex::Alphanumeric, "short one"));
        Dec dec;
        std::vector<Dec::Message> out = runEncoder(enc, dec);
        const Dec::Message *single = nullptr;
        const Dec::Message *whole = nullptr;
        for (const Dec::Message& m : out)
        {
            if (m.m_text == "short one") {
                single = &m;
            } else if (m.m_text == chars) {
                whole = &m;
            }
        }
        bool ok = refused && single && whole && (single->m_messageNumber == 1) && !single->m_retrieval
            && (whole->m_messageNumber == 0) && !whole->m_retrieval && whole->m_complete && (whole->m_fragmentCount == 2);
        std::string why;
        for (const Dec::Message& m : out) {
            why += " [" + m.m_text.substr(0, 10) + " N" + std::to_string(m.m_messageNumber) + " R" + std::to_string(m.m_retrieval)
                + " c" + std::to_string(m.m_complete) + "]";
        }
        check(ok, ("numbering off: R = 0, N 0 or a number apart from the message of the address in progress, the short message "
            "does not abandon the fragments, numbered short message refused (refused " + std::to_string(refused) + why + ")").c_str());
    }

    // Receiver: the messages with R = 1 of an address are checked for skipped numbers; the sequence wraps after the
    // largest number of the network
    {
        auto runOne = [&](int txMax, int rxMax, int skip, int count) {
            Enc enc;
            std::string error;
            Enc::Numbering numbering;
            numbering.m_maxNumber = txMax;
            enc.setNumbering(numbering, error);
            Dec dec;
            dec.setMaxMessageNumber(rxMax);
            std::vector<Dec::Message> out;
            uint64_t index = 0;
            for (int i = 0; i < count; i++)
            {
                enc.enqueue(msg(4096, Flex::Alphanumeric, "m" + std::to_string(i)));
                for (; (index < 400) && (enc.pending() > 0); index++)
                {
                    Enc::Frame f = enc.buildFrame(index);
                    if (i == skip) {
                        continue;   // Not received
                    }
                    std::vector<Dec::Message> o = dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
                    out.insert(out.end(), o.begin(), o.end());
                }
            }
            return out;
        };
        auto missed = [](const Dec::Message& m) {
            std::string s;
            for (int n : m.m_missedNumbers) {
                s += std::to_string(n) + ",";
            }
            return s;
        };

        std::vector<Dec::Message> a = runOne(63, 63, 1, 4);
        bool ok = (a.size() == 3) && a[0].m_missedNumbers.empty() && (missed(a[1]) == "1,") && (a[1].m_messageNumber == 2)
            && a[2].m_missedNumbers.empty();
        std::vector<Dec::Message> b = runOne(2, 2, -1, 5);
        bool wrap = (b.size() == 5);
        for (const Dec::Message& m : b) {
            wrap = wrap && m.m_missedNumbers.empty();
        }
        std::vector<Dec::Message> c = runOne(2, 63, -1, 4);
        bool mismatch = (c.size() == 4) && (c[3].m_messageNumber == 0) && (c[3].m_missedNumbers.size() == 61) && (c[3].m_missedNumbers[0] == 3);
        check(ok && wrap && mismatch, ("numbering: receiver reports the numbers skipped before a message, wraps after the largest number ("
            + (a.size() > 1 ? missed(a[1]) : std::string("?")) + ")").c_str());
    }
}

// Retransmission and send delay (STD-43A 4.3.7.6: a message sent again has its number and R = 0)
// The numeric formats (STD-43A 3.10.1.1): standard, special, numbered, numbered special
void testNumericFormats()
{
    const std::vector<std::pair<Flex::NumericFormat, std::pair<int, const char *>>> cases = {
        {Flex::NumericStandard, {(int) Dec::TypeNumeric, "standard"}},
        {Flex::NumericSpecial, {(int) Dec::TypeSpecialNumeric, "special"}},
        {Flex::NumericNumbered, {(int) Dec::TypeNumberedNumeric, "numbered"}},
        {Flex::NumericNumberedSpecial, {(int) Dec::TypeNumberedSpecialNumeric, "numbered special"}}
    };

    for (uint64_t capcode : {(uint64_t) 4096, (uint64_t) 2101249})      // A short and a long address
    {
        for (const auto& c : cases)
        {
            for (const char *text : {"1", "12345", "98765-4321 UU[]", "123456789012345678901234567890123456789"})
            {
                Enc enc;
                Enc::TxMessage m;
                m.m_capcode = capcode;
                m.m_type = Flex::Numeric;
                m.m_numericFormat = c.first;
                m.m_text = text;
                Enc::EnqueueResult er = enc.enqueue(m);
                Dec dec;
                std::vector<Dec::Message> got;

                for (uint64_t index = 100; (index < 140) && (enc.pending() > 0); index++)
                {
                    Enc::Frame f = enc.buildFrame(index);
                    for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data))) {
                        got.push_back(r);
                    }
                }

                const bool numbered = Flex::isNumberedFormat(c.first);
                // The numbered format holds 39 digits at most
                const bool fits = !(numbered && (std::string(text).size() > 39));
                bool ok = (er.m_ok == fits);

                if (fits)
                {
                    std::string shown = got.empty() ? std::string() : got[0].m_text;

                    // The unused digit positions of the last word are spaces
                    while (!shown.empty() && (shown.back() == ' ')) {
                        shown.pop_back();
                    }

                    ok = ok && (got.size() == 1) && ((int) got[0].m_type == c.second.first) && (shown == text)
                        && (got[0].m_checksum == Dec::CheckOk) && (got[0].m_capcode == capcode)
                        && (numbered ? (got[0].m_retrieval && (got[0].m_messageNumber == 0)) : (got[0].m_messageNumber < 0))
                        && (got[0].m_special == ((c.first == Flex::NumericNumberedSpecial) || (c.first == Flex::NumericSpecial)));
                }

                check(ok, (std::string("numeric ") + c.second.second + " format, " + std::to_string(std::string(text).size()) + " digits, "
                    + ((capcode > 2000000) ? "long" : "short") + " address: sent and decoded").c_str());
            }
        }
    }

    // Numbered numeric needs the numbering of the network; its numbers follow the sequence of the address; copies are numbered
    {
        Enc off;
        Enc::Numbering n;
        n.m_enabled = false;
        std::string error;
        off.setNumbering(n, error);
        Enc::TxMessage m;
        m.m_capcode = 4096;
        m.m_type = Flex::Numeric;
        m.m_numericFormat = Flex::NumericNumbered;
        m.m_text = "1";
        bool refused = !off.enqueue(m).m_ok;

        Enc enc;
        Dec dec;
        std::vector<int> numbers;
        m.m_text = "2";
        enc.enqueue(m);
        m.m_text = "3";
        m.m_retransmit = 1;
        m.m_retransmitInterval = 5;
        Enc::EnqueueResult copy = enc.enqueue(m);

        for (uint64_t index = 100; (index < 130) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data))) {
                numbers.push_back(r.m_messageNumber * 10 + (r.m_retrieval ? 1 : 0));
            }
        }

        check(refused && copy.m_ok && (numbers == std::vector<int>({1, 11, 10})),
            "numbered numeric: refused without numbering; numbers 0 and 1 with R = 1, the copy of 1 with R = 0");
    }
}

// M of an alphanumeric message: in the first fragment only (STD-43A 3.10.1.3)
void testAlphaMailDrop()
{
    Enc enc;
    Enc::TxMessage m;
    m.m_capcode = 4096;
    m.m_type = Flex::Alphanumeric;
    m.m_text = "drop me";
    m.m_binary.m_mailDrop = true;
    Enc::TxMessage plain = m;
    plain.m_capcode = 4097;
    plain.m_binary.m_mailDrop = false;
    Enc::EnqueueResult a = enc.enqueue(m);
    Enc::EnqueueResult b = enc.enqueue(plain);
    Dec dec;
    int mailDrop = 0, others = 0;

    for (uint64_t index = 100; (index < 110) && (enc.pending() > 0); index++)
    {
        Enc::Frame f = enc.buildFrame(index);

        for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
        {
            if (r.m_capcode == 4096) {
                mailDrop += (r.m_mailDrop && (r.m_checksum == Dec::CheckOk) && (r.m_text == "drop me")) ? 1 : 0;
            } else if (r.m_capcode == 4097) {
                others += !r.m_mailDrop ? 1 : 0;
            }
        }
    }

    check(a.m_ok && b.m_ok && (mailDrop == 1) && (others == 1), "alphanumeric message: the mail drop flag M is sent and decoded");
}

// Group messages with temporary addresses (STD-43A 5.2) and the short instruction vector (3.9.6)
void testGroupTx()
{
    // More pagers than the 8 of a fixed table: the group has all of them; 1000 pagers of one phase take their short
    // instructions over 24 frames
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_type = Flex::Numeric;
        m.m_text = "123";

        for (uint64_t c = 0; c < 1000; c++) {
            m.m_members.push_back(4000 + c);
        }

        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        size_t members = 0;
        int failed = 0;

        for (uint64_t index = 0; (index < 400) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            failed += (int) f.m_failedIds.size();

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if ((r.m_addressType == Dec::AddressTemporary) && r.m_groupTracked) {
                    members = r.m_groupMembers.size();
                }
            }
        }

        check(er.m_ok && (failed == 0) && (members == 1000), "group: the message to 1000 pagers has the 1000 of them");
        m.m_members.resize(Enc::m_maxGroupMembers + 1, 0);

        for (size_t i = 0; i < m.m_members.size(); i++) {
            m.m_members[i] = 10000 + i;
        }

        check(!enc.enqueue(m).m_ok, "group: more pagers than one phase carries before the group message are refused");

        // The most a phase carries: all their short instructions go before the group message
        m.m_members.resize(Enc::m_maxGroupMembers);

        for (size_t i = 0; i < m.m_members.size(); i++) {
            m.m_members[i] = 10000 + (uint64_t) i;
        }

        Enc full;
        Enc::EnqueueResult fr = full.enqueue(m);
        int fullFailed = 0, fullSent = 0;

        for (uint64_t index = 0; (index < 400) && (full.pending() > 0); index++)
        {
            Enc::Frame f = full.buildFrame(index);
            fullFailed += (int) f.m_failedIds.size();
            fullSent += (int) f.m_sentIds.size();
        }

        check(fr.m_ok && (fullSent == 1) && (fullFailed == 0), "group: the most pagers of one phase (127 frames of 43) are sent");

        // Collapse 2: the pagers of one frame in 4 get their instructions in a quarter of the frames. More than fit
        // before the group frame are refused, not dropped later
        {
            Enc narrow;
            std::string error;
            narrow.setCollapse(2, error);
            Enc::TxMessage g = m;
            g.m_members.clear();
            for (uint64_t k = 1; k <= 1500; k++) {
                g.m_members.push_back(64 * k);
            }
            Enc::EnqueueResult nr = narrow.enqueue(g);
            int narrowFailed = 0, narrowSent = 0;
            for (uint64_t index = 0; (index < 600) && (narrow.pending() > 0); index++)
            {
                Enc::Frame f = narrow.buildFrame(index);
                narrowFailed += (int) f.m_failedIds.size();
                narrowSent += (int) f.m_sentIds.size();
            }
            std::string label = "group: collapse 2, pagers of one frame in 4 that do not fit are refused (ok "
                + std::to_string(nr.m_ok) + " sent " + std::to_string(narrowSent) + " failed " + std::to_string(narrowFailed) + ")";
            check(!nr.m_ok && !nr.m_error.empty(), label.c_str());

            // 31 frames of 43 in 127: 1300 of them fit and are sent
            Enc fits;
            fits.setCollapse(2, error);
            g.m_members.resize(1300);
            Enc::EnqueueResult fitr = fits.enqueue(g);
            int fitsFailed = 0, fitsSent = 0;
            for (uint64_t index = 0; (index < 600) && (fits.pending() > 0); index++)
            {
                Enc::Frame f = fits.buildFrame(index);
                fitsFailed += (int) f.m_failedIds.size();
                fitsSent += (int) f.m_sentIds.size();
            }
            check(fitr.m_ok && (fitsSent == 1) && (fitsFailed == 0), "group: collapse 2, 1300 pagers of one frame in 4 are sent");
        }
    }

    // A group message in fragments: a new assignment on its slot in the middle starts a new group, the message
    // keeps the pagers it was for; a fragment without its first one gets no pagers
    {
        auto phaseFrame = [](unsigned frame, const std::vector<Flex::PhaseMessage>& messages) {
            Flex::PhaseParams params;
            Flex::PhaseResult r = Flex::buildPhase(params, messages);
            Flex::FrameParams fp;
            fp.m_cycle = 3;
            fp.m_frame = frame;
            Flex::FrameBytes bytes = Flex::assemble(fp, {r.m_words});
            return toRxFrame(Flex::Mode1600_2, false, bytes.m_sync, bytes.m_data);
        };
        auto assignment = [](uint64_t capcode, int frame) {
            Flex::PhaseMessage pm;
            pm.m_type = Flex::ShortInstruction;
            pm.m_capcode = capcode;
            pm.m_instruction.m_type = Flex::InstructionParams::TemporaryAddress;
            pm.m_instruction.m_slot = 0;
            pm.m_instruction.m_frame = frame;
            return pm;
        };
        Flex::PhaseMessage message;
        message.m_type = Flex::Alphanumeric;
        message.m_capcode = Flex::m_temporaryBase;
        message.m_text = std::string(400, 'G');
        message.m_number = 5;
        std::vector<Flex::PhaseMessage> fragments;
        std::string error;
        bool split = Flex::fragmentMessage(message, fragments, error) && (fragments.size() >= 2);
        Dec dec;
        std::vector<Dec::Message> out;

        auto take = [&](const Dec::RxFrame& frame) {
            for (const Dec::Message& r : dec.decodeFrame(frame)) {
                out.push_back(r);
            }
        };

        take(phaseFrame(10, {assignment(3000, 20), assignment(3001, 20), assignment(5000, 20)}));
        take(phaseFrame(20, {fragments[0]}));
        take(phaseFrame(21, {assignment(7777, 60)}));
        const Dec::Message newAssignment = out.back();

        for (size_t f = 1; f < fragments.size(); f++) {
            take(phaseFrame(21 + f, {fragments[f]}));
        }

        const Dec::Message delivered = out.back();
        out.clear();
        Flex::PhaseMessage numeric;
        numeric.m_type = Flex::Numeric;
        numeric.m_capcode = Flex::m_temporaryBase;
        numeric.m_text = "42";
        take(phaseFrame(60, {numeric}));

        check(split && (delivered.m_text == message.m_text) && delivered.m_groupTracked
            && (delivered.m_groupMembers == std::vector<uint64_t>({3000, 3001, 5000})), "group: the fragments of a group message keep its pagers");
        check((newAssignment.m_type == Dec::TypeShortInstruction) && (newAssignment.m_groupMembers == std::vector<uint64_t>({7777})),
            "group: an assignment on a slot whose message started is a new group, not added to the old one");
        check((out.size() == 1) && out[0].m_groupTracked && (out[0].m_groupMembers == std::vector<uint64_t>({7777})),
            "group: the last fragment of the old message leaves the new group");

        Dec late;
        std::vector<Dec::Message> orphan = late.decodeFrame(phaseFrame(22, {fragments[1]}));
        check(!orphan.empty() && !orphan[0].m_groupTracked && orphan[0].m_groupMembers.empty(),
            "group: a fragment without its first one gets no pagers");
    }

    // The way the pagers read it: the decoder tracks the assignment, then the message to the temporary address
    for (int collapse : {0, 2})
    {
        Enc enc;
        std::string error;
        enc.setCollapse(collapse, error);
        Enc::TxMessage m;
        m.m_type = Flex::Alphanumeric;
        m.m_text = "GROUP CALL";
        m.m_members = {3000, 3001, 5000};
        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        std::vector<Dec::Message> instructions;
        std::vector<std::pair<uint64_t, Dec::Message>> deliveries;
        int sent = 0, failed = 0;

        for (uint64_t index = 1000; (index < 1200) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            sent += (int) f.m_sentIds.size();
            failed += (int) f.m_failedIds.size();

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if (r.m_type == Dec::TypeShortInstruction) {
                    instructions.push_back(r);
                } else if (r.m_addressType == Dec::AddressTemporary) {
                    deliveries.push_back(std::make_pair(index, r));
                }
            }
        }

        bool ok = er.m_ok && (instructions.size() == 3) && (deliveries.size() == 1) && (sent == 1) && (failed == 0) && (enc.pending() == 0);

        if (ok)
        {
            const Dec::Message& d = deliveries[0].second;
            ok = (instructions[0].m_instructionType == 0) && (instructions[0].m_instructionSlot == 0)
                && (instructions[0].m_instructionFrame == (int) (deliveries[0].first % 128)) && d.m_groupTracked && (d.m_groupMembers.size() == 3)
                && (d.m_text == "GROUP CALL") && (d.m_checksum == Dec::CheckOk) && !d.m_retrieval;
        }

        check(ok, ("group message: a short instruction to each pager, then the message to the temporary address in the frame told (collapse "
            + std::to_string(collapse) + ")").c_str());
    }

    // The temporary address is free again once the message went; two groups use two addresses
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_type = Flex::Numeric;
        m.m_text = "123";
        m.m_members = {10, 20};
        Enc::EnqueueResult a = enc.enqueue(m);
        Enc::EnqueueResult b = enc.enqueue(m);
        Enc::EnqueueResult c = enc.enqueue(m);
        Dec dec;
        std::set<int> slots;
        int delivered = 0;

        for (uint64_t index = 0; (index < 300) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if (r.m_type == Dec::TypeShortInstruction) {
                    slots.insert(r.m_instructionSlot);
                } else if (r.m_addressType == Dec::AddressTemporary) {
                    delivered++;
                }
            }
        }

        Enc::EnqueueResult again = enc.enqueue(m);
        check(a.m_ok && b.m_ok && c.m_ok && (slots.size() == 3) && (delivered == 3) && (enc.pending() > 0) && again.m_ok,
            "group messages: each takes a temporary address, free again after its message");
    }

    // Pagers in different phases: the group message goes in each phase, in the same frame
    {
        Enc plain;
        plain.setMode(Flex::Mode6400_4);
        Enc::TxMessage single;
        single.m_capcode = 24;
        single.m_type = Flex::Alphanumeric;
        single.m_text = "SINGLE";
        plain.enqueue(single);
        Dec single_dec;
        bool seen = false;

        for (uint64_t index = 0; (index < 5) && (plain.pending() > 0); index++)
        {
            Enc::Frame f = plain.buildFrame(index);

            for (const Dec::Message& r : single_dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data))) {
                seen = seen || ((r.m_text == "SINGLE") && (r.m_phase == 'C'));
            }
        }

        check(seen, "6400/4: a message to a capcode of phase C");
    }
    {
        Enc enc;
        enc.setMode(Flex::Mode6400_4);
        Enc::TxMessage m;
        m.m_type = Flex::Alphanumeric;
        m.m_text = "PHASES";
        m.m_members = {16, 20, 24};     // Phases 0, 1 and 2 of the capcode
        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        std::vector<uint64_t> frames;
        std::string phasesSeen;
        int instructions = 0;

        for (uint64_t index = 0; (index < 200) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if (r.m_type == Dec::TypeShortInstruction) {
                    instructions++;
                } else if ((r.m_addressType == Dec::AddressTemporary) && (r.m_text == "PHASES")) {
                    frames.push_back(index);
                    phasesSeen += std::string(" ") + r.m_phase + (r.m_duplicate ? "d" : "");
                }
            }
        }

        check(er.m_ok && (instructions == 3) && (frames.size() == 3) && (frames[0] == frames[1]) && (frames[1] == frames[2]),
            ("group message: one message in each phase of the pagers, in the same frame (" + std::to_string(instructions) + " instructions, "
            + std::to_string(frames.size()) + " messages, phases" + phasesSeen + ", pending " + std::to_string(enc.pending()) + ")").c_str());
    }

    // Refused
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_type = Flex::Alphanumeric;
        m.m_text = "x";
        m.m_members = {10};
        bool one = !enc.enqueue(m).m_ok;
        m.m_members = {10, 10};
        bool twice = !enc.enqueue(m).m_ok;
        m.m_members = {10, 20};
        m.m_type = Flex::Binary;
        m.m_text = "A5";
        bool binary = !enc.enqueue(m).m_ok;
        m.m_type = Flex::Alphanumeric;
        m.m_retransmit = 1;
        bool again = !enc.enqueue(m).m_ok;
        m.m_retransmit = 0;
        m.m_members = {10, 0};
        bool address = !enc.enqueue(m).m_ok;
        Enc late;
        std::string error;
        late.setCollapse(7, error);
        m.m_members = {10, 20};
        bool collapse = !late.enqueue(m).m_ok;
        check(one && twice && binary && again && address && collapse,
            "group message refused: one pager, a pager twice, binary, copies, a capcode out of range, collapse 7");
    }

    // System Event Notification: the short instruction with the event flags to its address
    {
        Enc enc;
        Enc::TxMessage m;
        m.m_capcode = Flex::m_systemEventAddress;
        m.m_type = Flex::ShortInstruction;
        m.m_instruction.m_type = Flex::InstructionParams::SystemEvent;
        m.m_instruction.m_events = 0x0a5;
        m.m_frame = 7;
        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        bool got = false;

        for (uint64_t index = 0; (index < 20) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                got = got || ((r.m_type == Dec::TypeShortInstruction) && (r.m_instructionType == 1) && (r.m_eventFlags == 0xa5)
                    && (index == 7) && (r.m_addressType == Dec::AddressOperator));
            }
        }

        check(er.m_ok && got, "system event notification: the event flags to the notification address, in its frame");
    }

    // The System Event Notification service: in every phase of each frame for the frames asked, with the messages;
    // reserved frames do not count; out of range refused
    {
        Enc enc;
        std::string error;
        enc.setMode(Flex::Mode6400_4);
        check(!enc.notifySystemEvent(0, 128, error) && !enc.notifySystemEvent(0x800, 128, error)
            && !enc.notifySystemEvent(1, 127, error) && !enc.notifySystemEvent(1, 513, error),
            "system event notification: flags 1-2047 and 128-512 frames, else refused");
        std::vector<uint8_t> reserved(128, 0);
        reserved[10] = 1;
        enc.setReservedFrames(reserved, error);
        check(enc.notifySystemEvent(0x013, 128, error) && (enc.systemEventFrames() == 128) && enc.mandatory(5),
            "system event notification: started, each frame is sent");
        Enc::TxMessage m;
        m.m_capcode = 4096;
        m.m_text = "WITH THE EVENT";
        enc.enqueue(m);
        Dec dec;
        int framesWithAll = 0, framesWithAny = 0;
        bool messageOk = false, reservedSkipped = true;
        uint64_t index = 0;

        for (; index < 140; index++)
        {
            Enc::Frame f = enc.buildFrame(index);

            if (f.m_reserved)
            {
                reservedSkipped = reservedSkipped && !f.m_systemEvent;
                continue;
            }

            std::set<char> phases;

            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if ((r.m_type == Dec::TypeShortInstruction) && (r.m_instructionType == 1) && (r.m_eventFlags == 0x013)) {
                    phases.insert(r.m_phase);
                }
                messageOk = messageOk || (r.m_text == "WITH THE EVENT");
            }

            framesWithAll += (phases.size() == 4) ? 1 : 0;
            framesWithAny += phases.empty() ? 0 : 1;
        }

        char what[200];
        std::snprintf(what, sizeof(what), "system event notification: 128 frames with it in the 4 phases (%d, %d with it), the reserved "
            "frame skipped, the message sent too, then none left", framesWithAll, framesWithAny);
        check((framesWithAll == 128) && (framesWithAny == 128) && reservedSkipped && messageOk && (enc.systemEventFrames() == 0)
            && !enc.mandatory(index), what);
    }
}

void testRetransmission()
{
    auto msg = [](const std::string& text) {
        Enc::TxMessage m;
        m.m_capcode = 4096;
        m.m_type = Flex::Alphanumeric;
        m.m_text = text;
        return m;
    };

    // Collapse 0: every frame is a frame of the pager
    {
        Enc enc;
        Enc::TxMessage m = msg("again and again");
        m.m_sendDelay = 5;
        m.m_retransmit = 2;
        m.m_retransmitInterval = 10;
        Enc::EnqueueResult er = enc.enqueue(m);
        Dec dec;
        std::vector<std::pair<uint64_t, Dec::Message>> got;
        int sent = 0, retransmitted = 0, done = 0;

        for (uint64_t index = 100; (index < 200) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            sent += (int) f.m_sentIds.size();
            retransmitted += (int) f.m_retransmittedIds.size();
            done += (int) f.m_doneIds.size();
            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data))) {
                got.push_back(std::make_pair(index, r));
            }
        }

        bool ok = er.m_ok && (got.size() == 3) && (sent == 1) && (retransmitted == 2) && (done == 1) && (enc.pending() == 0);
        if (ok)
        {
            ok = (got[0].first == 105) && (got[1].first == 115) && (got[2].first == 125)
                && got[0].second.m_retrieval && !got[1].second.m_retrieval && !got[2].second.m_retrieval
                && (got[0].second.m_messageNumber == got[1].second.m_messageNumber) && (got[1].second.m_messageNumber == got[2].second.m_messageNumber)
                && !got[0].second.m_duplicate && got[1].second.m_duplicate && got[2].second.m_duplicate;
        }
        std::string why;
        for (const auto& g : got) {
            why += " " + std::to_string(g.first) + ":N" + std::to_string(g.second.m_messageNumber) + "R" + std::to_string(g.second.m_retrieval)
                + "d" + std::to_string(g.second.m_duplicate);
        }
        check(ok, ("retransmission: after the send delay, twice again at the interval with the number and R = 0, the id sent once ("
            + why + ")").c_str());
    }

    // A message in fragments is sent again whole, its first fragment in a frame of the pager
    {
        Enc enc;
        std::string error;
        enc.setCollapse(2, error);
        std::string chars;
        for (int i = 0; i < 600; i++) {
            chars += (char) ('a' + (i % 26));
        }
        Enc::TxMessage m = msg(chars);
        m.m_retransmit = 1;
        m.m_retransmitInterval = 6;
        enc.enqueue(m);
        Dec dec;
        std::vector<Dec::Message> whole;
        for (uint64_t index = 0; (index < 400) && (enc.pending() > 0); index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            for (const Dec::Message& r : dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data)))
            {
                if (r.m_complete && (r.m_text == chars)) {
                    whole.push_back(r);
                }
            }
        }
        bool ok = (whole.size() == 2) && whole[0].m_retrieval && !whole[1].m_retrieval
            && (whole[0].m_messageNumber == whole[1].m_messageNumber) && whole[1].m_duplicate;
        check(ok, "retransmission: a message in fragments is sent again whole, the copy a duplicate");
    }

    // Refused: messages without numbers, a network without numbering, out of range
    {
        Enc enc;
        std::string error;
        Enc::TxMessage tone = msg("");
        tone.m_type = Flex::Tone;
        tone.m_retransmit = 1;
        Enc::TxMessage many = msg("x");
        many.m_retransmit = 16;
        Enc::TxMessage late = msg("x");
        late.m_sendDelay = 1921;
        bool ok = !enc.enqueue(tone).m_ok && !enc.enqueue(many).m_ok && !enc.enqueue(late).m_ok;
        Enc::Numbering numbering;
        numbering.m_enabled = false;
        enc.setNumbering(numbering, error);
        Enc::TxMessage again = msg("x");
        again.m_retransmit = 1;
        ok = ok && !enc.enqueue(again).m_ok;
        check(ok, "retransmission: refused for messages without numbers, a network without numbering and out of range");
    }
}

void testTime()
{
    // Fig. 3.7.2-1: date word of December 31, 1999, the 32 bits in transmission order
    const char *fig = "110110010100111110011" "1111100011" "0";
    uint32_t expected = 0;
    for (int i = 0; i < 32; i++) {
        expected |= (uint32_t) (fig[i] - '0') << (31 - i);
    }
    check(Flex::dateWord(1999, 12, 31) == expected, "time: date word of Fig. 3.7.2-1 (December 31, 1999), all 32 bits");

    // Fields of the time and zone words
    uint32_t t = Flex::wordData(Flex::timeWord(23, 59, 7));
    check(Flex::checksumValid(t) && (((t >> 4) & 7) == 2) && (((t >> 7) & 0x1f) == 23) && (((t >> 12) & 0x3f) == 59) && (((t >> 18) & 7) == 7),
        "time: time word, type 010, H bits 7-11, M bits 12-17, s bits 18-20");
    uint32_t z = Flex::wordData(Flex::timeZoneWord(9, true, 5));
    check(Flex::checksumValid(z) && (((z >> 4) & 7) == 5) && (((z >> 7) & 0xf) == 5) && (((z >> 11) & 0x1f) == 9)
        && (((z >> 16) & 1) == 0) && (((z >> 17) & 1) == 0) && (((z >> 18) & 7) == 5),
        "time: zone word, type 101 A = 0101, Z bits 11-15, L0 = 0 for daylight saving, reserved 0, S5-S3 bits 18-20");
    check(((Flex::wordData(Flex::timeZoneWord(9, false, 0)) >> 16) & 1) == 1, "time: zone word, L0 = 1 for standard time");
    // Year field: 1994-2025, then modulo 32
    check((((Flex::wordData(Flex::dateWord(1994, 1, 1)) >> 7) & 0x1f) == 0) && (((Flex::wordData(Flex::dateWord(2025, 1, 1)) >> 7) & 0x1f) == 31)
        && (((Flex::wordData(Flex::dateWord(2026, 1, 1)) >> 7) & 0x1f) == 0), "time: year field 1994 = 0, 2025 = 31, 2026 = 0 (modulo 32)");

    // Table 3.7.2-3
    struct Zone { unsigned m_code; int m_minutes; };
    const Zone zones[] = {{0, 0}, {9, 540}, {12, 720}, {13, 210}, {15, 330}, {17, 345}, {18, 390}, {19, 570}, {20, -210},
        {21, -660}, {27, -300}, {31, -60}};
    bool zonesOk = true;
    for (const Zone& zz : zones)
    {
        int minutes = 1;
        zonesOk = zonesOk && Flex::timeZoneMinutes(zz.m_code, minutes) && (minutes == zz.m_minutes) && (Flex::timeZoneCode(zz.m_minutes) == (int) zz.m_code);
    }
    int undefinedMinutes = 1;
    check(zonesOk && !Flex::timeZoneMinutes(16, undefinedMinutes) && (Flex::timeZoneCode(123) == -1),
        "time: zone table 3.7.2-3, code 10000 not defined, an offset not in the table refused");

    // Civil date and time of epoch seconds (reference values)
    struct Civil { int64_t m_seconds; int m_year; unsigned m_month, m_day, m_hour, m_minute, m_second; };
    const Civil civil[] = {
        {0, 1970, 1, 1, 0, 0, 0}, {946684800, 2000, 1, 1, 0, 0, 0}, {951782400, 2000, 2, 29, 0, 0, 0},
        {1700000000, 2023, 11, 14, 22, 13, 20}, {4102444800LL, 2100, 1, 1, 0, 0, 0}, {-1, 1969, 12, 31, 23, 59, 59}};
    bool civilOk = true;
    for (const Civil& c : civil)
    {
        int y; unsigned mo, d, h, mi, s;
        Flex::civilTime(c.m_seconds, y, mo, d, h, mi, s);
        civilOk = civilOk && (y == c.m_year) && (mo == c.m_month) && (d == c.m_day) && (h == c.m_hour) && (mi == c.m_minute) && (s == c.m_second);
    }
    check(civilOk, "time: civil date and time of epoch seconds (1970, 2000, leap day 2000, 2023, 2100, before the epoch)");

    // Encoder to decoder: zone +9 h, daylight saving, the offset sets 2026-11-11 13:28 local at frame 0 of
    // the cycle. Frame index 1920 * H is the start of an hour of the grid (cycle 0 frame 0)
    for (Flex::Mode mode : {Flex::Mode1600_2, Flex::Mode6400_4})
    {
        Enc enc;
        std::string error;
        enc.setMode(mode);
        Enc::TimeService ts;
        ts.m_enabled = true;
        ts.m_zoneMinutes = 540;
        ts.m_daylightSaving = true;
        const uint64_t hourIndex = 1920ull * 400000;      // UTC hour 400000 from the epoch
        // Wanted local time of the hour start: 2026-11-11 13:00:00. 2026-01-01 is day 20454 from the epoch (56
        // years, 14 leap days), November 11 is day 314 of 2026: day 20768
        const int64_t wanted = 20768LL * 86400 + 13 * 3600;
        ts.m_offsetSeconds = wanted - (int64_t) (hourIndex * 1875 / 1000) - (540 + 60) * 60;
        bool set = enc.setTimeService(ts, error);

        Dec dec;
        dec.setReferenceYear(2026);
        // Cycle 7 of the hour: frame 0 of the cycle is 13:28 local
        const uint64_t start = hourIndex + 7 * 128;
        bool allPhases = true;
        // 40 frames: the decoder tells the time of frame 0 from the time of the frame by observations 5 frames apart
        for (uint64_t index = start; index < start + 40; index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));

            for (int p = 0; p < Flex::phaseCount(mode); p++)
            {
                std::vector<uint32_t> extra = enc.extraBlockInfo(index, p);
                allPhases = allPhases && (extra.size() == 3);
            }
        }

        Dec::NetworkTime nt = dec.networkTime(false);
        bool ok = set && allPhases && nt.m_dateValid && (nt.m_year == 2026) && (nt.m_month == 11) && (nt.m_day == 11)
            && nt.m_timeValid && (nt.m_hour == 13) && (nt.m_minute == 28) && (nt.m_second7_5 == 0)
            && nt.m_zoneValid && nt.m_zoneDefined && (nt.m_zoneMinutes == 540) && (nt.m_dst == 0) && (nt.m_mode == 0);
        std::string label = std::string("time ") + Flex::modeName(mode)
            + ": date, time of frame 0, zone +9 h daylight saving decoded; the three words in every phase";
        if (!ok)
        {
            label += " (date " + std::to_string(nt.m_year) + "-" + std::to_string(nt.m_month) + "-" + std::to_string(nt.m_day)
                + " time " + std::to_string(nt.m_hour) + ":" + std::to_string(nt.m_minute) + " s " + std::to_string(nt.m_second7_5)
                + " zone " + std::to_string(nt.m_zoneMinutes) + " dst " + std::to_string(nt.m_dst) + " mode " + std::to_string(nt.m_mode) + ")";
        }
        check(ok, label.c_str());
    }

    // A network that sends the time in cycle 0 only (the least, STD-43A 3.7.2: once an hour): the time goes on with
    // the frames, right in the later cycles too (was: the frame offset in the cycle only, 4 minutes behind for each
    // cycle since), and a time not received again for over an hour is forgotten
    {
        Enc enc;
        std::string error;
        enc.setMode(Flex::Mode1600_2);
        Enc::TimeService ts;
        ts.m_enabled = true;
        ts.m_zoneMinutes = 540;
        ts.m_daylightSaving = true;
        const uint64_t hourIndex = 1920ull * 400000;
        const int64_t wanted = 20768LL * 86400 + 13 * 3600;     // 2026-11-11 13:00:00 local at cycle 0 frame 0
        ts.m_offsetSeconds = wanted - (int64_t) (hourIndex * 1875 / 1000) - (540 + 60) * 60;
        bool set = enc.setTimeService(ts, error);
        Dec dec;
        dec.setReferenceYear(2026);

        for (uint64_t index = hourIndex; index < hourIndex + 40; index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
        }

        // No more time words; a frame of cycle 3: 394 frames (12 min 18.75 s) after 13:00:00
        ts.m_enabled = false;
        set = set && enc.setTimeService(ts, error);
        Enc::Frame later = enc.buildFrame(hourIndex + 3 * 128 + 10);
        dec.decodeFrame(toRxFrame(later.m_mode, later.m_inverted, later.m_bytes.m_sync, later.m_bytes.m_data));
        Dec::NetworkTime nt = dec.networkTime(false);
        check(set && nt.m_timeValid && (nt.m_mode == 0) && nt.m_modeConfirmed && (nt.m_currentHour == 13)
            && (nt.m_currentMinute == 12) && (std::fabs(nt.m_currentSecond - 18.75) < 0.01) && (nt.m_timeAge == 3 * 128 + 10 - 39),
            "time: sent in cycle 0 only, right in cycle 3 (13:12:18.75), the time of frame 0 confirmed, its age in frames");

        // An hour and more later without the time: forgotten
        Enc::Frame hourLater = enc.buildFrame(hourIndex + 1920 + 50);
        dec.decodeFrame(toRxFrame(hourLater.m_mode, hourLater.m_inverted, hourLater.m_bytes.m_sync, hourLater.m_bytes.m_data));
        nt = dec.networkTime(false);
        check(!nt.m_timeValid && !nt.m_dateValid && !nt.m_zoneValid, "time: not received for over an hour, forgotten");
    }

    // A time received in one frame only is assumed to be the time of frame 0 (standard) until the mode is told
    {
        Dec::NetworkTime fresh = Dec().networkTime(false);
        check((fresh.m_mode == 0) && !fresh.m_modeConfirmed, "time: standard assumed, not confirmed, before any time word");
    }

    // Roaming in frames 0-3 takes 2 block words in its phase: one time word a frame, the three in turn; frame 0 of
    // cycle 0 has a time word in every phase
    {
        Enc enc;
        std::string error;
        enc.setMode(Flex::Mode6400_4);
        Enc::Roaming r;
        r.m_enabled = true;
        r.m_localId = 5;
        enc.setRoaming(r);
        Enc::TimeService ts;
        ts.m_enabled = true;
        enc.setTimeService(ts, error);

        int ssidPhase = Flex::ssidPhase(Flex::Mode6400_4, 0);
        std::vector<unsigned> types;
        bool everyPhase = true;
        for (uint64_t index = 0; index < 4; index++)
        {
            for (int p = 0; p < 4; p++)
            {
                std::vector<uint32_t> extra = enc.extraBlockInfo(index, p);
                bool hasTime = false;
                for (uint32_t w : extra)
                {
                    unsigned type = (Flex::wordData(w) >> 4) & 7;
                    hasTime = hasTime || (type == 1) || (type == 2) || (type == 5);
                    if ((p == Flex::ssidPhase(Flex::Mode6400_4, (unsigned) index)) && (type != 0) && (type != 7)) {
                        types.push_back(type);
                    }
                }
                everyPhase = everyPhase && hasTime && (extra.size() <= 3);
            }
        }
        (void) ssidPhase;
        bool rotation = (types.size() >= 3) && (types[0] != types[1]) && (types[1] != types[2]) && (types[0] != types[2]);
        check(everyPhase && rotation,
            "time: with roaming in frames 0-3 the free block word carries date, time and zone in turn; a time word in every phase");
    }

    // A zone offset not in the table is refused
    {
        Enc enc;
        std::string error;
        Enc::TimeService ts;
        ts.m_enabled = true;
        ts.m_zoneMinutes = 61;
        check(!enc.setTimeService(ts, error) && !error.empty(), "time: zone offset not in Table 3.7.2-3 refused");
        ts.m_zoneMinutes = 0;
        ts.m_extendedSeconds = 8;
        check(!enc.setTimeService(ts, error), "time: extended seconds over 7 refused");
    }

    // Equivalent year: the year itself in 1994-2025, else the latest year of the period with the same calendar
    check((Flex::equivalentYear(2024) == 2024) && (Flex::equivalentYear(2026) == 2015) && (Flex::equivalentYear(2028) == 2000)
        && (Flex::equivalentYear(1990) == 2018), "time: equivalent years (2026 as 2015, leap 2028 as 2000, 1990 as 2018)");

    // The equivalent year and the extended second correction (S5-S3) as set are decoded
    {
        Enc enc;
        std::string error;
        enc.setMode(Flex::Mode1600_2);
        Enc::TimeService ts;
        ts.m_enabled = true;
        ts.m_equivalentYear = true;
        ts.m_extendedSeconds = 5;
        const uint64_t hourIndex = 1920ull * 400000;
        ts.m_offsetSeconds = 20768LL * 86400 + 13 * 3600 - (int64_t) (hourIndex * 1875 / 1000);
        bool set = enc.setTimeService(ts, error);

        Dec dec;
        dec.setReferenceYear(2015);
        for (uint64_t index = hourIndex; index < hourIndex + 40; index++)
        {
            Enc::Frame f = enc.buildFrame(index);
            dec.decodeFrame(toRxFrame(f.m_mode, f.m_inverted, f.m_bytes.m_sync, f.m_bytes.m_data));
        }

        Dec::NetworkTime nt = dec.networkTime(false);
        check(set && nt.m_dateValid && (nt.m_year == 2015) && (nt.m_month == 11) && (nt.m_day == 11) && nt.m_zoneValid
            && (nt.m_extendedSeconds == 5), "time: 2026 sent as the equivalent year 2015, the extended second correction 5");
    }
}

int main()
{
    check((Flex::addressText(1234567) == "0001234567") && (Flex::addressText(4297068542ULL) == "4297068542"),
        "record address: capcode in 10 digits");
    testTime();
    testNumbering();
    testRetransmission();
    testGroupTx();
    testAlphaMailDrop();
    testNumericFormats();
    testFragmentScheduling();
    testFragmentsFitTheFrame();
    testBinaryTx();
    testHexStatus();
    testHexBits();
    testReferenceFrames();
    testRoaming();
    testShortMessage();
    testVectorOfNoWords();
    testBiw();
    testRoundTrip();
    testPolarityReports();
    testErrors();
    testFragments();
    testDuplicates();
    testGroup();
    testSubframes();
    testMisc();

    std::printf("%d checks passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
