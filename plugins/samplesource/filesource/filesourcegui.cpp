///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <iostream>

#include "ui_filesourcegui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "mainwindow.h"

#include "filesourcegui.h"

FileSourceGui::FileSourceGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::FileSourceGui),
	m_pluginAPI(pluginAPI),
	m_settings(),
	m_sampleSource(NULL)
{
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	displaySettings();

	m_sampleSource = new FileSourceInput(m_pluginAPI->getMainWindowMessageQueue(), m_pluginAPI->getMainWindow()->getMasterTimer());
	m_pluginAPI->setSampleSource(m_sampleSource);
}

FileSourceGui::~FileSourceGui()
{
	delete ui;
}

void FileSourceGui::destroy()
{
	delete this;
}

void FileSourceGui::setName(const QString& name)
{
	setObjectName(name);
}

QString FileSourceGui::getName() const
{
	return objectName();
}

void FileSourceGui::resetToDefaults()
{
	m_generalSettings.resetToDefaults();
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray FileSourceGui::serializeGeneral() const
{
	return m_generalSettings.serialize();
}

bool FileSourceGui::deserializeGeneral(const QByteArray&data)
{
	if(m_generalSettings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

qint64 FileSourceGui::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

QByteArray FileSourceGui::serialize() const
{
	return m_settings.serialize();
}

bool FileSourceGui::deserialize(const QByteArray& data)
{
	if(m_settings.deserialize(data)) {
		displaySettings();
		sendSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool FileSourceGui::handleMessage(Message* message)
{
	if(FileSourceInput::MsgReportFileSource::match(message)) {
		displaySettings();
		message->completed();
		return true;
	} else {
		return false;
	}
}

void FileSourceGui::displaySettings()
{
}

void FileSourceGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FileSourceGui::updateHardware()
{
	FileSourceInput::MsgConfigureFileSource* message = FileSourceInput::MsgConfigureFileSource::create(m_generalSettings, m_settings);
	message->submit(m_pluginAPI->getDSPEngineMessageQueue());
	m_updateTimer.stop();
}
