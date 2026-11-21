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

// Self test of the composer of paging messages: the page it gives for each protocol and type, the fields it
// shows, the copies. Run with QT_QPA_PLATFORM=offscreen. Prints one line per check and returns non-zero if
// any check fails.

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolButton>
#include <cstdio>

#include "gui/pagingcodepagetable.h"

#include "gui/pagingcomposer.h"

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

// A combo of the composer by one of its items
QComboBox *comboWith(QWidget& composer, const QString& item)
{
    for (QComboBox *combo : composer.findChildren<QComboBox*>())
    {
        for (int i = 0; i < combo->count(); i++)
        {
            if (combo->itemText(i) == item) {
                return combo;
            }
        }
    }

    return nullptr;
}

void select(QWidget& composer, const QString& item)
{
    QComboBox *combo = comboWith(composer, item);

    if (combo) {
        combo->setCurrentIndex(combo->findText(item));
    }
}

QLineEdit *addressEdit(QWidget& composer)
{
    return composer.findChildren<QLineEdit*>().value(0);
}

QLineEdit *groupEdit(QWidget& composer)
{
    return composer.findChildren<QLineEdit*>().value(1);
}

QLineEdit *textEdit(QWidget& composer)
{
    return composer.findChildren<QLineEdit*>().value(2);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PagingComposer composer;
    composer.show();

    // POCSAG: address, function, speed
    composer.setProtocols({"pocsag", "flex", "gsc"});
    check(composer.protocol() == "pocsag", "the first protocol is the one chosen");
    addressEdit(composer)->setText("1234567");
    textEdit(composer)->setText("HELLO");
    select(composer, "C");
    select(composer, "1200");
    RadioPagingPage p = composer.page();
    check((p.m_protocol == "pocsag") && (p.m_type == "alphanumeric") && (p.m_address == "1234567") && (p.m_function == 2)
        && (p.m_speed == 1200) && (p.m_text == "HELLO") && p.check().isEmpty(), "POCSAG page: address, function C, speed, text");
    check(composer.summary().startsWith("1234567 C"), "the summary has the address and the function letter");

    // Tone: no text
    select(composer, "Tone");
    check(composer.page().m_text.isEmpty() && !textEdit(composer)->isEnabled(), "a tone page has no text, the field is off");

    // A destination filled in (a contact): transmitter (one that did not announce itself too), protocol, address,
    // function and polarity; not an edit of the user
    {
        PagingComposer filled;
        filled.setTransmitters({"tx1"});
        int edits = 0;
        QObject::connect(&filled, &PagingComposer::destinationEdited, [&edits]() { edits++; });
        filled.setDestination("tx-gsc", "gsc", "012345", 2, 1);
        RadioPagingPage d = filled.page();
        check((filled.transmitter() == "tx-gsc") && (d.m_protocol == "gsc") && (d.m_address == "012345") && (d.m_function == 2)
            && (d.m_polarity == 1) && (edits == 0), "a destination filled in: transmitter, protocol, address, function, polarity");
    }

    // FLEX: priority, short message, binary, secure
    composer.setProtocols({"pocsag", "flex", "gsc"});
    select(composer, "FLEX");
    check(composer.protocol() == "flex", "the protocol is chosen from the selector");
    addressEdit(composer)->setText("2101249");
    select(composer, "Alphanumeric");
    textEdit(composer)->setText("FLEX TEXT");
    check((composer.page().m_address == "2101249") && composer.page().check().isEmpty(), "FLEX page: a long capcode");

    select(composer, "Short message");
    select(composer, "Numbered");
    p = composer.page();
    check((p.m_type == "short_message") && (p.m_shortType == 2) && p.m_text.isEmpty(), "FLEX numbered short message: no text, the source");

    select(composer, "Binary");
    textEdit(composer)->setText("A5A5");
    p = composer.page();
    check((p.m_type == "binary") && (p.m_blocking == 1) && (p.m_text == "A5A5"), "FLEX binary page");

    select(composer, "Secure");
    comboWith(composer, "Separate")->setCurrentIndex(2);
    p = composer.page();
    check((p.m_type == "secure") && (p.m_secureType == 2) && p.check().isEmpty(), "FLEX secure page: binary");

    // A group message: the pagers replace the address, no copies
    select(composer, "Alphanumeric");
    composer.setNetworkMode(true);
    composer.setFlexNumbering(true);
    QLineEdit *group = groupEdit(composer);
    group->setText("1000, 2000");
    p = composer.page();
    check((p.m_groupMembers == "1000, 2000") && (p.groupMembers().size() == 2) && p.check().isEmpty(), "FLEX group page: the pagers, no address needed");
    group->setText(QString());
    composer.setNetworkMode(false);

    // GSC: the address number, the voice file
    select(composer, "GSC");
    addressEdit(composer)->setText("1234");
    select(composer, "3 (C)");
    select(composer, "Numeric");
    p = composer.page();
    check((p.m_protocol == "gsc") && (p.m_function == 2) && (p.m_type == "numeric") && p.check().isEmpty(),
        "GSC page: the address number 3 is the function 2");
    addressEdit(composer)->setText("1234560");
    p = composer.page();
    QComboBox *functionBox = nullptr;

    for (QComboBox *box : composer.findChildren<QComboBox*>())
    {
        if (box->findText("3 (C)") >= 0) {
            functionBox = box;
        }
    }

    QString capCodeText;

    for (QLabel *label : composer.findChildren<QLabel*>())
    {
        if (label->text().startsWith("index ")) {
            capCodeText = label->text();
        }
    }

    check(functionBox && !functionBox->isEnabled() && (functionBox->currentIndex() == 1) && (p.m_function == 1)
        && (composer.summary().startsWith("123456/2 (B)")), "GSC 7 digits: suffix 0 is function B, the selector follows it");
    check(capCodeText.startsWith("index 1, group 23, address 456, suffix 0 = 2 (B) -> preamble 4"), "GSC cap code explained");
    addressEdit(composer)->setText("1234");
    check(functionBox->isEnabled(), "GSC without a suffix: the function is selected");
    select(composer, "3 (C)");
    select(composer, "Voice");
    textEdit(composer)->setText("/a/b.wav");
    p = composer.page();
    check((p.m_type == "voice") && (p.m_voiceFile == "/a/b.wav") && p.m_text.isEmpty() && !composer.microphoneSelected(),
        "GSC voice page: the file, no text");

    // The microphone is for the user that allows it
    composer.setMicrophone(true);
    select(composer, "Microphone");
    check(composer.microphoneSelected() && composer.page().m_voiceFile.isEmpty(), "the microphone replaces the file");
    composer.setMicrophone(false);
    check(!composer.microphoneSelected(), "no microphone when it is not allowed");

    // Copies: network mode, POCSAG and FLEX numbered types
    composer.setProtocols({"pocsag", "flex"});
    composer.setNetworkMode(true);
    composer.setFlexNumbering(true);
    select(composer, "POCSAG");
    select(composer, "Alphanumeric");
    QSpinBox *retransmit = nullptr;

    for (QSpinBox *spin : composer.findChildren<QSpinBox*>())
    {
        if ((spin->maximum() >= 10) && spin->prefix().isEmpty()) {
            retransmit = spin;
        }
    }

    check(retransmit && retransmit->isVisibleTo(&composer) && (retransmit->maximum() == 10), "POCSAG network: up to 10 copies");
    retransmit->setValue(3);
    check(composer.page().m_retransmit == 3, "the page has the copies");
    select(composer, "FLEX");
    select(composer, "Numeric");
    check(!retransmit->isEnabled() && (composer.page().m_retransmit == 0), "FLEX numeric: no copies");
    select(composer, "Numbered numeric");
    check(retransmit->isEnabled() && (composer.page().m_type == "numbered_numeric"), "FLEX numbered numeric: copies");
    select(composer, "Special numeric");
    check(!retransmit->isEnabled() && (composer.page().m_type == "special_numeric"), "FLEX special numeric: no copies");
    select(composer, "Alphanumeric");
    check(retransmit->isEnabled() && (retransmit->maximum() == 15), "FLEX alphanumeric: up to 15 copies");
    composer.setFlexNumbering(false);
    check(!retransmit->isEnabled(), "FLEX without numbering: no copies");
    composer.setNetworkMode(false);
    check(!retransmit->isVisibleTo(&composer) && (composer.page().m_retransmit == 0), "one-shot: no copies");

    // Transmitters (message center)
    composer.setTransmitters({"t1", "t2"});
    int selections = 0;
    QObject::connect(&composer, &PagingComposer::selectionChanged, [&selections]() { selections++; });
    composer.setTransmitters({"t1", "t2"});
    check((composer.transmitter() == "t1") && (selections == 0), "the same transmitters change nothing");
    composer.setTransmitters({"t2", "t3"});
    check((composer.transmitter() == "t2") && (selections == 1), "the transmitter chosen is kept when it is still there");

    // The protocols the transmitter sends
    composer.setProtocols({"pocsag"});
    check((composer.protocol() == "pocsag") && !comboWith(composer, "FLEX"), "a single protocol: no selector");

    // Codepage: the combo of the protocol, the page takes it; the table inserts a character in the text
    select(composer, "Alphanumeric");
    select(composer, "  Generic - German");
    RadioPagingPage german = composer.page();
    check((german.m_charset == RadioPagingCharset::Key{"generic", "generic", "latin-german"}) && german.check().isEmpty(),
        "the page has the codepage chosen");
    PagingCodepageTable *table = composer.findChild<PagingCodepageTable*>();
    QToolButton *tableButton = nullptr;

    for (QToolButton *button : composer.findChildren<QToolButton*>())
    {
        if (button->text() == "Table") {
            tableButton = button;
        }
    }

    check(table && tableButton && tableButton->isVisibleTo(&composer), "alphanumeric: the codepage table button");
    textEdit(composer)->setText("Gr");
    textEdit(composer)->setCursorPosition(2);
    emit table->codeSelected(PagingCodepageTable::codeText(*RadioPagingCharset::find("pocsag", german.m_charset), 0x7d));
    emit table->codeSelected(PagingCodepageTable::codeText(*RadioPagingCharset::find("pocsag", german.m_charset), 0x03));
    check(textEdit(composer)->text() == QString::fromUtf8("Gr\xC3\xBC<0x03>"), "the table inserts the glyph, <0xNN> for a code without one");
    composer.setDefaultCodepage("pocsag", RadioPagingCharset::Key{"generic", "generic", "latin-german"});
    QComboBox *charsetCombo = comboWith(composer, "  Generic - German");
    charsetCombo->setCurrentIndex(0);
    check(composer.page().m_charset.empty(), "Default: no codepage in the page, the one of the channel");

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
