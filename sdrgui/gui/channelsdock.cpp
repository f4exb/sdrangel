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

#include "channelsdock.h"

ChannelsDock::ChannelsDock(QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags),
    m_channelAddDialog(this)
{
    m_titleBar = new QWidget();
    m_titleBarLayout = new QHBoxLayout();
    m_titleBarLayout->setMargin(0);
    m_titleBar->setLayout(m_titleBarLayout);

    m_titleLabel = new QLabel();
    m_titleLabel->setText(QString("Channels"));

    m_addChannelButton = new QPushButton();
    QIcon addIcon(":/create.png");
    m_addChannelButton->setIcon(addIcon);
    m_addChannelButton->setToolTip("Add channels");
    m_addChannelButton->setFixedSize(16, 16);

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

    m_titleBarLayout->addWidget(m_addChannelButton);
    m_titleBarLayout->addWidget(m_titleLabel);
    m_titleBarLayout->addWidget(m_normalButton);
    m_titleBarLayout->addWidget(m_closeButton);
    setTitleBarWidget(m_titleBar);

    QObject::connect(
        m_addChannelButton,
        &QPushButton::clicked,
        this,
        &ChannelsDock::addChannelDialog
    );

    QObject::connect(
        m_normalButton,
        &QPushButton::clicked,
        this,
        &ChannelsDock::toggleFloating
    );

    QObject::connect(
        &m_channelAddDialog,
        &ChannelAddDialog::addChannel,
        this,
        &ChannelsDock::addChannelEmitted
    );

    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(hide()));
}

ChannelsDock::~ChannelsDock()
{
    delete m_closeButton;
    delete m_normalButton;
    delete m_addChannelButton;
    delete m_titleLabel;
    delete m_titleBarLayout;
    delete m_titleBar;
}

void ChannelsDock::toggleFloating()
{
    setFloating(!isFloating());
}

void ChannelsDock::addChannelDialog()
{
    m_channelAddDialog.exec();

}

void ChannelsDock::addChannelEmitted(int channelIndex)
{
    if (channelIndex >= 0) {
        emit addChannel(channelIndex);
    }
}