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

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "radiopagingcharset.h"
#include "radiopaginggsc.h"
#include "radiopagingkanji.h"
#include "radiopagingpocsag.h"
#include "radiopagingrecode.h"
#include "radiopagingpage.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

typedef RadioPagingCharset Cs;

// A codepage of a protocol: vendor, model, codepage
const Cs::Codepage *find(const char *protocol, const char *vendor, const char *model, const char *codepage)
{
    return Cs::find(protocol, Cs::Key{vendor, model, codepage});
}

std::vector<uint8_t> encode(const Cs::Codepage *codepage, const std::string& text, bool *ok = nullptr)
{
    std::vector<uint8_t> codes;
    bool done = Cs::encode(*codepage, text, codes);
    if (ok) {
        *ok = done;
    }
    return codes;
}

std::string decodeText(const Cs::Codepage& codepage, const std::string& codes)
{
    return Cs::decode(codepage, std::vector<uint8_t>(codes.begin(), codes.end()));
}

} // namespace

int main()
{
    // The codepages: 26 POCSAG, 14 FLEX, 1 GSC, keys unique within the protocol, each found by its key, generic ASCII
    // the default
    std::set<std::string> keys;
    bool found = true;

    for (const Cs::Codepage& c : Cs::codepages())
    {
        keys.insert(std::string(c.m_protocol) + "/" + c.m_vendor + "/" + c.m_model + "/" + c.m_codepage);
        found = found && (Cs::find(c.m_protocol, c.key()) == &c) && c.m_table && c.m_label;
    }

    check((Cs::codepages("pocsag").size() == 28) && (Cs::codepages("flex").size() == 17) && (Cs::codepages("gsc").size() == 1)
        && (keys.size() == Cs::codepages().size()) && found, "46 codepages, keys unique in each protocol, each found by its key");
    check(encode(find("pocsag", "generic", "generic", "hebrew"), "\xD7\x90\xD7\xAA" "A") == std::vector<uint8_t>({0x60, 0x7A, 0x41}),
        "POCSAG Hebrew: alef 0x60, tav 0x7A, ASCII kept");
    {
        QString text = "ab<0x03>c<...>", status = ".cXXXXXX.XXXXX";
        RadioPagingRecode::reverseText(text, status);
        check((text == "<...>c<0x03>ba") && (status == "XXXXX.XXXXXXc."), "right-to-left: codes and lost fragments kept whole, status with them");
    }
    {
        RadioPagingCustomCodepage::Codes codes;
        bool rtl;
        std::string error;
        check(RadioPagingCustomCodepage::parse("pocsag", "rtl 60=05d0, 41=0416", codes, rtl, error) && rtl && (codes.size() == 2)
            && (RadioPagingCustomCodepage::text(codes, rtl) == "rtl 60=05D0 41=0416"), "custom codepage: codes and direction, as text");
        check(!RadioPagingCustomCodepage::parse("pocsag", "80=0041", codes, rtl, error)
            && !RadioPagingCustomCodepage::parse("gsc", "40=0041", codes, rtl, error)
            && !RadioPagingCustomCodepage::parse("pocsag", "41=D800", codes, rtl, error)
            && !RadioPagingCustomCodepage::parse("pocsag", "41", codes, rtl, error), "custom codepage: a code out of the protocol, not a character refused");
        RadioPagingCustomCodepage custom("pocsag", "rtl 60=05D0");
        check((decodeText(custom.codepage(), "`A") == "\xD7\x90" "A") && custom.codepage().m_rtl && RadioPagingCustomCodepage::isCustom(custom.codepage().key())
            && (encode(&custom.codepage(), "\xD7\x90" "B") == std::vector<uint8_t>({0x60, 0x42})), "custom codepage: ASCII with the codes mapped, both ways");
        RadioPagingCustomCodepage gsc("gsc", "");
        check(gsc.codepage().codes() == 64, "custom codepage of GSC: its 6-bit table");

        MessageRecord flexRx;
        flexRx.m_direction = MessageRecord::DirectionRX;
        flexRx.m_protocol = "flex";
        flexRx.m_msgType = "alphanumeric";
        flexRx.m_protocolData.insert("alpha_codes", QString("6041"));
        flexRx.m_protocolData.insert("custom_codes", QString("rtl 60=05D0"));
        MessageRecord out;
        QString recodeError;
        check(RadioPagingRecode::redecode(flexRx, RadioPagingCustomCodepage::key(), QString(), out, recodeError)
            && (out.m_text == QString::fromUtf8("A\xD7\x90")), "decode again with the custom codepage of the record, right-to-left");
        flexRx.m_protocolData.remove("custom_codes");
        check(!RadioPagingRecode::redecode(flexRx, RadioPagingCustomCodepage::key(), QString(), out, recodeError),
            "decode again: no custom codepage kept, refused");
    }
    const Cs::Key ascii0{"generic", "generic", "ascii"};
    check((Cs::defaultFor("pocsag")->key() == ascii0) && (Cs::defaultFor("flex")->key() == ascii0)
        && (Cs::defaultFor("gsc")->key() == ascii0) && (Cs::defaultFor("ermes") == nullptr),
        "default codepage of each protocol: generic, generic, ascii");
    check((find("golay", "GENERIC", "generic", "ascii") == Cs::defaultFor("gsc")) && (Cs::codepages("golay").size() == 1)
        && (find("pocsag", "generic", "generic", "none") == nullptr), "keys not case sensitive, golay is gsc, an unknown key is none");
    check((find("flex", "generic", "generic", "latin-german") == nullptr) && (find("pocsag", "motorola", "linguist", "cyrillic") == nullptr)
        && (find("flex", "motorola", "linguist", "cyrillic") != nullptr), "a codepage belongs to its protocol");
    check((find("pocsag", "partner", "generic", "cyrillic") != nullptr) && (find("pocsag", "multitone", "generic", "ascii") != nullptr)
        && (find("pocsag", "nec", "maxima-compact", "cyrillic") != nullptr), "the Partner, Multitone and NEC Maxima Compact codepages are there");

    // Decode: glyphs, <0xNN> for the codes without one; the code masked to the codepage
    const Cs::Codepage& ascii = *Cs::defaultFor("pocsag");
    check(Cs::decode(ascii, {0x48, 0x49, 0x00, 0x03, 0x7F, 0x0A, 0x0D}) == "HI<0x00><0x03><0x7F>\xE2\x90\x8A\xE2\x90\x8D",
        "ASCII decode: glyphs, <0xNN> for the controls and DEL, the symbols for LF and CR");
    const Cs::Codepage& german = *find("pocsag", "generic", "generic", "latin-german");
    check(Cs::decode(german, {0x5B, 0x7E}) == "\xC3\x84\xC3\x9F", "German decode: 0x5B A umlaut, 0x7E sharp s");
    const Cs::Codepage& gsc = *Cs::defaultFor("gsc");
    check((gsc.codes() == 64) && (Cs::decode(gsc, {0x21, 0x3C, 0x3E}) == "A\xE2\x90\x8D<0x3E>"),
        "GSC: 64 codes, A, CR/LF as the symbol for CR, the fill without a glyph");

    // Encode: the glyph or an alias, <0xNN> for any code, the first character without a code refused
    bool ok;
    check(encode(find("pocsag", "generic", "generic", "ascii-uppercase"), "ab") == std::vector<uint8_t>({0x41, 0x42}),
        "uppercase pager: lowercase encoded on the uppercase codes");
    check(encode(find("pocsag", "motorola", "advisor-linguist", "latin-cyrillic"), "\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD1\x96\xD1\x82")
        == std::vector<uint8_t>({0x6B, 0x50, 0x68, 0x42, 0x49, 0x54}),
        "Linguist: Ukrainian pryvit, pe and y on their own codes, er ve i te on the Latin P B I T");
    check(encode(&ascii, "A<0x04>B<0x7F>") == std::vector<uint8_t>({0x41, 0x04, 0x42, 0x7F}),
        "<0xNN> encodes any code");
    std::vector<uint8_t> refused = encode(&ascii, "A\xC3\x84", &ok);
    std::string error;
    std::vector<uint8_t> codes;
    Cs::encode(ascii, "A\xC3\x84", codes, &error);
    check(!ok && (error.find("\xC3\x84") != std::string::npos), "a character without a code is refused, named in the error");
    check(encode(Cs::defaultFor("gsc"), "hi\n") == std::vector<uint8_t>({0x28, 0x29, 0x3C}), "GSC: lowercase taken as uppercase, LF as CR");
    check(encode(Cs::defaultFor("gsc"), "A\xE2\x90\x8D" "B\r" "C\xE2\x90\x8A" "D\xE2\x90\xA4")
        == std::vector<uint8_t>({0x21, 0x3C, 0x22, 0x3C, 0x23, 0x3C, 0x24, 0x3C}),
        "GSC: CR/LF from the symbols for CR, LF and newline and from CR");
    check(encode(&ascii, "A\xE2\x90\x8D" "B\xE2\x90\x8A" "C\r\n") == std::vector<uint8_t>({0x41, 0x0D, 0x42, 0x0A, 0x43, 0x0D, 0x0A}),
        "ASCII: CR and LF from their symbols and from the characters");
    check(Cs::decode(*find("pocsag", "generic", "generic", "ascii-uppercase"), {0x0D, 0x0A}) == "\xE2\x90\x8D\xE2\x90\x8A",
        "uppercase ASCII: the symbols for CR and LF");
    check(Cs::decode(*find("pocsag", "motorola", "advisor-linguist", "latin-cyrillic"), {0x0D, 0x0A}) == "\xE2\x90\x8D\xE2\x90\x8A",
        "Linguist Latin/Cyrillic: the symbols for CR and LF");
    check(encode(find("pocsag", "nec", "maxima-compact", "cyrillic"), "\xD0\x90\xD0\x91\xD0\x95\xD0\x81") == std::vector<uint8_t>({0x41, 0x61, 0x45, 0x65}),
        "NEC Maxima/Compact (the 21A layout): A and E on the Latin codes, Be on 0x61, Io on 0x65");
    check(encode(find("flex", "motorola", "linguist", "arabic"), "\xD9\x84\xD8\xA7\xD9\x84") == std::vector<uint8_t>({0x5B, 0x64}),
        "Arabic: lam-alef on its code 0x5B, a lam alone on 0x64");

    // Round trip of every code with a glyph of one character, for every codepage: the glyph encodes to a code that
    // has it as its glyph or an alias (a character on two codes takes the first: the backtick of an uppercase pager
    // is sent as its apostrophe)
    bool roundTrip = true;

    for (const Cs::Codepage& c : Cs::codepages())
    {
        for (int code = 0; code < c.codes(); code++)
        {
            const char *glyph = c.glyph(code);

            if (!glyph || (std::string(glyph).size() > 4) || (glyph[0] == 0)) {
                continue;
            }

            size_t pos = 0;
            int back = Cs::encodeOne(c, glyph, pos);
            bool listed = false;

            for (const std::string& g : (back >= 0) ? c.glyphs(back) : std::vector<std::string>()) {
                listed = listed || (g == glyph);
            }

            roundTrip = roundTrip && listed;
        }
    }

    check(roundTrip, "every glyph of every codepage encodes to a code that has it");

    // The codepage in the encoders and decoders: the codes of a text sent as the message bits, decoded back with the
    // codepage from the bits as the decoders store them (8 a byte, the first bit the most significant of byte 0)
    auto pack = [](const std::vector<uint8_t>& bits) {
        std::vector<uint8_t> packed((bits.size() + 7) / 8, 0);
        for (size_t i = 0; i < bits.size(); i++) {
            packed[i / 8] |= (uint8_t) (bits[i] << (7 - (i % 8)));
        }
        return packed;
    };

    const std::string grusse = "Gr\xC3\xBC\xC3\x9F" "e";
    RadioPagingPocsag::TxMessage pocsag;
    pocsag.m_type = RadioPagingPocsag::Alphanumeric;
    codes = encode(&german, grusse);
    pocsag.m_text.assign(codes.begin(), codes.end());
    std::vector<uint8_t> bits;
    check(RadioPagingPocsag::messageBits(pocsag, RadioPagingPocsag::EncodeOptions(), bits, error) && (bits.size() == 40),
        "POCSAG: 5 characters of 7 bits in 2 codewords of 20 message bits");
    RadioPagingPocsag::Interpretation pi = RadioPagingPocsag::interpret(pack(bits), (int) bits.size(), &german);
    check((pi.m_type == "alphanumeric") && (pi.m_text == grusse), "POCSAG: German text back from the message bits with its codepage");
    check(RadioPagingPocsag::interpret(pack(bits), (int) bits.size()).m_text == std::string(codes.begin(), codes.end()),
        "POCSAG: without a codepage the text is the 7-bit characters");

    // POCSAG fill: the last character and the one before if it is the same code (the characters of the message bits
    // as received, no padding)
    auto alphaText = [&](const std::string& text, const std::vector<uint8_t>& fill) {
        std::vector<uint8_t> b;
        for (char c : text) {
            for (int i = 0; i < 7; i++) {
                b.push_back(((unsigned char) c >> i) & 1);
            }
        }
        return RadioPagingPocsag::decodeAlphanumeric(pack(b), (int) b.size(), fill);
    };
    check((alphaText("AB\x03", RadioPagingPocsag::fillMinimal()) == "AB\x03") && (alphaText("AB\x03", RadioPagingPocsag::fillConservative()) == "AB"),
        "POCSAG fill: ETX kept as NUL fill only, dropped as conservative fill");
    check(alphaText(std::string("AB\x04\x04", 4), RadioPagingPocsag::fillConservative()) == "AB",
        "POCSAG fill: two characters of the same fill code dropped");
    check(alphaText(std::string("A\x04\x04\x04", 4), RadioPagingPocsag::fillConservative()) == std::string("A\x04", 2),
        "POCSAG fill: no more than two characters dropped");
    check(alphaText(std::string("A\x03\x04", 3), RadioPagingPocsag::fillConservative()) == std::string("A\x03", 2),
        "POCSAG fill: the one before only if it is the same code");

    RadioPagingGsc::TxMessage gscMessage;
    gscMessage.m_type = RadioPagingGsc::Alphanumeric;
    codes = encode(&gsc, "@HI[]_");
    gscMessage.m_text.assign(codes.begin(), codes.end());
    gscMessage.m_codes = true;
    bits.clear();
    check(RadioPagingGsc::messageBits(gscMessage, bits, error) && (bits.size() == 48), "GSC: 6 codes of 6 bits padded to a block of 48 bits");
    RadioPagingGsc::Interpretation gi = RadioPagingGsc::interpret(pack(bits), (int) bits.size(), &gsc);
    check((gi.m_type == "alphanumeric") && (gi.m_text == "@HI[]_"), "GSC: Table VII glyphs back from the message bits, Null fill dropped");
    std::vector<uint8_t> asciiBits;
    RadioPagingGsc::TxMessage gscAscii = gscMessage;
    gscAscii.m_text = "HI";
    gscAscii.m_codes = false;
    gscMessage.m_text = std::string("\x28\x29", 2);
    bits.clear();
    check(RadioPagingGsc::messageBits(gscAscii, asciiBits, error) && RadioPagingGsc::messageBits(gscMessage, bits, error) && (bits == asciiBits),
        "GSC: the codes of a codepage are the bits of the same ASCII text");
    gscMessage.m_text = std::string(1, (char) 0x40);
    bits.clear();
    check(!RadioPagingGsc::messageBits(gscMessage, bits, error), "GSC: a code above 0x3F is refused");

    // BeepwearPRO alarm command: key=value to the ASCII of the command and back
    const Cs::Codepage *beepwear = find("flex", "timex", "beepwear-pro", "alarm");
    auto alarm = [&](const std::string& text, std::string *warning = nullptr) {
        std::vector<uint8_t> c;
        std::string e;
        return Cs::encode(*beepwear, text, c, &e, warning) ? std::string(c.begin(), c.end()) : "refused: " + e;
    };
    auto received = [&](const std::string& text) { return Cs::decode(*beepwear, std::vector<uint8_t>(text.begin(), text.end())); };
    std::string warning;

    check((beepwear != nullptr) && (beepwear->m_transcoder != nullptr), "BeepwearPRO alarm: a FLEX codepage with a special format");
    check(alarm("hour=7 min=0 msg=Wake up") == "///A1107000000WAKE UP17", "BeepwearPRO alarm: daily, the defaults (arm 1, month 0, day 0)");
    check(alarm("arm=1 hour=10 min=0 day=15 msg=Call Mom") == "///A110:00000?CALL MOM61", "BeepwearPRO alarm: monthly, 4 bits + 0x30 per character");
    check(alarm("hour=0 min=1 month=12 day=25 msg=Merry Xmas") == "///A1100010<19MERRY XMAS22", "BeepwearPRO alarm: yearly");
    check((alarm("arm=0 hour=7 min=0 msg=ABCDEFGHIJKLMNOPQRS", &warning).size() == 3 + 11 + 16 + 2) && (warning.find("QRS") != std::string::npos),
        "BeepwearPRO alarm: msg over 16 characters cut, with a warning");
    check((alarm("hour=24 min=0 msg=X").compare(0, 8, "refused:") == 0) && (alarm("hour=7 msg=X").compare(0, 8, "refused:") == 0)
        && (alarm("hour=7 min=0 sound=1 msg=X").compare(0, 8, "refused:") == 0) && (alarm("hour=7 min=0 month=3 msg=X").compare(0, 8, "refused:") == 0)
        && (alarm("hour 7").compare(0, 8, "refused:") == 0), "BeepwearPRO alarm: out of range, missing, unknown keys, a yearly alarm without day, not key=value refused");
    check(received("///A1107000000WAKE UP17") == "arm=1 hour=7 min=0 month=0 day=0 msg=WAKE UP", "BeepwearPRO alarm received as key=value");
    check(received("///A1107000000WAKE UP18") == "error=checksum raw=///A1107000000WAKE UP18", "BeepwearPRO alarm: a bad checksum is a bad message");
    check((received("///B12345") == "error=unknown_command raw=///B12345") && (received("HELLO") == "HELLO"),
        "BeepwearPRO: another command is unknown, a message without /// is shown as it is");

    // A received record decoded again: POCSAG with another codepage and as numeric, FLEX from its characters
    MessageRecord rx;
    rx.m_direction = MessageRecord::DirectionRX;
    rx.m_protocol = "pocsag";
    rx.m_msgType = "alphanumeric";
    codes = encode(&german, grusse);
    pocsag.m_text.assign(codes.begin(), codes.end());
    bits.clear();
    RadioPagingPocsag::messageBits(pocsag, RadioPagingPocsag::EncodeOptions(), bits, error);
    const std::vector<uint8_t> packed = pack(bits);
    rx.m_rawData = QByteArray((const char *) packed.data(), (int) packed.size());
    rx.m_rawBits = (int) bits.size();
    rx.m_text = "Gr}~e";
    rx.m_protocolData.insert("charset", RadioPagingPage::charsetData(ascii.key()));
    MessageRecord out;
    QString recodeError;
    check(RadioPagingRecode::redecode(rx, german.key(), QString(), out, recodeError) && (out.m_text == QString::fromUtf8(grusse.c_str()))
        && (RadioPagingPage::charsetKey(out.m_protocolData.value("charset")) == german.key()) && out.m_protocolData.value("redecoded").toBool(),
        "decode again: POCSAG with the German codepage, recorded");
    check(RadioPagingRecode::redecode(rx, Cs::Key(), "numeric", out, recodeError) && (out.m_msgType == "numeric")
        && (out.m_protocolData.value("alt_type").toString() == "alphanumeric") && (out.m_text != rx.m_text),
        "decode again: POCSAG as numeric, alphanumeric the other candidate");
    check(!RadioPagingRecode::redecode(rx, find("flex", "motorola", "linguist", "cyrillic")->key(), QString(), out, recodeError),
        "decode again: a codepage of another protocol is refused");
    MessageRecord flexRx;
    flexRx.m_direction = MessageRecord::DirectionRX;
    flexRx.m_protocol = "flex";
    flexRx.m_msgType = "alphanumeric";
    flexRx.m_protocolData.insert("alpha_codes", QString("4142"));
    check((RadioPagingRecode::types(flexRx) == QStringList({"alphanumeric"}))
        && RadioPagingRecode::redecode(flexRx, find("flex", "motorola", "linguist", "cyrillic")->key(), QString(), out, recodeError)
        && (out.m_text == QString::fromUtf8("\xD0\xB0\xD0\xB1")), "decode again: FLEX characters with the Cyrillic codepage");
    rx.m_direction = MessageRecord::DirectionTX;
    check(RadioPagingRecode::types(rx).isEmpty(), "decode again: not a transmitted message");

    // ROT-13: the letters turned by 13 places both ways, <0xNN> kept; Kanji: HEX/binary Shift JIS characters (STD-43A
    // 1.3.1), the hex digits least significant first
    const Cs::Codepage *rot = find("pocsag", "generic", "generic", "ascii-rot13");
    check(rot && (encode(rot, "Hello, World<0x04>") == std::vector<uint8_t>({'U', 'r', 'y', 'y', 'b', ',', ' ', 'J', 'b', 'e', 'y', 'q', 0x04}))
        && (Cs::decode(*rot, {'U', 'r', 'y', 'y', 'b', 0x03}) == "Hello<0x03>"), "ROT-13: encoded and decoded, a code given as <0xNN> kept");
    const std::string nihon = "\xE6\x97\xA5\xE6\x9C\xAC";
    std::string hex;
    check(RadioPagingKanji::encode(nihon + "A", 16, hex, error) && (hex == "AF39B7691400"),
        "Kanji: 16-bit Shift JIS characters, a 1-byte character in 16 bits");
    check((RadioPagingKanji::decode("AF39B769", 16) == nihon) && (RadioPagingKanji::decode("B769AF39", 16, true) == nihon),
        "Kanji: decoded left to right and right to left");
    check((RadioPagingKanji::decode("1424", 8) == "AB") && (RadioPagingKanji::decode("AF3?B769", 16) == "?\xE6\x9C\xAC"),
        "Kanji: 8-bit characters; a character with a lost digit is '?'");
    check(!RadioPagingKanji::encode(nihon, 8, hex, error) && !RadioPagingKanji::encode("\xD1\x96", 16, hex, error),
        "Kanji: a 2-byte character in 8 bits and a character without a Shift JIS code refused");
    const Cs::Codepage *kanjiCp = find("flex", "generic", "generic", "kanji");
    check(kanjiCp && kanjiCp->m_transcoder && kanjiCp->m_transcoder->m_binary, "Kanji: a codepage of HEX/binary messages");
    MessageRecord kanjiRx = flexRx;
    kanjiRx.m_msgType = "binary";
    kanjiRx.m_protocolData.remove("alpha_codes");
    kanjiRx.m_protocolData.insert("hex", QString("AF39B769"));
    kanjiRx.m_protocolData.insert("blocking", 16);
    kanjiRx.m_protocolData.insert("display_direction", QString("ltr"));
    check((RadioPagingRecode::types(kanjiRx) == QStringList({"binary"}))
        && RadioPagingRecode::redecode(kanjiRx, kanjiCp->key(), QString(), out, recodeError) && (out.m_text == QString::fromUtf8(nihon.c_str()))
        && RadioPagingRecode::redecode(MessageRecord(out), Cs::defaultFor("flex")->key(), QString(), out, recodeError) && (out.m_text == "AF39B769")
        && !out.m_protocolData.contains("charset"),
        "decode again: HEX/binary as Kanji text and back to hex");
    check(!RadioPagingRecode::redecode(flexRx, kanjiCp->key(), QString(), out, recodeError), "decode again: alphanumeric not as Kanji");

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
