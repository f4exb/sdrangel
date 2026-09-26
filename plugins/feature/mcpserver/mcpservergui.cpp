///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QMessageBox>
#include <QFileDialog>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"

#include "ui_mcpservergui.h"
#include "mcpserver.h"
#include "mcpservergui.h"
#include "mcpcodexconfig.h"
#include "mcpclaudeextension.h"

MCPServerGUI* MCPServerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	MCPServerGUI* gui = new MCPServerGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void MCPServerGUI::destroy()
{
	delete this;
}

void MCPServerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray MCPServerGUI::serialize() const
{
    return m_settings.serialize();
}

bool MCPServerGUI::deserialize(const QByteArray& data)
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

bool MCPServerGUI::handleMessage(const Message& message)
{
    if (MCPServer::MsgConfigureMCPServer::match(message))
    {
        qDebug("MCPServerGUI::handleMessage: MCPServer::MsgConfigureMCPServer");
        const MCPServer::MsgConfigureMCPServer& cfg = (MCPServer::MsgConfigureMCPServer&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }

	return false;
}

void MCPServerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MCPServerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
}

MCPServerGUI::MCPServerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::MCPServerGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/mcpserver/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_mcpServer = reinterpret_cast<MCPServer*>(feature);
    m_mcpServer->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, &MCPServerGUI::customContextMenuRequested, this, &MCPServerGUI::onMenuDialogCalled);
    connect(getInputMessageQueue(), &MessageQueue::messageEnqueued, this, &MCPServerGUI::handleInputMessages);
    connect(m_mcpServer, &Feature::stateChanged, this, &MCPServerGUI::updateFeatureState);

	connect(&m_statisticsTimer, &QTimer::timeout, this, &MCPServerGUI::updateStatistics);
	m_statisticsTimer.start(1000);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    updateFeatureState();
    updateStatistics();
    m_resizer.enableChildMouseTracking();
}

MCPServerGUI::~MCPServerGUI()
{
	delete ui;
}

void MCPServerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_settingsKeys.append("workspaceIndex");
    m_feature->setWorkspaceIndex(index);
}

void MCPServerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MCPServerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->address->setText(m_settings.m_address);
    ui->port->setValue(m_settings.m_port);
    ui->token->setText(m_settings.m_token);
    ui->captureDir->setText(m_settings.m_captureDir);
    ui->url->setText(m_mcpServer->getServerURL());
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void MCPServerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void MCPServerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        MCPServer::MsgStartStop *message = MCPServer::MsgStartStop::create(checked);
        m_mcpServer->getInputMessageQueue()->push(message);
    }
}

void MCPServerGUI::on_address_editingFinished()
{
    m_settings.m_address = ui->address->text().trimmed();
    m_settingsKeys.append("address");
    ui->url->setText(QString("http://%1:%2/mcp").arg(m_settings.m_address.isEmpty() ? "0.0.0.0" : m_settings.m_address).arg(m_settings.m_port));
    applySettings();
}

void MCPServerGUI::on_port_valueChanged(int value)
{
    m_settings.m_port = value;
    m_settingsKeys.append("port");
    ui->url->setText(QString("http://%1:%2/mcp").arg(m_settings.m_address.isEmpty() ? "0.0.0.0" : m_settings.m_address).arg(m_settings.m_port));
    applySettings();
}

void MCPServerGUI::on_token_editingFinished()
{
    m_settings.m_token = ui->token->text().trimmed();
    m_settingsKeys.append("token");
    applySettings();
}

void MCPServerGUI::on_captureDir_editingFinished()
{
    m_settings.m_captureDir = ui->captureDir->text().trimmed();
    m_settingsKeys.append("captureDir");
    applySettings();
}

void MCPServerGUI::on_captureDirBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Capture directory"), m_settings.m_captureDir);

    if (!dir.isEmpty())
    {
        m_settings.m_captureDir = dir;
        ui->captureDir->setText(dir);
        m_settingsKeys.append("captureDir");
        applySettings();
    }
}

void MCPServerGUI::on_addToCodex_clicked()
{
    // The settings hold what the server is configured with, which is what a client has to use
    const QString url = MCPCodexConfig::serverUrl(m_settings.m_address, m_settings.m_port);
    QString message;
    MCPCodexConfig::Result result = MCPCodexConfig::addServer("sdrangel", url, m_settings.m_token, message);

    if (result == MCPCodexConfig::Failed)
    {
        QMessageBox::critical(this, tr("Add to Codex"), message);
    }
    else
    {
        // Codex reads its configuration once, at startup
        QMessageBox::information(this, tr("Add to Codex"),
            tr("%1\n\nRestart Codex for it to see the server.").arg(message));
    }
}

void MCPServerGUI::on_addToClaude_clicked()
{
    QString message;
    MCPClaudeExtension::Result result = MCPClaudeExtension::install(message);

    if (result == MCPClaudeExtension::Launched) {
        QMessageBox::information(this, tr("Add to Claude Desktop"), message);
    } else {
        QMessageBox::warning(this, tr("Add to Claude Desktop"), message);
    }
}

void MCPServerGUI::updateFeatureState()
{
    updateStartStopButton(ui->startStop);
}

void MCPServerGUI::updateStatistics()
{
    int streams = m_mcpServer->getStreamCount();
    ui->requests->setText(streams > 0
        ? QString("Requests: %1  Streams: %2").arg(m_mcpServer->getRequestCount()).arg(streams)
        : QString("Requests: %1").arg(m_mcpServer->getRequestCount()));
    ui->lastRequest->setText(m_mcpServer->getLastRequest());
}

void MCPServerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    MCPServer::MsgConfigureMCPServer* message = MCPServer::MsgConfigureMCPServer::create(m_settings, m_settingsKeys, force);
	    m_mcpServer->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void MCPServerGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &MCPServerGUI::on_startStop_toggled);
	QObject::connect(ui->address, &QLineEdit::editingFinished, this, &MCPServerGUI::on_address_editingFinished);
	QObject::connect(ui->port, qOverload<int>(&QSpinBox::valueChanged), this, &MCPServerGUI::on_port_valueChanged);
	QObject::connect(ui->token, &QLineEdit::editingFinished, this, &MCPServerGUI::on_token_editingFinished);
	QObject::connect(ui->captureDir, &QLineEdit::editingFinished, this, &MCPServerGUI::on_captureDir_editingFinished);
	QObject::connect(ui->captureDirBrowse, &QToolButton::clicked, this, &MCPServerGUI::on_captureDirBrowse_clicked);
	QObject::connect(ui->addToCodex, &QPushButton::clicked, this, &MCPServerGUI::on_addToCodex_clicked);
	QObject::connect(ui->addToClaude, &QPushButton::clicked, this, &MCPServerGUI::on_addToClaude_clicked);
}
