///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <cmath>
#include <QMessageBox>
#include <QLineEdit>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"

#include "ui_pertestergui.h"
#include "pertester.h"
#include "pertestergui.h"
#include "pertesterreport.h"

PERTesterGUI* PERTesterGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    PERTesterGUI* gui = new PERTesterGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void PERTesterGUI::destroy()
{
    delete this;
}

void PERTesterGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray PERTesterGUI::serialize() const
{
    return m_settings.serialize();
}

bool PERTesterGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
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

bool PERTesterGUI::handleMessage(const Message& message)
{
    if (PERTester::MsgConfigurePERTester::match(message))
    {
        qDebug("PERTesterGUI::handleMessage: PERTester::MsgConfigurePERTester");
        const PERTester::MsgConfigurePERTester& cfg = (PERTester::MsgConfigurePERTester&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (PERTesterReport::MsgReportStats::match(message))
    {
        PERTesterReport::MsgReportStats& stats = (PERTesterReport::MsgReportStats&) message;
        int tx = stats.getTx();
        int rxMatched = stats.getRxMatched();
        double per = tx == 0 ? 0.0 : 100.0 - (rxMatched/(double)tx)*100.0;
        ui->transmittedText->setText(QString("%1").arg(tx));
        ui->receivedMatchedText->setText(QString("%1").arg(rxMatched));
        ui->receivedUnmatchedText->setText(QString("%1").arg(stats.getRxUnmatched()));
        ui->perText->setText(QString("%1%").arg(per, 0, 'f', 1));
        return true;
    }

    return false;
}

void PERTesterGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void PERTesterGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

PERTesterGUI::PERTesterGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::PERTesterGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/pertester/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_perTester = reinterpret_cast<PERTester*>(feature);
    m_perTester->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    displaySettings();
    applySettings(true);
    makeUIConnections();
}

PERTesterGUI::~PERTesterGUI()
{
    delete ui;
}

void PERTesterGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void PERTesterGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PERTesterGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->packetCount->setValue(m_settings.m_packetCount);
    ui->start->setCurrentIndex((int)m_settings.m_start);
    ui->satellites->setVisible(m_settings.m_start != PERTesterSettings::START_IMMEDIATELY);
    ui->satellitesLabel->setVisible(m_settings.m_start != PERTesterSettings::START_IMMEDIATELY);
    ui->satellites->setText(m_settings.m_satellites.join(" "));
    ui->interval->setValue(m_settings.m_interval);
    ui->packet->setPlainText(m_settings.m_packet);
    ui->leading->setValue(m_settings.m_ignoreLeadingBytes);
    ui->trailing->setValue(m_settings.m_ignoreTrailingBytes);
    ui->txUDPAddress->setText(m_settings.m_txUDPAddress);
    ui->txUDPPort->setText(QString::number(m_settings.m_txUDPPort));
    ui->rxUDPAddress->setText(m_settings.m_rxUDPAddress);
    ui->rxUDPPort->setText(QString::number(m_settings.m_rxUDPPort));
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void PERTesterGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void PERTesterGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        PERTester::MsgStartStop *message = PERTester::MsgStartStop::create(checked);
        m_perTester->getInputMessageQueue()->push(message);
    }
}

void PERTesterGUI::on_resetStats_clicked()
{
    if (m_doApplySettings)
    {
        PERTester::MsgResetStats *message = PERTester::MsgResetStats::create();
        m_perTester->getInputMessageQueue()->push(message);
    }
}

void PERTesterGUI::on_packetCount_valueChanged(int value)
{
    m_settings.m_packetCount = value;
    applySettings();
}

void PERTesterGUI::on_start_currentIndexChanged(int index)
{
    m_settings.m_start = (PERTesterSettings::Start)index;
    ui->satellites->setVisible(m_settings.m_start != PERTesterSettings::START_IMMEDIATELY);
    ui->satellitesLabel->setVisible(m_settings.m_start != PERTesterSettings::START_IMMEDIATELY);
    applySettings();
    getRollupContents()->arrangeRollups();
}

