///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

#include "device/deviceenumerator.h"
#include "samplingdevicesdock.h"

SamplingDevicesDock::SamplingDevicesDock(QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags),
    m_currentTabIndex(0)
{
    m_titleBar = new QWidget();
    m_titleBarLayout = new QHBoxLayout();
    m_titleBarLayout->setMargin(1);
    m_titleBar->setLayout(m_titleBarLayout);

    m_titleLabel = new QLabel();
    m_titleLabel->setText(QString("Sampling device")); // will be changed dynamically

    m_changeDeviceButton = new QPushButton();
    QIcon changeIcon(":/swap.png");
    m_changeDeviceButton->setIcon(changeIcon);
    m_changeDeviceButton->setToolTip("Change device");
    m_changeDeviceButton->setFixedSize(16, 16);

    m_reloadDeviceButton = new QPushButton();
    QIcon reloadIcon(":/recycle.png");
    m_reloadDeviceButton->setIcon(reloadIcon);
    m_reloadDeviceButton->setToolTip("Reload device");
    m_reloadDeviceButton->setFixedSize(16, 16);

    m_normalButton = new QPushButton();
    QIcon normalIcon = style()->standardIcon(QStyle::SP_TitleBarNormalButton, 0, this);
    m_normalButton->setIcon(normalIcon);
    m_normalButton->setToolTip("Dock/undock");
    m_normalButton->setFixedSize(12, 12);

    m_closeButton = new QPushButton();
    QIcon closeIcon = style()->standardIcon(QStyle::SP_TitleBarCloseButton, 0, this);
    m_closeButton->setIcon(closeIcon);
    m_closeButton->setToolTip("Close");
    m_closeButton->setFixedSize(12, 12);

    m_titleBarLayout->addWidget(m_changeDeviceButton);
    m_titleBarLayout->addWidget(m_reloadDeviceButton);
    m_titleBarLayout->addWidget(m_titleLabel);
    m_titleBarLayout->addWidget(m_normalButton);
    m_titleBarLayout->addWidget(m_closeButton);
    setTitleBarWidget(m_titleBar);

    QObject::connect(
        m_changeDeviceButton,
        &QPushButton::clicked,
        this,
        &SamplingDevicesDock::openChangeDeviceDialog
    );

    QObject::connect(
        m_reloadDeviceButton,
        &QPushButton::clicked,
        this,
        &SamplingDevicesDock::reloadDevice
    );

    QObject::connect(
        m_normalButton,
        &QPushButton::clicked,
        this,
        &SamplingDevicesDock::toggleFloating
    );

    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(hide()));
}

SamplingDevicesDock::~SamplingDevicesDock()
{
    for (int i = 0; i < m_devicesInfo.size(); i++) {
        delete m_devicesInfo[i].m_samplingDeviceDialog;
    }

    delete m_closeButton;
    delete m_normalButton;
    delete m_reloadDeviceButton;
    delete m_changeDeviceButton;
    delete m_titleLabel;
    delete m_titleBarLayout;
    delete m_titleBar;
}

void SamplingDevicesDock::addDevice(int deviceType, int deviceTabIndex)
{
    m_devicesInfo.push_back(DeviceInfo{
        deviceType,
        deviceTabIndex,
        new SamplingDeviceDialog(deviceType, deviceTabIndex, this)
    });

    setCurrentTabIndex(deviceTabIndex);
}

void SamplingDevicesDock::removeLastDevice()
{
    if (m_devicesInfo.size() > 0)
    {
        delete m_devicesInfo.back().m_samplingDeviceDialog;
        m_devicesInfo.pop_back();
    }
}

void SamplingDevicesDock::setCurrentTabIndex(int deviceTabIndex)
{
    m_currentTabIndex = deviceTabIndex;
    QString newTitle;
    m_devicesInfo[m_currentTabIndex].m_samplingDeviceDialog->getDeviceId(newTitle);
    int newTitleSize = newTitle.size();

    if (newTitleSize > 0)
    {
        if (newTitleSize > 40) {
            newTitle.chop(newTitleSize - 40);
        }

        m_titleLabel->setText(newTitle);
    }
}

void SamplingDevicesDock::setSelectedDeviceIndex(int deviceTabIndex, int deviceIndex)
{
    if (deviceTabIndex < m_devicesInfo.size())
    {
        m_devicesInfo[deviceTabIndex].m_samplingDeviceDialog->setSelectedDeviceIndex(deviceIndex);
        setCurrentTabIndex(m_currentTabIndex); // update title
    }
}

void SamplingDevicesDock::toggleFloating()
{
    setFloating(!isFloating());
}

void SamplingDevicesDock::reloadDevice()
{
    emit deviceChanged(
        m_devicesInfo[m_currentTabIndex].m_deviceType,
        m_devicesInfo[m_currentTabIndex].m_deviceTabIndex,
        m_devicesInfo[m_currentTabIndex].m_samplingDeviceDialog->getSelectedDeviceIndex()
    );
}

void SamplingDevicesDock::openChangeDeviceDialog()
{
    if (m_currentTabIndex < m_devicesInfo.size())
    {
        m_devicesInfo[m_currentTabIndex].m_samplingDeviceDialog->exec();

        if (m_devicesInfo[m_currentTabIndex].m_samplingDeviceDialog->hasChanged())
        {
            setCurrentTabIndex(m_currentTabIndex); // update title
            emit deviceChanged(
                m_devicesInfo[m_currentTabIndex].m_deviceType,
                m_devicesInfo[m_currentTabIndex].m_deviceTabIndex,
                m_devicesInfo[m_currentTabIndex].m_samplingDeviceDialog->getSelectedDeviceIndex()
            );
        }
    }
}
