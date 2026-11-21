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

// Self test of the page of the message center and the modulator: its limits, its TX record and its tx action.
// Prints one line per check and returns non-zero if any check fails.

#include <cstdio>

#include "SWGRadioPagingModTx.h"

#include "radiopagingpage.h"
#include "radiopagingflex.h"
#include "radiopagingpocsag.h"
#include "radiopagingrecode.h"
#include "radiopagingcharset.h"
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

bool same(const RadioPagingPage& a, const RadioPagingPage& b)
{
    return (a.m_id == b.m_id) && (a.m_protocol == b.m_protocol) && (a.m_type == b.m_type) && (a.m_address == b.m_address)
        && (a.m_function == b.m_function) && (a.m_priority == b.m_priority) && (a.m_text == b.m_text)
        && (a.m_voiceFile == b.m_voiceFile) && (a.m_speed == b.m_speed) && (a.m_polarity == b.m_polarity)
        && (a.m_shortType == b.m_shortType) && (a.m_source == b.m_source) && (a.m_blocking == b.m_blocking)
        && (a.m_mailDrop == b.m_mailDrop) && (a.m_rightToLeft == b.m_rightToLeft) && (a.m_headerMessage == b.m_headerMessage)
        && (a.m_statusInfo == b.m_statusInfo) && (a.m_secureType == b.m_secureType) && (a.m_retransmit == b.m_retransmit)
        && (a.m_retransmitInterval == b.m_retransmitInterval) && (a.m_sendDelay == b.m_sendDelay) && (a.m_groupMembers == b.m_groupMembers)
        && (a.m_charset == b.m_charset);
}

RadioPagingPage page(const char *protocol, const char *type, const char *address)
{
    RadioPagingPage p;
    p.m_id = "p1";
    p.m_transmitter = "t1";
    p.m_protocol = protocol;
    p.m_type = type;
    p.m_address = address;
    p.m_text = "TEXT";
    return p;
}

