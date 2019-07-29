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
#include "ui_cwkeyergui.h"
#include "dsp/cwkeyer.h"
#include "util/simpleserializer.h"
#include "util/messagequeue.h"
#include "commandkeyreceiver.h"
#include "mainwindow.h"

CWKeyerGUI::CWKeyerGUI(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::CWKeyerGUI),
    m_messageQueue(nullptr),
    m_cwKeyer(nullptr),
    m_doApplySettings(true),
    m_keyScope(NoKeyScope)
{
    ui->setupUi(this);
    m_commandKeyReceiver = new CommandKeyReceiver();
    m_commandKeyReceiver->setRelease(true);
    this->installEventFilter(m_commandKeyReceiver);
}

CWKeyerGUI::~CWKeyerGUI()
{
    this->releaseKeyboard(); // just in case
    m_commandKeyReceiver->deleteLater();
    delete ui;
}

void CWKeyerGUI::setBuddies(MessageQueue* messageQueue, CWKeyer* cwKeyer)
{
    m_messageQueue = messageQueue;
    m_cwKeyer = cwKeyer;
    applySettings();
    sendSettings();
    displaySettings(m_cwKeyer->getSettings());
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
        sendSettings();

        return true;
    }
    else
    {
        return false;
    }
}

// === SLOTS ==================================================================

void CWKeyerGUI::on_cwTextClear_clicked(bool checked)
{
    (void) checked;
    ui->cwTextEdit->clear();
    m_cwKeyer->setText("");
}

void CWKeyerGUI::on_cwTextEdit_editingFinished()
{
    if (m_doApplySettings)
    {
        m_cwKeyer->setText(ui->cwTextEdit->text());
        sendSettings();
    }
}

void CWKeyerGUI::on_cwSpeed_valueChanged(int value)
{
    ui->cwSpeedText->setText(QString("%1").arg(value));

    if (m_doApplySettings)
    {
        m_cwKeyer->setWPM(value);
        sendSettings();
    }
}

void CWKeyerGUI::on_playDots_toggled(bool checked)
{
    //ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);

    if (m_doApplySettings)
    {
        m_cwKeyer->setMode(checked ? CWKeyerSettings::CWDots : CWKeyerSettings::CWNone);
        sendSettings();
    }
}

void CWKeyerGUI::on_playDashes_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    //ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);

    if (m_doApplySettings)
    {
        m_cwKeyer->setMode(checked ? CWKeyerSettings::CWDashes : CWKeyerSettings::CWNone);
        sendSettings();
    }
}

void CWKeyerGUI::on_playText_toggled(bool checked)
{
    ui->playDots->setEnabled(!checked); // release other source inputs
    ui->playDashes->setEnabled(!checked);
    //ui->playText->setEnabled(!checked);
    ui->keyboardKeyer->setEnabled(!checked);

    if (m_doApplySettings)
    {
        m_cwKeyer->setMode(checked ? CWKeyerSettings::CWText : CWKeyerSettings::CWNone);
        sendSettings();
    }

    if (checked) {
        ui->playStop->setChecked(true);
    } else {
        ui->playStop->setChecked(false);
    }
}

void CWKeyerGUI::on_playLoopCW_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        m_cwKeyer->setLoop(checked);
        sendSettings();
    }
}

void CWKeyerGUI::on_playStop_toggled(bool checked)
{
    if (checked) {
        m_cwKeyer->resetText();
    } else {
        m_cwKeyer->stopText();
    }
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
//    qDebug("CWKeyerGUI::commandKeyPressed: key: %x", m_key);
//    qDebug("CWKeyerGUI::commandKeyPressed: has modifiers: %x", QFlags<Qt::KeyboardModifier>::Int(keyModifiers));

    if (m_keyScope == DotKeyScope)
    {
        m_dotKey = key;
        m_dotKeyModifiers = keyModifiers;
        setKeyLabel(ui->keyDotLabel, key, keyModifiers);
        ui->keyDotCapture->setChecked(false);

        if (m_doApplySettings)
        {
            m_cwKeyer->setDotKey(key);
            m_cwKeyer->setDotKeyModifiers(keyModifiers);
            sendSettings();
        }
    }
    else if (m_keyScope == DashKeyScope)
    {
        m_dashKey = key;
        m_dashKeyModifiers = keyModifiers;
        setKeyLabel(ui->keyDashLabel, key, keyModifiers);
        ui->keyDashCapture->setChecked(false);

        if (m_doApplySettings)
        {
            m_cwKeyer->setDashKey(key);
            m_cwKeyer->setDashKeyModifiers(keyModifiers);
            sendSettings();
        }
    }

    m_commandKeyReceiver->setRelease(true);
}

void CWKeyerGUI::on_keyboardKeyer_toggled(bool checked)
{
    qDebug("CWKeyerGUI::on_keyboardKeyer_toggled: %s", checked ? "true" : "false");
    ui->playDots->setEnabled(!checked);   // block or release other source inputs
    ui->playDashes->setEnabled(!checked);
    ui->playText->setEnabled(!checked);

    if (m_doApplySettings)
    {
        m_cwKeyer->setMode(checked ? CWKeyerSettings::CWKeyboard : CWKeyerSettings::CWNone);
        sendSettings();
    }

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
    }
    else if ((key == settings.m_dashKey) && (keyModifiers == settings.m_dashKeyModifiers))
    {
        qDebug("CWKeyerGUI::keyboardKeyPressed: dash %s", release ? "released" : "pressed");
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

    m_cwKeyer->setDotKey(m_dotKey);
    m_cwKeyer->setDotKeyModifiers(m_dotKeyModifiers);
    m_cwKeyer->setDashKey(m_dashKey);
    m_cwKeyer->setDashKeyModifiers(m_dashKeyModifiers);
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

    setKeyLabel(ui->keyDotLabel, settings.m_dotKey, settings.m_dotKeyModifiers);
    setKeyLabel(ui->keyDashLabel, settings.m_dashKey, settings.m_dashKeyModifiers);

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
        int maskedModifiers = (keyModifiers & 0x3FFFFFFF) + ((keyModifiers & 0x40000000)>>3);
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

void CWKeyerGUI::sendSettings()
{
    if (m_cwKeyer && m_messageQueue)
    {
        CWKeyer::MsgConfigureCWKeyer *msg = CWKeyer::MsgConfigureCWKeyer::create(m_cwKeyer->getSettings(), false);
        m_messageQueue->push(msg);
    }
}
