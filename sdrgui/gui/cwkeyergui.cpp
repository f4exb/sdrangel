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
    m_doApplySettings(true)
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
    ui->cwTextEdit->setText("");
    ui->cwSpeed->setValue(13);
}

QByteArray CWKeyerGUI::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, ui->cwTextEdit->text());
    s.writeS32(2, ui->cwSpeed->value());

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

        applySettings();
        return true;
    }
    else
    {
        return false;
    }
}

// === SLOTS ==================================================================

void CWKeyerGUI::on_cwTextClear_clicked(bool checked __attribute__((unused)))
{
    ui->cwTextEdit->clear();
    m_cwKeyer->setText("");
}

void CWKeyerGUI::on_cwTextEdit_editingFinished()
{
    if (m_doApplySettings) { m_cwKeyer->setText(ui->cwTextEdit->text()); }
}

void CWKeyerGUI::on_cwSpeed_valueChanged(int value)
{
    ui->cwSpeedText->setText(QString("%1").arg(value));
    if (m_doApplySettings) { m_cwKeyer->setWPM(value); }
}

void CWKeyerGUI::on_playDots_toggled(bool checked)
{
    //ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);

    if (m_doApplySettings) { m_cwKeyer->setMode(checked ? CWKeyerSettings::CWDots : CWKeyerSettings::CWNone); }
}

void CWKeyerGUI::on_playDashes_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    //ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);

    if (m_doApplySettings) { m_cwKeyer->setMode(checked ? CWKeyerSettings::CWDashes : CWKeyerSettings::CWNone); }
}

void CWKeyerGUI::on_playText_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    //ui->playText->setEnabled(!checked);

    if (m_doApplySettings) { m_cwKeyer->setMode(checked ? CWKeyerSettings::CWText : CWKeyerSettings::CWNone); }

    if (checked) {
        ui->playStop->setChecked(true);
    } else {
        ui->playStop->setChecked(false);
    }
}

void CWKeyerGUI::on_playLoopCW_toggled(bool checked)
{
    if (m_doApplySettings) { m_cwKeyer->setLoop(checked); }
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
}

void CWKeyerGUI::displaySettings(const CWKeyerSettings& settings)
{
    blockApplySettings(true);

    ui->playLoopCW->setChecked(settings.m_loop);

    ui->playDots->setEnabled((settings.m_mode == CWKeyerSettings::CWDots) || (settings.m_mode == CWKeyerSettings::CWNone));
    ui->playDots->setChecked(settings.m_mode == CWKeyerSettings::CWDots);

    ui->playDashes->setEnabled((settings.m_mode == CWKeyerSettings::CWDashes) || (settings.m_mode == CWKeyerSettings::CWNone));
    ui->playDashes->setChecked(settings.m_mode == CWKeyerSettings::CWDashes);

    ui->playText->setEnabled((settings.m_mode == CWKeyerSettings::CWText) || (settings.m_mode == CWKeyerSettings::CWNone));
    ui->playText->setChecked(settings.m_mode == CWKeyerSettings::CWText);

    ui->cwTextEdit->setText(settings.m_text);
    ui->cwSpeed->setValue(settings.m_wpm);
    ui->cwSpeedText->setText(QString("%1").arg(settings.m_wpm));

    blockApplySettings(false);
}

void CWKeyerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}
