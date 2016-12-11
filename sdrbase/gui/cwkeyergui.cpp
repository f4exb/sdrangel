///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QKeyEvent>
#include <QDebug>

#include "gui/cwkeyergui.h"
#include "ui_cwkeyergui.h"
#include "dsp/cwkeyer.h"
#include "util/simpleserializer.h"

CWKeyerGUI::CWKeyerGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::CWKeyerGUI),
    m_messageQueue(0),
    m_cwKeyer(0),
    m_key(0x30),
    m_keyDot(0x31),
    m_keyDash(0x32)
{
    ui->setupUi(this);
}

CWKeyerGUI::~CWKeyerGUI()
{
    this->releaseKeyboard(); // just in case
    delete ui;
}

void CWKeyerGUI::setBuddies(MessageQueue* messageQueue, CWKeyer* cwKeyer)
{
    m_messageQueue = messageQueue;
    m_cwKeyer = cwKeyer;
    applySettings();
}

void CWKeyerGUI::resetToDefaults()
{
    m_key = false;
    m_keyDot = false;
    m_keyDash = false;
}

QByteArray CWKeyerGUI::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, ui->cwTextEdit->text());
    s.writeS32(2, ui->cwSpeed->value());
    s.writeS32(3, ui->morseKeyAssign->currentIndex());
    s.writeS32(4, ui->iambicKeyDotAssign->currentIndex());
    s.writeS32(5, ui->iambicKeyDashAssign->currentIndex());

    return s.final();
}

bool CWKeyerGUI::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QString aString;
        int aValue;

        d.readString(1, &aString, "");
        ui->cwTextEdit->setText(aString);
        d.readS32(2, &aValue, 13);
        ui->cwSpeed->setValue(aValue);
        d.readS32(3, &aValue, 0);
        ui->morseKeyAssign->setCurrentIndex(aValue);
        d.readS32(4, &aValue, 1);
        ui->iambicKeyDotAssign->setCurrentIndex(aValue);
        d.readS32(5, &aValue, 2);
        ui->iambicKeyDashAssign->setCurrentIndex(aValue);

        applySettings();
        return true;
    }
}

// === SLOTS ==================================================================

void CWKeyerGUI::on_cwTextClear_clicked(bool checked)
{
    ui->cwTextEdit->clear();
    m_cwKeyer->setText("");
}

void CWKeyerGUI::on_cwTextEdit_editingFinished()
{
    m_cwKeyer->setText(ui->cwTextEdit->text());
}

void CWKeyerGUI::on_cwSpeed_valueChanged(int value)
{
    ui->cwSpeedText->setText(QString("%1").arg(value));
    m_cwKeyer->setWPM(value);
}

void CWKeyerGUI::on_morseKey_toggled(bool checked)
{
    //ui->morseKey->setEnabled(!checked);
    ui->iambicKey->setEnabled(!checked);
    ui->playStop->setEnabled(!checked);
    m_cwKeyer->setMode(checked ? CWKeyer::CWKey : CWKeyer::CWNone);
}

void CWKeyerGUI::on_morseKeyAssign_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < 9)) {
        m_key = 0x30 + index;
    }
}

void CWKeyerGUI::on_iambicKey_toggled(bool checked)
{
    ui->morseKey->setEnabled(!checked);
    //ui->iambicKey->setEnabled(!checked);
    ui->playStop->setEnabled(!checked);
    m_cwKeyer->setMode(checked ? CWKeyer::CWIambic : CWKeyer::CWNone);
}

void CWKeyerGUI::on_iambicKeyDotAssign_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < 9)) {
        m_keyDot = 0x30 + index;
    }
}

void CWKeyerGUI::on_iambicKeyDashAssign_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < 9)) {
        m_keyDash = 0x30 + index;
    }
}

void CWKeyerGUI::on_playText_toggled(bool checked)
{
    ui->morseKey->setEnabled(!checked);
    ui->iambicKey->setEnabled(!checked);
    //ui->playStop->setEnabled(!checked);
    m_cwKeyer->setMode(checked ? CWKeyer::CWText : CWKeyer::CWNone);
}

void CWKeyerGUI::on_playLoop_toggled(bool checked)
{
    m_cwKeyer->setLoop(checked);
}

void CWKeyerGUI::on_playStop_toggled(bool checked)
{
    if (checked)
    {
        m_cwKeyer->resetText();
    }
    else
    {
        m_cwKeyer->stopText();
    }
}

// === End SLOTS ==============================================================

void CWKeyerGUI::applySettings()
{
    int value;

    m_cwKeyer->setText(ui->cwTextEdit->text());

    value = ui->cwSpeed->value();
    ui->cwSpeedText->setText(QString("%1").arg(value));
    m_cwKeyer->setWPM(value);

    value = ui->morseKeyAssign->currentIndex();
    if ((value >= 0) && (value < 9)) {
        m_key = 0x30 + value;
    }

    value = ui->iambicKeyDotAssign->currentIndex();
    if ((value >= 0) && (value < 9)) {
        m_keyDot = 0x30 + value;
    }

    value = ui->iambicKeyDashAssign->currentIndex();
    if ((value >= 0) && (value < 9)) {
        m_keyDash = 0x30 + value;
    }
}

void CWKeyerGUI::keyPressEvent(QKeyEvent* keyEvent)
{
    int key = keyEvent->key();

    // Escape halts CW engine and releases keyboard immediately
    // Thus keyboard cannot be left stuck
    if (key == Qt::Key_Escape)
    {
        m_cwKeyer->setMode(CWKeyer::CWNone);
        ui->morseKey->setChecked(false);
        ui->iambicKey->setChecked(false);
        ui->playStop->setChecked(false);
        ui->morseKey->setEnabled(true);
        ui->iambicKey->setEnabled(true);
        ui->playStop->setEnabled(true);
        this->releaseKeyboard();
        return;
    }

    if (!keyEvent->isAutoRepeat())
    {
        qDebug() << "CWKeyerGUI::keyPressEvent: key"
                << ": " << key;
        if (key == m_key)
        {
            m_cwKeyer->setKey(true);
        }
        else if (key == m_keyDot)
        {
            m_cwKeyer->setDot(true);
        }
        else if (key == m_keyDash)
        {
            m_cwKeyer->setDash(true);
        }
    }
}

void CWKeyerGUI::keyReleaseEvent(QKeyEvent* keyEvent)
{
    if (!keyEvent->isAutoRepeat())
    {
        qDebug() << "CWKeyerGUI::keyReleaseEvent: key"
                << ": " << keyEvent->key();
        if (keyEvent->key() == m_key)
        {
            m_cwKeyer->setKey(false);
        }
        else if (keyEvent->key() == m_keyDot)
        {
            m_cwKeyer->setDot(false);
        }
        else if (keyEvent->key() == m_keyDash)
        {
            m_cwKeyer->setDash(false);
        }
    }
}