// The record and the tx action give the page back
void testRoundTrips()
{
    RadioPagingPage pocsag = page("pocsag", "alphanumeric", "1234567");
    pocsag.m_function = 2;
    pocsag.m_speed = 512;
    pocsag.m_polarity = 1;
    pocsag.m_retransmit = 3;
    pocsag.m_retransmitInterval = 5.0;
    pocsag.m_sendDelay = 2.0;
    pocsag.m_charset = RadioPagingCharset::Key{"generic", "generic", "latin-german"};

    RadioPagingPage binary = page("flex", "binary", "2101249");
    binary.m_priority = true;
    binary.m_blocking = 8;
    binary.m_mailDrop = true;
    binary.m_rightToLeft = true;
    binary.m_headerMessage = true;
    binary.m_statusInfo = true;
    binary.m_text = "A5A5";

    RadioPagingPage shortMessage = page("flex", "short_message", "1000");
    shortMessage.m_shortType = 2;
    shortMessage.m_source = 5;
    shortMessage.m_text = "";

    RadioPagingPage secure = page("flex", "secure", "1001");
    secure.m_secureType = 2;
    secure.m_text = "123";

    RadioPagingPage voice = page("gsc", "voice", "123456");
    voice.m_function = 3;
    voice.m_text = "";
    voice.m_voiceFile = "/a/b.wav";

    RadioPagingPage gsc = page("gsc", "numeric", "001234");
    gsc.m_function = 1;

    RadioPagingPage group = page("flex", "alphanumeric", "0");
    group.m_groupMembers = "1000,2101249,5000";
    group.m_mailDrop = true;

    for (const RadioPagingPage& p : {pocsag, binary, shortMessage, secure, voice, gsc, group})
    {
        MessageRecord r = p.toRecord();
        RadioPagingPage fromRecord = RadioPagingPage::fromRecord(r);
        check(p.check().isEmpty(), (p.m_protocol + " " + p.m_type + ": the page is valid").toUtf8().constData());
        check(same(p, fromRecord) && (fromRecord.m_transmitter == "t1") && (r.m_direction == MessageRecord::DirectionTX),
            (p.m_protocol + " " + p.m_type + ": the record gives the page back").toUtf8().constData());

        SWGSDRangel::SWGRadioPagingModTx tx;
        p.toTx(tx);
        RadioPagingPage fromTx = RadioPagingPage::fromTx(tx, "flex");
        fromTx.m_transmitter = p.m_transmitter;
        check(same(p, fromTx), (p.m_protocol + " " + p.m_type + ": the tx action gives the page back").toUtf8().constData());
    }

    MessageRecord r = pocsag.toRecord();
    check((r.m_address == "1234567C") && (r.m_protocolData.value("ric").toULongLong() == 1234567)
        && (r.m_speed == 512) && (r.m_polarity == MessageRecord::PolarityInverted),
        "POCSAG record: address with its function letter, ric, speed, polarity");
    r = gsc.toRecord();
    check((r.m_address == "001234/2") && (r.m_protocolData.value("address_number").toInt() == 2)
        && (r.m_protocolData.value("function").toInt() == 1),
        "GSC record: 6 digits, / and the address number, the function 0-3");
    r = binary.toRecord();
    check((r.m_protocolData.value("blocking").toInt() == 8) && (r.m_protocolData.value("display_direction").toString() == "rtl")
        && r.m_protocolData.value("long_address").toBool() && r.m_protocolData.value("priority").toBool(),
        "FLEX binary record: the keys of the received records");
    r = voice.toRecord();
    check(r.m_text.isNull() && (r.m_protocolData.value("voice_source").toString() == "/a/b.wav"),
        "GSC voice record: no text, the sound file as voice_source");
    r = group.toRecord();
    check((r.m_address == "0000001000,0002101249,0000005000") && (r.m_protocolData.value("group_members").toList().size() == 3)
        && !r.m_protocolData.contains("capcode"), "FLEX group record: the pagers, no capcode");

    // Without a protocol the tx action is of the channel's
    SWGSDRangel::SWGRadioPagingModTx bare;
    bare.setAddress(new QString("42"));
    bare.setType(new QString("tone"));
    RadioPagingPage channel = RadioPagingPage::fromTx(bare, "flex");
    check((channel.m_protocol == "flex") && (channel.m_address == "42") && (channel.m_retransmitInterval < 0.0),
        "tx action without a protocol: the channel protocol, the capcode, the default interval");
}

