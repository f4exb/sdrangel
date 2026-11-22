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

#ifndef MODEMRADIOPAGING_RADIOPAGINGPAGE_H
#define MODEMRADIOPAGING_RADIOPAGINGPAGE_H

#include <QString>
#include <QStringList>
#include <QList>

#include "export.h"
#include "util/messagerecord.h"
#include "radiopagingcharset.h"

namespace SWGSDRangel {
    class SWGRadioPagingModTx;
    class SWGRadioPagingCharset;
}

// A page to transmit: the fields of the tx action of the modulator and of the post of the message center.
//
// The message center keeps a page as a TX record (toRecord): the address and the fields in the keys of the
// records of the receivers and transmitters (protocolData). A transmitter takes it back with fromRecord.
// check() holds the limits that do not need the channel; the encoder checks the rest when it is queued.
struct MODEMRADIOPAGING_API RadioPagingPage
{
    QString m_id;                       //!< Page of the message center, empty if none
    QString m_transmitter;              //!< Alias of the transmitter (message center)
    QString m_protocol;                 //!< pocsag, flex or gsc
    QString m_type;                     //!< One of types(); empty: alphanumeric
    QString m_address;                  //!< Digits: POCSAG address, FLEX capcode, GSC cap code (up to 6 digits, or 7 with the suffix)
    int m_function = 0;                 //!< POCSAG function 0-3 (A-D); GSC function 0-3, the address number 1-4
    bool m_priority = false;            //!< FLEX priority address
    //!< FLEX group message: the capcodes of the pagers (digits separated by commas or spaces, 2 to 5461); the address is
    //!< not used. The message goes to a temporary address that a short instruction tells the pagers (STD-43A 5.2)
    QString m_groupMembers;
    QString m_text;
    //!< Alphanumeric (FLEX secure type 0 too): the codepage of the text in the protocol of the page, none (no vendor)
    //!< for the one of the transmitter
    RadioPagingCharset::Key m_charset;
    QString m_voiceFile;                //!< GSC voice: the sound file
    int m_speed = 0;                    //!< POCSAG baud, 0: the channel setting
    int m_polarity = -1;                //!< -1 the channel setting, 0 normal, 1 inverted
    int m_shortType = 0;                //!< FLEX short message (STD-43A Table 3.9.2-1): 0 numeric, 1 source, 2 numbered
    int m_source = 0;                   //!< FLEX short message source code 0-7
    int m_blocking = 1;                 //!< FLEX binary: B, 1-16
    bool m_mailDrop = false;            //!< FLEX binary: M
    bool m_rightToLeft = false;         //!< FLEX binary: D
    bool m_headerMessage = false;       //!< FLEX binary: H
    bool m_statusInfo = false;          //!< FLEX binary: I
    int m_secureType = 0;               //!< FLEX secure t1 t0: 0 alphanumeric, 1 data defined separately, 2 binary
    int m_retransmit = 0;               //!< Network mode: copies sent after the message
    double m_retransmitInterval = -1.0; //!< Seconds between the copies, negative: the default of the protocol
    double m_sendDelay = 0.0;           //!< Network mode: seconds before the first transmission

    static const int m_maxScheduleSeconds = 3600;

    //!< The protocols and the types of messages; the types a protocol sends
    static const QStringList& protocols();
    static const QStringList& types();
    static QStringList types(const QString& protocol);

    //!< Empty if the page can be queued, else why not
    QString check() const;
    //!< Empty if m_protocol is one and m_address and m_function are an address of it, else why not (the address
    //!< part of check(), for whatever names an address: the composer, the contacts of the Message Center)
    QString checkAddress() const;
    //!< The names of the functions of a protocol, index 0-3: POCSAG A to D, GSC 1 (A) to 4 (D); none for FLEX
    static QStringList functions(const QString& protocol);
    //!< What an address is: POCSAG its frame and function, FLEX what the capcode form tells, GSC the digits of the
    //!< cap code; empty for another protocol
    static QString addressDescription(const QString& protocol, const QString& address, int function);
    //!< Clean, corrected or damaged, as the paging receiver judges a record it decoded: its errors, then the checks of
    //!< its protocol (a failed FLEX checksum K or signature S, a GSC block with a wrong checksum: damaged; K incomplete:
    //!< damaged; S incomplete: damaged unless it is a fragment or a part, whose rest is still to come). Checks passed
    //!< and no error count: clean. Bits corrected and the checks passed (or none to check): corrected
    static MessageRecord::Quality receivedQuality(const MessageRecord& record);
    //!< The fields of protocolData of the records of the paging receiver, described for the conditions of filters
    //!< (MessageEndpoint m_fields): key, name, protocol, type (list, number, text) and the values of a list
    static QVariantList receivedFields();
    //!< A check of protocolData ("checksum" K of the words of a fragment, "signature" S of the whole message) damages it
    static bool checkDamages(const MessageRecord& record, const char *check);
    //!< The quality of a record as its producer set it, else as receivedQuality judges it; sent: clean
    static MessageRecord::Quality quality(const MessageRecord& record);
    //!< Why: e.g. "checksum K failed", "1 codeword uncorrectable", "3 bit errors corrected"; empty when clean
    static QString qualityReason(const MessageRecord& record);
    //!< The capcodes of m_groupMembers (digits or the capcode forms of STD-43A Appendix A: their addresses)
    QList<qulonglong> groupMembers(bool *ok = nullptr) const;
    //!< FLEX: the address of the capcode in m_address, digits or a capcode form of STD-43A Appendix A (only the
    //!< address is sent). False and why if it is not one
    bool flexCapcode(qulonglong& capcode, QString *error = nullptr) const;
    //!< GSC: the 6 digit address of the cap code in m_address and the function 0-3, of its suffix digit for 7
    //!< digits, else m_function. False if the cap code is not one
    bool gscAddress(QString& address, int& function) const;
    //!< The page as a TX record of the message center
    MessageRecord toRecord() const;
    //!< The page of a TX record (toRecord, or a record of a source file in the same keys)
    static RadioPagingPage fromRecord(const MessageRecord& record);
    //!< The tx action of the modulator (WebAPI) of the page, and the page of a tx action (without a protocol: the
    //!< protocol of the channel)
    void toTx(SWGSDRangel::SWGRadioPagingModTx& tx) const;
    static RadioPagingPage fromTx(SWGSDRangel::SWGRadioPagingModTx& tx, const QString& defaultProtocol);

    //!< A codepage key in the records (protocolData charset): vendor, model and codepage; the protocol is the one of the
    //!< record
    static QVariantMap charsetData(const RadioPagingCharset::Key& key);
    static RadioPagingCharset::Key charsetKey(const QVariant& data);
    //!< A codepage key in the WebAPI, nullptr for none; the key of a WebAPI object (none for nullptr)
    static SWGSDRangel::SWGRadioPagingCharset *charsetSwg(const RadioPagingCharset::Key& key);
    static RadioPagingCharset::Key charsetKey(SWGSDRangel::SWGRadioPagingCharset *swg);
    //!< Empty if the key is a codepage of the protocol, else why not
    static QString checkCharset(const QString& protocol, const RadioPagingCharset::Key& key);
    //!< The key as the user reads it: vendor / model / codepage
    static QString charsetText(const RadioPagingCharset::Key& key);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGPAGE_H
