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

#ifndef SDRGUI_GUI_PAGINGCOMPOSER_H_
#define SDRGUI_GUI_PAGINGCOMPOSER_H_

#include <QString>
#include <QStringList>
#include <QWidget>

#include <map>

#include "export.h"
#include "radiopagingpage.h"

class PagingCodepageTable;

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
class QValidator;

// The fields of a message to page (POCSAG, FLEX or GSC), as the paging modulator and the message center
// send it. The user of the composer says what can be composed (setProtocols, setNetworkMode,
// setFlexNumbering, setMicrophone, setTransmitters); the composer shows the fields of the protocol and the
// type chosen and gives the RadioPagingPage composed, which the user checks and sends.
class SDRGUI_API PagingComposer : public QWidget
{
    Q_OBJECT
public:
    explicit PagingComposer(QWidget *parent = nullptr);

    //!< The protocols that can be composed (pocsag, flex, gsc): the selector is there when there are several.
    //!< The protocol and the type chosen are kept if they are still there
    void setProtocols(const QStringList& protocols);
    QString protocol() const;
    //!< The transmitters that can send the page (message center): the selector is there when this is called
    void setTransmitters(const QStringList& aliases);
    QString transmitter() const;
    //!< The destination of the page filled in (a contact): transmitter (shown even if it has not announced itself),
    //!< protocol, address, function and polarity (-1 the channel's, 0 normal, 1 inverted). The user can change them:
    //!< destinationEdited
    void setDestination(const QString& transmitter, const QString& protocol, const QString& address, int function, int polarity);
    //!< Copies and send delay are for the network mode (POCSAG and FLEX)
    void setNetworkMode(bool network);
    //!< What the address is (its frame, function, capcode form) on a line below it, or (false) in its tool tip only:
    //!< the rows of the form do not move while the address is typed
    void setAddressHintShown(bool shown);
    //!< The text alone on its row, the codepage and the send button on a last row: a narrow form keeps the text wide
    void setSendOnOwnRow();
    //!< FLEX copies are for the numbered types of a network with message numbering (STD-43A 4.3.7)
    void setFlexNumbering(bool numbering);
    //!< A GSC voice page can come from the microphone, which the user takes
    void setMicrophone(bool allowed);
    bool microphoneSelected() const;
    //!< The text of the button, empty for its own (Send, Send voice)
    void setSendText(const QString& text);
    void setSendEnabled(bool enabled);
    //!< The page composed: no id
    RadioPagingPage page() const;
    //!< The page in a line of text, for the log of the user
    QString summary() const;
    //!< The codepages of a protocol in a combo box, grouped by vendor (a header for each that cannot be chosen): the label
    //!< shown, the vendor, model and codepage as the data. With defaultEntry, a first entry Default of no codepage (the
    //!< one set for the channel)
    static void fillCodepages(QComboBox *combo, const QString& protocol, bool defaultEntry);
    //!< Selects the codepage of the key in a combo of fillCodepages, else the first entry that can be chosen
    static void selectCodepage(QComboBox *combo, const RadioPagingCharset::Key& key);
    //!< The codepage chosen in a combo of fillCodepages, none for Default
    static RadioPagingCharset::Key codepageKey(const QComboBox *combo);
    //!< The codepage the Default entry stands for in a protocol (the one of the channel); the default of the
    //!< protocol if not set
    void setDefaultCodepage(const QString& protocol, const RadioPagingCharset::Key& key);

signals:
    void sendRequested();
    void microphoneInputRequested();
    //!< The protocol, the type, the transmitter or the voice source changed
    void selectionChanged();
    //!< The user changed the transmitter, the address, the function or the polarity (not setDestination)
    void destinationEdited();

private:
    QComboBox *m_transmitter;
    QLabel *m_transmitterLabel;
    QComboBox *m_protocol;
    QLabel *m_protocolLabel;
    QLabel *m_addressLabel;
    QLineEdit *m_address;
    QLabel *m_capCode;                  //!< What the address is: GSC cap code, FLEX capcode, POCSAG frame and function
    QValidator *m_digitsValidator;      //!< POCSAG and GSC: digits
    QValidator *m_flexValidator;        //!< FLEX: digits or the capcode forms with letters (STD-43A Appendix A)
    QLabel *m_groupLabel;
    QLineEdit *m_group;
    QComboBox *m_function;
    QCheckBox *m_priority;
    QComboBox *m_type;
    QLabel *m_textLabel;
    QLineEdit *m_text;
    QString m_keptText;                 //!< The text of a type that has one, while a type without text is chosen
    QComboBox *m_charset;               //!< Alphanumeric: the codepage of the text
    QToolButton *m_codepageTable;       //!< The table of the codepage, to insert a character
    PagingCodepageTable *m_table;
    std::map<QString, RadioPagingCharset::Key> m_defaultCodepages;  //!< The codepage of Default, by protocol
    QToolButton *m_browse;
    QComboBox *m_voiceSource;
    QToolButton *m_micInput;
    QPushButton *m_send;
    QLabel *m_overrideLabel;
    QLabel *m_polarityLabel;
    QLabel *m_charsetLabel;
    QLabel *m_intervalLabel;
    QLabel *m_delayLabel;
    QComboBox *m_speed;
    QComboBox *m_polarity;
    QLabel *m_shortLabel;
    QComboBox *m_shortType;
    QSpinBox *m_source;
    QLabel *m_binaryLabel;
    QSpinBox *m_blocking;
    QCheckBox *m_mailDrop;
    QCheckBox *m_rightToLeft;
    QCheckBox *m_headerMessage;
    QCheckBox *m_statusInfo;
    QComboBox *m_secureType;
    QLabel *m_scheduleLabel;
    QSpinBox *m_retransmit;
    QDoubleSpinBox *m_retransmitInterval;
    QDoubleSpinBox *m_sendDelay;

    bool m_network = false;
    bool m_addressHintShown = true;
    QGridLayout *m_grid = nullptr;
    QHBoxLayout *m_textRow = nullptr;
    QHBoxLayout *m_sendRow = nullptr;
    int m_textGridRow = 0;
    bool m_flexNumbering = true;
    bool m_microphone = false;
    QString m_sendText;

    QString typeName() const;
    void fillTypes();
    void fillFunctions();
    void display();
    void browse();
    //!< The codepage of the text: the one chosen, else the one of Default
    const RadioPagingCharset::Codepage *textCodepage() const;
};

#endif // SDRGUI_GUI_PAGINGCOMPOSER_H_
