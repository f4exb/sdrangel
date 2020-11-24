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

#include "featuresdock.h"

FeaturesDock::FeaturesDock(QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags),
    m_featureAddDialog(this)
{
    m_titleBar = new QWidget();
    m_titleBarLayout = new QHBoxLayout();
    m_titleBarLayout->setMargin(0);
    m_titleBar->setLayout(m_titleBarLayout);

    m_titleLabel = new QLabel();
    m_titleLabel->setText(QString("Features"));

    m_addFeatureButton = new QPushButton();
    QIcon addIcon(":/create.png");
    m_addFeatureButton->setIcon(addIcon);
    m_addFeatureButton->setToolTip("Add features");
    m_addFeatureButton->setFixedSize(16, 16);

    m_presetsButton = new QPushButton();
    QIcon presetsIcon(":/star.png");
    m_presetsButton->setIcon(presetsIcon);
    m_presetsButton->setToolTip("Feature presets");
    m_presetsButton->setFixedSize(16, 16);

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

    m_titleBarLayout->addWidget(m_addFeatureButton);
    m_titleBarLayout->addWidget(m_presetsButton);
    m_titleBarLayout->addWidget(m_titleLabel);
    m_titleBarLayout->addWidget(m_normalButton);
    m_titleBarLayout->addWidget(m_closeButton);
    setTitleBarWidget(m_titleBar);

    QObject::connect(
        m_addFeatureButton,
        &QPushButton::clicked,
        this,
        &FeaturesDock::addFeatureDialog
    );

    QObject::connect(
        m_presetsButton,
        &QPushButton::clicked,
        this,
        &FeaturesDock::presetsDialog
    );

    QObject::connect(
        m_normalButton,
        &QPushButton::clicked,
        this,
        &FeaturesDock::toggleFloating
    );

    QObject::connect(
        &m_featureAddDialog,
        &FeatureAddDialog::addFeature,
        this,
        &FeaturesDock::addFeatureEmitted
    );

    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(hide()));
}

FeaturesDock::~FeaturesDock()
{
    delete m_closeButton;
    delete m_normalButton;
    delete m_addFeatureButton;
    delete m_titleLabel;
    delete m_titleBarLayout;
    delete m_titleBar;
}

void FeaturesDock::toggleFloating()
{
    setFloating(!isFloating());
}

void FeaturesDock::addFeatureDialog()
{
    m_featureAddDialog.exec();
}

void FeaturesDock::presetsDialog()
{
    m_featurePresetsDialog.populateTree();
    m_featurePresetsDialog.exec();
}

void FeaturesDock::addFeatureEmitted(int featureIndex)
{
    if (featureIndex >= 0) {
        emit addFeature(featureIndex);
    }
}
