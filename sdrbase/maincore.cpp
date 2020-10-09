///////////////////////////////////////////////////////////////////////////////////
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

#include <QGlobalStatic>
#include <QCoreApplication>
#include <QString>

#include "loggerwithfile.h"
#include "dsp/dsptypes.h"

#include "maincore.h"

MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteInstance, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSavePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeletePreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgLoadFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSaveFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteFeatureSetPreset, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgRemoveLastDeviceSet, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgSetDevice, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddChannel, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteChannel, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgApplySettings, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgAddFeature, Message)
MESSAGE_CLASS_DEFINITION(MainCore::MsgDeleteFeature, Message)

MainCore::MainCore()
{}

MainCore::~MainCore()
{}

Q_GLOBAL_STATIC(MainCore, mainCore)
MainCore *MainCore::instance()
{
	return mainCore;
}

void MainCore::setLoggingOptions()
{
    m_logger->setConsoleMinMessageLevel(m_settings.getConsoleMinLogLevel());

    if (m_settings.getUseLogFile())
    {
        qtwebapp::FileLoggerSettings fileLoggerSettings; // default values

        if (m_logger->hasFileLogger()) {
            fileLoggerSettings = m_logger->getFileLoggerSettings(); // values from file logger if it exists
        }

        fileLoggerSettings.fileName = m_settings.getLogFileName(); // put new values
        m_logger->createOrSetFileLogger(fileLoggerSettings, 2000); // create file logger if it does not exist and apply settings in any case
    }

    if (m_logger->hasFileLogger()) {
        m_logger->setFileMinMessageLevel(m_settings.getFileMinLogLevel());
    }

    m_logger->setUseFileLogger(m_settings.getUseLogFile());

    if (m_settings.getUseLogFile())
    {
#if QT_VERSION >= 0x050400
        QString appInfoStr(QString("%1 %2 Qt %3 %4b %5 %6 DSP Rx:%7b Tx:%8b PID %9")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(QSysInfo::currentCpuArchitecture())
                .arg(QSysInfo::prettyProductName())
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_TX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid()));
#else
        QString appInfoStr(QString("%1 %2 Qt %3 %4b DSP Rx:%5b Tx:%6b PID %7")
                .arg(QCoreApplication::applicationName())
                .arg(QCoreApplication::applicationVersion())
                .arg(QT_VERSION_STR)
                .arg(QT_POINTER_SIZE*8)
                .arg(SDR_RX_SAMP_SZ)
                .arg(SDR_RX_SAMP_SZ)
                .arg(QCoreApplication::applicationPid());
 #endif
        m_logger->logToFile(QtInfoMsg, appInfoStr);
    }
}