void testLimits()
{
    struct Case {
        RadioPagingPage m_page;
        const char *m_what;
    };
    std::vector<Case> refused;

    RadioPagingPage p = page("ermes", "alphanumeric", "1");
    refused.push_back({p, "an unknown protocol"});
    p = page("pocsag", "binary", "1");
    refused.push_back({p, "a type the protocol does not send"});
    p = page("pocsag", "voice", "1");
    refused.push_back({p, "POCSAG voice"});
    p = page("pocsag", "alphanumeric", "12a");
    refused.push_back({p, "an address that is not a number"});
    p = page("pocsag", "alphanumeric", "2097152");
    refused.push_back({p, "a POCSAG address over 21 bits"});
    p = page("flex", "alphanumeric", "0");
    refused.push_back({p, "FLEX capcode 0"});
    p = page("gsc", "alphanumeric", "12345678");
    refused.push_back({p, "a GSC cap code of 8 digits"});
    p = page("gsc", "alphanumeric", "000000");
    refused.push_back({p, "GSC address digits not allowed for the group"});
    p = page("pocsag", "alphanumeric", "1");
    p.m_function = 4;
    refused.push_back({p, "function 4"});
    p = page("pocsag", "alphanumeric", "1");
    p.m_polarity = -2;
    refused.push_back({p, "an unknown polarity"});
    p = page("gsc", "voice", "1");
    refused.push_back({p, "a voice page without its file"});
    p = page("flex", "short_message", "1");
    p.m_shortType = 3;
    refused.push_back({p, "short message type 3"});
    p = page("flex", "short_message", "1");
    p.m_source = 8;
    refused.push_back({p, "short message source 8"});
    p = page("flex", "binary", "1");
    p.m_blocking = 17;
    refused.push_back({p, "blocking 17"});
    p = page("flex", "secure", "1");
    p.m_secureType = 3;
    refused.push_back({p, "secure type 3"});
    p = page("flex", "alphanumeric", "0");
    p.m_groupMembers = "1000";
    refused.push_back({p, "a group of one pager"});
    p = page("flex", "alphanumeric", "0");
    p.m_groupMembers = "1000,x";
    refused.push_back({p, "a group member that is not a number"});
    p = page("flex", "alphanumeric", "0");
    p.m_groupMembers = "1000,0";
    refused.push_back({p, "a group member out of range"});
    p = page("flex", "binary", "0");
    p.m_groupMembers = "1000,2000";
    refused.push_back({p, "a binary group message"});
    p = page("pocsag", "alphanumeric", "1");
    p.m_retransmit = -1;
    refused.push_back({p, "negative retransmissions"});
    p = page("pocsag", "alphanumeric", "1");
    p.m_sendDelay = 3601.0;
    refused.push_back({p, "a send delay over an hour"});
    p = page("pocsag", "alphanumeric", "1");
    p.m_retransmitInterval = 3601.0;
    refused.push_back({p, "an interval over an hour"});

    for (const Case& c : refused) {
        check(!c.m_page.check().isEmpty(), (std::string("refused: ") + c.m_what).c_str());
    }

    p = page("pocsag", "", "2097151");
    check(p.check().isEmpty() && (p.toRecord().m_msgType == "alphanumeric"), "no type is alphanumeric, the largest POCSAG address");
    p = page("pocsag", "tone", "1");
    check(p.toRecord().m_text.isNull(), "a tone page has no text");

    // GSC cap codes: 7 digits, the suffix sets the function; 5 digits, index 0
    p = page("gsc", "alphanumeric", "1234567");
    p.m_function = 3;
    check(p.check().isEmpty() && (p.toRecord().m_address == "123456/3") && (p.toRecord().m_protocolData.value("function").toInt() == 2),
        "GSC 7 digits: suffix 7 is function C, over the function of the page");
    p = page("gsc", "alphanumeric", "1234560");
    check(p.toRecord().m_address == "123456/2", "GSC suffix 0 is function B");
    p = page("gsc", "alphanumeric", "23456");
    p.m_function = 1;
    check(p.check().isEmpty() && (p.toRecord().m_address == "023456/2"), "GSC 5 digits: index 0, the function of the page");

    // FLEX numeric formats
    const char *formats[] = {"special_numeric", "numbered_numeric", "numbered_special_numeric"};

    for (const char *format : formats)
    {
        p = page("flex", format, "1000");
        p.m_text = "123";
        check(p.check().isEmpty() && (p.toRecord().m_msgType == format), (std::string("FLEX ") + format).c_str());
        p = page("pocsag", format, "1000");
        check(!p.check().isEmpty(), (std::string("POCSAG refuses ") + format).c_str());
    }

    // The codepage of the text: one of the protocol
    p = page("flex", "alphanumeric", "1000");
    p.m_charset = RadioPagingCharset::Key{"motorola", "linguist", "cyrillic"};
    check(p.check().isEmpty() && (RadioPagingPage::charsetKey(p.toRecord().m_protocolData.value("charset")) == p.m_charset)
        && (p.toRecord().m_protocolData.value("charset").toMap().value("model").toString() == "linguist"),
        "FLEX page with a FLEX codepage: vendor, model and codepage in the record");
    p.m_charset = RadioPagingCharset::Key{"generic", "generic", "latin-german"};
    check(!p.check().isEmpty(), "FLEX refuses a POCSAG codepage");
    p.m_charset = RadioPagingCharset::Key{"motorola", "linguist", "none"};
    check(!p.check().isEmpty(), "an unknown codepage is refused");
    p.m_charset = RadioPagingCharset::Key();
    check(p.check().isEmpty() && !p.toRecord().m_protocolData.contains("charset"), "no codepage: the one of the transmitter, no key");
}

