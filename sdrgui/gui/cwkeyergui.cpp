///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QKeyEvent>
#include <QDebug>

#include "gui/cwkeyergui.h"
#include "gui/dialpopup.h"
#include "ui_cwkeyergui.h"
#include "dsp/cwkeyer.h"
#include "util/simpleserializer.h"
#include "util/messagequeue.h"
#include "commands/commandkeyreceiver.h"
#include "mainwindow.h"

CWKeyerGUI::CWKeyerGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::CWKeyerGUI),
    m_cwKeyer(nullptr),
    m_doApplySettings(true),
    m_keyScope(NoKeyScope)
{
    ui->setupUi(this);
    m_commandKeyReceiver = new CommandKeyReceiver();
    m_commandKeyReceiver->setRelease(true);
    this->installEventFilter(m_commandKeyReceiver);
    DialPopup::addPopupsToChildDials(this);
}

CWKeyerGUI::~CWKeyerGUI()
{
    this->releaseKeyboard(); // just in case
    m_commandKeyReceiver->deleteLater();
    delete ui;
}

void CWKeyerGUI::setCWKeyer(CWKeyer* cwKeyer)
{
    m_cwKeyer = cwKeyer;
    setSettings(cwKeyer->getSettings());
	displaySettings();
}

void CWKeyerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray CWKeyerGUI::serialize() const
{
    return m_settings.serialize();
}

bool CWKeyerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void CWKeyerGUI::formatTo(SWGSDRangel::SWGObject *swgObject) const
{
    m_settings.formatTo(swgObject);
}

void CWKeyerGUI::updateFrom(const QStringList& keys, const SWGSDRangel::SWGObject *swgObject)
{
    m_settings.updateFrom(keys, swgObject);
}

// === SLOTS ==================================================================

void CWKeyerGUI::on_cwTextClear_clicked(bool checked)
{
    (void) checked;
    ui->cwTextEdit->clear();
    m_settings.m_text = "";
    applySettings();
}

void CWKeyerGUI::on_cwTextEdit_editingFinished()
{
    m_settings.m_text = ui->cwTextEdit->text();
    applySettings();
}

void CWKeyerGUI::on_cwSpeed_valueChanged(int value)
{
    ui->cwSpeedText->setText(QString("%1").arg(value));
    m_settings.m_wpm = value;
    applySettings();
}

void CWKeyerGUI::on_playDots_toggled(bool checked)
{
    //ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);
    m_settings.m_mode = checked ? CWKeyerSettings::CWDots : CWKeyerSettings::CWNone;
    applySettings();
}

void CWKeyerGUI::on_playDashes_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    //ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);
    m_settings.m_mode = checked ? CWKeyerSettings::CWDashes : CWKeyerSettings::CWNone;
    applySettings();
}

void CWKeyerGUI::on_playText_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    //ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);

    if (checked) {
        ui->playStop->setChecked(true);
    } else {
        ui->playStop->setChecked(false);
    }

    m_settings.m_mode = checked ? CWKeyerSettings::CWText : CWKeyerSettings::CWNone;
    applySettings();
}

void CWKeyerGUI::on_playLoopCW_toggled(bool checked)
{
    m_settings.m_loop = checked;
    applySettings();
}

void CWKeyerGUI::on_playStop_toggled(bool checked)
{
    if (checked) {
        m_cwKeyer->resetText();
    } else {
        m_cwKeyer->stopText();
    }
}

void CWKeyerGUI::on_keyingStyle_toggled(bool checked)
{
    m_settings.m_keyboardIambic = !checked;
    applySettings();
}

void CWKeyerGUI::on_keyDotCapture_toggled(bool checked)
{
    if (checked && ui->keyDashCapture->isChecked())
    {
        ui->keyDotCapture->setChecked(false);
        ui->keyDashCapture->setChecked(false);
        return;
    }

    if (checked)
    {
        m_keyScope = DotKeyScope;
        setFocus();
        setFocusPolicy(Qt::StrongFocus);
        connect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                        this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        m_keyScope = NoKeyScope;
        disconnect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                        this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
        setFocusPolicy(Qt::NoFocus);
        clearFocus();
    }
}

void CWKeyerGUI::on_keyDashCapture_toggled(bool checked)
{
    if (checked && ui->keyDotCapture->isChecked())
    {
        ui->keyDotCapture->setChecked(false);
        ui->keyDashCapture->setChecked(false);
        return;
    }

    if (checked)
    {
        m_keyScope = DashKeyScope;
        m_commandKeyReceiver->setRelease(false);
        setFocus();
        setFocusPolicy(Qt::StrongFocus);
        connect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                        this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        m_keyScope = NoKeyScope;
        m_commandKeyReceiver->setRelease(false);
        disconnect(m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
                        this, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
        setFocusPolicy(Qt::NoFocus);
        clearFocus();
    }
}

void CWKeyerGUI::commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release)
{
    (void) release;
    // qDebug("CWKeyerGUI::commandKeyPressed: key scope: %d", (int) m_keyScope);
    // qDebug("CWKeyerGUI::commandKeyPressed: key: %x", key);
    // qDebug("CWKeyerGUI::commandKeyPressed: has modifiers: %x", QFlags<Qt::KeyboardModifier>::Int(keyModifiers));

    if (m_keyScope == DotKeyScope)
    {
        setKeyLabel(ui->keyDotLabel, key, keyModifiers);
        ui->keyDotCapture->setChecked(false);
        m_settings.m_dotKey = key;
        m_settings.m_dotKeyModifiers = keyModifiers;
        applySettings();
    }
    else if (m_keyScope == DashKeyScope)
    {
        setKeyLabel(ui->keyDashLabel, key, keyModifiers);
        ui->keyDashCapture->setChecked(false);
        m_settings.m_dashKey = key;
        m_settings.m_dashKeyModifiers = keyModifiers;
        applySettings();
    }

    m_commandKeyReceiver->setRelease(true);
}