void PERTesterGUI::on_satellites_editingFinished()
{
    m_settings.m_satellites = ui->satellites->text().trimmed().split(" ");
    applySettings();
}

void PERTesterGUI::on_interval_valueChanged(double value)
{
    m_settings.m_interval = value;
    applySettings();
}

void PERTesterGUI::on_packet_textChanged()
{
    m_settings.m_packet = ui->packet->toPlainText();
    applySettings();
}

void PERTesterGUI::on_leading_valueChanged(int value)
{
    m_settings.m_ignoreLeadingBytes = value;
    applySettings();
}

void PERTesterGUI::on_trailing_valueChanged(int value)
{
    m_settings.m_ignoreTrailingBytes = value;
    applySettings();
}

void PERTesterGUI::on_txUDPAddress_editingFinished()
{
    m_settings.m_txUDPAddress = ui->txUDPAddress->text();
    applySettings();
}

void PERTesterGUI::on_txUDPPort_editingFinished()
{
    m_settings.m_txUDPPort = ui->txUDPPort->text().toInt();
    applySettings();
}

void PERTesterGUI::on_rxUDPAddress_editingFinished()
{
    m_settings.m_rxUDPAddress = ui->rxUDPAddress->text();
    applySettings();
}

void PERTesterGUI::on_rxUDPPort_editingFinished()
{
    m_settings.m_rxUDPPort = ui->rxUDPPort->text().toInt();
    applySettings();
}

void PERTesterGUI::updateStatus()
{
    int state = m_perTester->getState();

    if (m_lastFeatureState != state)
    {
        // We set checked state of start/stop button, in case it was changed via API
        bool oldState;
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(false);
                ui->startStop->blockSignals(oldState);
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(true);
                ui->startStop->blockSignals(oldState);
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_perTester->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void PERTesterGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        PERTester::MsgConfigurePERTester* message = PERTester::MsgConfigurePERTester::create(m_settings, force);
        m_perTester->getInputMessageQueue()->push(message);
    }
}

void PERTesterGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &PERTesterGUI::on_startStop_toggled);
    QObject::connect(ui->resetStats, &QToolButton::clicked, this, &PERTesterGUI::on_resetStats_clicked);
    QObject::connect(ui->packetCount, qOverload<int>(&QSpinBox::valueChanged), this, &PERTesterGUI::on_packetCount_valueChanged);
    QObject::connect(ui->start, qOverload<int>(&QComboBox::currentIndexChanged), this, &PERTesterGUI::on_start_currentIndexChanged);
    QObject::connect(ui->satellites, &QLineEdit::editingFinished, this, &PERTesterGUI::on_satellites_editingFinished);
    QObject::connect(ui->interval, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &PERTesterGUI::on_interval_valueChanged);
    QObject::connect(ui->packet, &QPlainTextEdit::textChanged, this, &PERTesterGUI::on_packet_textChanged);
    QObject::connect(ui->leading, qOverload<int>(&QSpinBox::valueChanged), this, &PERTesterGUI::on_leading_valueChanged);
    QObject::connect(ui->trailing, qOverload<int>(&QSpinBox::valueChanged), this, &PERTesterGUI::on_trailing_valueChanged);
    QObject::connect(ui->txUDPAddress, &QLineEdit::editingFinished, this, &PERTesterGUI::on_txUDPAddress_editingFinished);
    QObject::connect(ui->txUDPPort, &QLineEdit::editingFinished, this, &PERTesterGUI::on_txUDPPort_editingFinished);
    QObject::connect(ui->rxUDPAddress, &QLineEdit::editingFinished, this, &PERTesterGUI::on_rxUDPAddress_editingFinished);
    QObject::connect(ui->rxUDPPort, &QLineEdit::editingFinished, this, &PERTesterGUI::on_rxUDPPort_editingFinished);
}