// FLEX capcodes as written (STD-43A Appendix A): only the address is used
void testCapcodes()
{
    RadioPagingFlex::CapcodeText c;
    std::string error;

    check(RadioPagingFlex::parseCapcode("3E005237593", c, error) && (c.m_capcode == 5237593) && (c.m_letter == 'E')
        && (c.pagerCollapse() == 3) && (c.phaseType() == 1) && (c.subtract() == 0) && (c.m_frame == -1) && (c.m_digits == 9),
        "3E005237593: collapse 3, any phase, long address 5237593");
    check(RadioPagingFlex::parseCapcode("R0073U0123456", c, error) && (c.m_roaming == 'R') && (c.m_frame == 7)
        && (c.pagerCollapse() == 3) && (c.phase() == 0) && (c.m_capcode == 123456), "RfffbU: roaming R, frame 007, collapse 3, phase A");
    check(RadioPagingFlex::parseCapcode("c1234567", c, error) && (c.m_letter == 'C') && (c.subtract() == 2) && (c.pagerCollapse() == 4)
        && (c.phaseType() == 0), "C: single phase, subtract 2, collapse 4 without b (lower case accepted)");
    check(RadioPagingFlex::parseCapcode("1234567", c, error) && !c.m_letter && (c.pagerCollapse() == -1), "digits only: the address");
    check(!RadioPagingFlex::parseCapcode("0073A1234567", c, error), "fff with A-L is refused (the frame is in the address)");
    check(!RadioPagingFlex::parseCapcode("8A1234567", c, error), "collapse 8 is refused");
    check(!RadioPagingFlex::parseCapcode("1283U1234567", c, error), "frame 128 is refused");
    check(!RadioPagingFlex::parseCapcode("M1234567", c, error), "letter M is refused");
    check(!RadioPagingFlex::parseCapcode("R1234567", c, error), "a 2nd alpha character needs the letter");
    check(!RadioPagingFlex::parseCapcode("12345678901", c, error), "11 digits are refused");

    check(RadioPagingFlex::capcodeType(1020927) == "Short address", "Table A-1: short address");
    check(RadioPagingFlex::capcodeType(2029573) == "Temporary address 5", "Table A-1: temporary address 5");
    check(RadioPagingFlex::capcodeType(2029599) == "Operator messaging address: system event notification",
        "Table 3.8.2.4-1: system event notification");
    check(RadioPagingFlex::capcodeType(2025472) == "Network address", "Table A-1: network address");
    check(RadioPagingFlex::capcodeType(4290440001ULL) == "Information service, long address 2-3 (per country for world wide use), country code 440",
        "Table A-1 Note 3: country code 440");
    check(RadioPagingFlex::capcodeType(4297068543ULL) == "Reserved for future use (not defined)", "Table A-1: above the last address");

    std::string d = RadioPagingFlex::capcodeDescription("3E005237593");
    check((d.find("Long address 1-2") != std::string::npos) && (d.find("frame 053, phase C") != std::string::npos),
        "description: long address 1-2, sent in frame 053 phase C");
    d = RadioPagingFlex::capcodeDescription("2025472");
    check(d.find("not sent by the encoder") != std::string::npos, "description: a network address is not sent");
    d = RadioPagingFlex::capcodeDescription("B1020928");
    check((d.find("Standard rule: frame 063, phase D") != std::string::npos)
        && (d.find("not in the frame and phase of the standard rule") != std::string::npos),
        "description: B1020928 has the rule of 1020927 (frame 063, phase D), the encoder sends in frame 064");

    RadioPagingPage p = page("flex", "alphanumeric", "3E005237593");
    qulonglong capcode = 0;
    check(p.check().isEmpty() && p.flexCapcode(capcode) && (capcode == 5237593)
        && (p.toRecord().m_protocolData.value("capcode").toULongLong() == 5237593), "a FLEX page sends the address of the capcode form");
    p.m_address = "9E1234567";
    check(!p.check().isEmpty(), "a FLEX page refuses a capcode form that is not one");
    p = page("flex", "alphanumeric", "1");
    p.m_groupMembers = "3E005237593, A1020927";
    check(p.check().isEmpty() && (p.groupMembers() == QList<qulonglong>({5237593, 1020927})), "group members in the capcode forms");

    check(RadioPagingPocsag::addressDescription(1234567, 2).find("Frame 7 (address mod 8)") == 0, "POCSAG: frame of the 3 LSBs");
}