void CWKeyerGUI::on_keyboardKeyer_toggled(bool checked)
{
    qDebug("CWKeyerGUI::on_keyboardKeyer_toggled: %s", checked ? "true" : "false");
    ui->playDots->setEnabled(!checked);   // block or release other source inputs
    ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);
    m_settings.m_mode = checked ? CWKeyerSettings::CWKeyboard : CWKeyerSettings::CWNone;
    applySettings();

    if (checked) {
        MainWindow::getInstance()->commandKeysConnect(this, SLOT(keyboardKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    } else {
        MainWindow::getInstance()->commandKeysDisconnect(this, SLOT(keyboardKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
}

void CWKeyerGUI::keyboardKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release)
{
    const CWKeyerSettings& settings = m_cwKeyer->getSettings();

    if ((key == settings.m_dotKey) && (keyModifiers == settings.m_dotKeyModifiers))
    {
        qDebug("CWKeyerGUI::keyboardKeyPressed: dot %s", release ? "released" : "pressed");
        if (release) {
            m_cwKeyer->setKeyboardSilence();
        } else {
            m_cwKeyer->setKeyboardDots();
        }
    }
    else if ((key == settings.m_dashKey) && (keyModifiers == settings.m_dashKeyModifiers))
    {
        qDebug("CWKeyerGUI::keyboardKeyPressed: dash %s", release ? "released" : "pressed");
        if (release) {
            m_cwKeyer->setKeyboardSilence();
        } else {
            m_cwKeyer->setKeyboardDashes();
        }
    }
}

// === End SLOTS ==============================================================

void CWKeyerGUI::applySettings(bool force)
{
    if (m_doApplySettings && m_cwKeyer)
    {
        CWKeyer::MsgConfigureCWKeyer* message = CWKeyer::MsgConfigureCWKeyer::create(m_settings, force);
        m_cwKeyer->getInputMessageQueue()->push(message);
    }
}

void CWKeyerGUI::displaySettings()
{
    blockApplySettings(true);

    ui->playLoopCW->setChecked(m_settings.m_loop);

    ui->playDots->setEnabled((m_settings.m_mode == CWKeyerSettings::CWDots) || (m_settings.m_mode == CWKeyerSettings::CWNone));
    ui->playDots->setChecked(m_settings.m_mode == CWKeyerSettings::CWDots);

    ui->playDashes->setEnabled((m_settings.m_mode == CWKeyerSettings::CWDashes) || (m_settings.m_mode == CWKeyerSettings::CWNone));
    ui->playDashes->setChecked(m_settings.m_mode == CWKeyerSettings::CWDashes);

    ui->playText->setEnabled((m_settings.m_mode == CWKeyerSettings::CWText) || (m_settings.m_mode == CWKeyerSettings::CWNone));
    ui->playText->setChecked(m_settings.m_mode == CWKeyerSettings::CWText);

    ui->keyboardKeyer->setEnabled((m_settings.m_mode == CWKeyerSettings::CWKeyboard) || (m_settings.m_mode == CWKeyerSettings::CWNone));
    ui->keyboardKeyer->setChecked(m_settings.m_mode == CWKeyerSettings::CWKeyboard);

    ui->cwTextEdit->setText(m_settings.m_text);
    ui->cwSpeed->setValue(m_settings.m_wpm);
    ui->cwSpeedText->setText(QString("%1").arg(m_settings.m_wpm));
    ui->keyingStyle->setChecked(!m_settings.m_keyboardIambic);

    setKeyLabel(ui->keyDotLabel, m_settings.m_dotKey, m_settings.m_dotKeyModifiers);
    setKeyLabel(ui->keyDashLabel, m_settings.m_dashKey, m_settings.m_dashKeyModifiers);

    blockApplySettings(false);
}

void CWKeyerGUI::setKeyLabel(QLabel *label, Qt::Key key, Qt::KeyboardModifiers keyModifiers)
{
    if (key == 0)
    {
        label->setText("");
    }
    else if (keyModifiers != Qt::NoModifier)
    {
        QString altGrStr = keyModifiers & Qt::GroupSwitchModifier ? "Gr " : "";
        int maskedModifiers = ((int) keyModifiers & 0x3FFFFFFF) + (((int) keyModifiers & 0x40000000)>>3);
        label->setText(altGrStr + QKeySequence(maskedModifiers, key).toString());
    }
    else
    {
        label->setText(QKeySequence(key).toString());
    }
}

void CWKeyerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}