// Text with the status of each character: lost codes as '?', a lost fragment as <...>
void testCharStatus()
{
    QString status;
    QString text = RadioPagingRecode::decodeWithStatus("AB?C", ".cX.", nullptr, status);
    check((text == "AB?C") && (status == ".cX."), "char status: received, corrected, lost");
    text = RadioPagingRecode::decodeWithStatus(std::string("AB") + '\x1a' + "C", "..G.", nullptr, status);
    check((text == "AB<...>C") && (status == "..XXXXX."), "char status: a lost fragment of unknown length is <...>");
    text = RadioPagingRecode::decodeWithStatus(std::string("A") + '\x01' + "B", "...", nullptr, status);
    check((text == "A<0x01>B") && (status == "........"), "char status: a code shown as <0xNN> keeps one status");
    text = RadioPagingRecode::decodeWithStatus("ABC", "", nullptr, status);
    check((text == "ABC") && status.isEmpty(), "char status: none without the status of the codes");

    // POCSAG: the characters of an uncorrectable codeword are '?'. 7-bit characters, 20 bits a codeword
    RadioPagingPocsag::TxMessage m;
    m.m_address = 1000;
    m.m_type = RadioPagingPocsag::Alphanumeric;
    m.m_text = "HELLO WORLD";
    RadioPagingPocsag::EncodeResult r = RadioPagingPocsag::encode({ m });
    std::vector<uint8_t> raw;
    int bits = 0;

    for (size_t i = 0; i < r.m_codewords.size(); i++)
    {
        uint32_t w = r.m_codewords[i];

        if ((w & 0x80000000u) && (w != RadioPagingPocsag::m_syncWord) && (w != RadioPagingPocsag::m_idleWord))
        {
            for (int b = 30; b >= 11; b--)
            {
                if ((bits % 8) == 0) {
                    raw.push_back(0);
                }
                if ((w >> b) & 1) {
                    raw.back() |= (uint8_t) (0x80 >> (bits % 8));
                }
                bits++;
            }
        }
    }

    const RadioPagingCharset::Codepage *ascii = RadioPagingCharset::defaultFor("pocsag");
    text = RadioPagingRecode::pocsagText(raw, bits, "alphanumeric", ascii, RadioPagingPocsag::fillMinimal(), ".X.c", status);
    // Codeword 1 holds bits 20-39: characters 2 (bits 14-20) to 5 (bits 35-41)
    check(text.startsWith("HE????") && (status.left(6) == "..XXXX") && (status.size() == text.size()),
        "POCSAG: the characters of an uncorrectable codeword are '?'");
    text = RadioPagingRecode::pocsagText(raw, bits, "alphanumeric", ascii, RadioPagingPocsag::fillMinimal(), "....", status);
    check(text.startsWith("HELLO WORLD") && !status.contains('X'), "POCSAG: all codewords received");
}

} // namespace

int main()
{
    testRoundTrips();
    testLimits();
    testCapcodes();
    testCharStatus();
    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
