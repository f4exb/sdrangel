///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
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
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QFrame>

#include "gui/samplingdevicedialog.h"
#include "workspace.h"

Workspace::Workspace(int index, QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags),
    m_index(index),
    m_featureAddDialog(this)
{
    m_mdi = new QMdiArea(this);
    m_mdi->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdi->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidget(m_mdi);

    setWindowTitle(tr("W%1").arg(m_index));
    setObjectName(tr("W%1").arg(m_index));

    m_titleBar = new QWidget();
    m_titleBarLayout = new QHBoxLayout();
    m_titleBarLayout->setMargin(0);
    m_titleBar->setLayout(m_titleBarLayout);

    m_titleLabel = new QLabel();
    m_titleLabel->setFixedSize(32, 16);
    m_titleLabel->setStyleSheet("QLabel { background-color: rgb(128, 128, 128); qproperty-alignment: AlignCenter; }");
    m_titleLabel->setText(windowTitle());

    m_addRxDeviceButton = new QPushButton();
    QIcon addRxIcon(":/rx.png");
    m_addRxDeviceButton->setIcon(addRxIcon);
    m_addRxDeviceButton->setToolTip("Add Rx device");
    m_addRxDeviceButton->setFixedSize(20, 20);

    m_addTxDeviceButton = new QPushButton();
    QIcon addTxIcon(":/tx.png");
    m_addTxDeviceButton->setIcon(addTxIcon);
    m_addTxDeviceButton->setToolTip("Add Tx device");
    m_addTxDeviceButton->setFixedSize(20, 20);

    m_addMIMODeviceButton = new QPushButton();
    QIcon addMIMOIcon(":/mimo.png");
    m_addMIMODeviceButton->setIcon(addMIMOIcon);
    m_addMIMODeviceButton->setToolTip("Add MIMO device");
    m_addMIMODeviceButton->setFixedSize(20, 20);

    m_vline1 = new QFrame();
    m_vline1->setFrameShape(QFrame::VLine);
    m_vline1->setFrameShadow(QFrame::Sunken);

    m_addFeatureButton = new QPushButton();
    QIcon addFeatureIcon(":/tool.png");
    m_addFeatureButton->setIcon(addFeatureIcon);
    m_addFeatureButton->setToolTip("Add features");
    m_addFeatureButton->setFixedSize(20, 20);

    m_featurePresetsButton = new QPushButton();
    QIcon presetsIcon(":/star.png");
    m_featurePresetsButton->setIcon(presetsIcon);
    m_featurePresetsButton->setToolTip("Feature presets");
    m_featurePresetsButton->setFixedSize(20, 20);

    m_vline2 = new QFrame();
    m_vline2->setFrameShape(QFrame::VLine);
    m_vline2->setFrameShadow(QFrame::Sunken);

    m_cascadeSubWindows = new QPushButton();
    QIcon cascadeSubWindowsIcon(":/cascade.png");
    m_cascadeSubWindows->setIcon(cascadeSubWindowsIcon);
    m_cascadeSubWindows->setToolTip("Cascade sub windows");
    m_cascadeSubWindows->setFixedSize(20, 20);

    m_tileSubWindows = new QPushButton();
    QIcon tileSubWindowsIcon(":/tiles.png");
    m_tileSubWindows->setIcon(tileSubWindowsIcon);
    m_tileSubWindows->setToolTip("Tile sub windows");
    m_tileSubWindows->setFixedSize(20, 20);

    m_normalButton = new QPushButton();
    QIcon normalIcon(":/dock.png");
    m_normalButton->setIcon(normalIcon);
    m_normalButton->setToolTip("Dock/undock");
    m_normalButton->setFixedSize(20, 20);

    m_closeButton = new QPushButton();
    QIcon closeIcon(":/hide.png");
    m_closeButton->setIcon(closeIcon);
    m_closeButton->setToolTip("Hide workspace");
    m_closeButton->setFixedSize(20, 20);

    m_titleBarLayout->addWidget(m_titleLabel);
    m_titleBarLayout->addWidget(m_addRxDeviceButton);
    m_titleBarLayout->addWidget(m_addTxDeviceButton);
    m_titleBarLayout->addWidget(m_addMIMODeviceButton);
    m_titleBarLayout->addWidget(m_vline1);
    m_titleBarLayout->addWidget(m_addFeatureButton);
    m_titleBarLayout->addWidget(m_featurePresetsButton);
    m_titleBarLayout->addWidget(m_vline2);
    m_titleBarLayout->addWidget(m_cascadeSubWindows);
    m_titleBarLayout->addWidget(m_tileSubWindows);
    m_titleBarLayout->addStretch(1);
    m_titleBarLayout->addWidget(m_normalButton);
    m_titleBarLayout->addWidget(m_closeButton);
    setTitleBarWidget(m_titleBar);

    QObject::connect(
        m_addRxDeviceButton,
        &QPushButton::clicked,
        this,
        &Workspace::addRxDeviceClicked
    );

    QObject::connect(
        m_addTxDeviceButton,
        &QPushButton::clicked,
        this,
        &Workspace::addTxDeviceClicked
    );

    QObject::connect(
        m_addMIMODeviceButton,
        &QPushButton::clicked,
        this,
        &Workspace::addMIMODeviceClicked
    );

    QObject::connect(
        m_addFeatureButton,
        &QPushButton::clicked,
        this,
        &Workspace::addFeatureDialog
    );

    QObject::connect(
        m_featurePresetsButton,
        &QPushButton::clicked,
        this,
        &Workspace::featurePresetsDialog
    );

    QObject::connect(
        m_cascadeSubWindows,
        &QPushButton::clicked,
        this,
        &Workspace::cascadeSubWindows
    );

    QObject::connect(
        m_tileSubWindows,
        &QPushButton::clicked,
        this,
        &Workspace::tileSubWindows
    );

    QObject::connect(
        m_normalButton,
        &QPushButton::clicked,
        this,
        &Workspace::toggleFloating
    );

    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(hide()));

    QObject::connect(
        &m_featureAddDialog,
        &FeatureAddDialog::addFeature,
        this,
        &Workspace::addFeatureEmitted
    );
}

Workspace::~Workspace()
{
    qDebug("Workspace::~Workspace");
    delete m_closeButton;
    delete m_normalButton;
    delete m_tileSubWindows;
    delete m_cascadeSubWindows;
    delete m_vline2;
    delete m_vline1;
    delete m_addRxDeviceButton;
    delete m_addTxDeviceButton;
    delete m_addMIMODeviceButton;
    delete m_addFeatureButton;
    delete m_featurePresetsButton;
    delete m_titleLabel;
    delete m_titleBarLayout;
    delete m_titleBar;
    qDebug("Workspace::~Workspace: about to delete MDI");
    delete m_mdi;
    qDebug("Workspace::~Workspace: end");
}

void Workspace::toggleFloating()
{
    setFloating(!isFloating());
}

void Workspace::addRxDeviceClicked()
{
    SamplingDeviceDialog dialog(0, this);

    if (dialog.exec() == QDialog::Accepted) {
        emit addRxDevice(this, dialog.getSelectedDeviceIndex());
    }
}

void Workspace::addTxDeviceClicked()
{
    SamplingDeviceDialog dialog(1, this);

    if (dialog.exec() == QDialog::Accepted) {
        emit addTxDevice(this, dialog.getSelectedDeviceIndex());
    }
}

void Workspace::addMIMODeviceClicked()
{
    SamplingDeviceDialog dialog(2, this);

    if (dialog.exec() == QDialog::Accepted) {
        emit addMIMODevice(this, dialog.getSelectedDeviceIndex());
    }
}

void Workspace::addFeatureDialog()
{
    m_featureAddDialog.exec();
}

void Workspace::addFeatureEmitted(int featureIndex)
{
    if (featureIndex >= 0) {
        emit addFeature(this, featureIndex);
    }
}

void Workspace::featurePresetsDialog()
{
    QPoint p = mapFromGlobal(QCursor::pos());
    emit featurePresetsDialogRequested(p, this);
}

void Workspace::cascadeSubWindows()
{
    m_mdi->cascadeSubWindows();
}

void Workspace::tileSubWindows()
{
    m_mdi->tileSubWindows();
}

void Workspace::addToMdiArea(QMdiSubWindow *sub)
{
    m_mdi->addSubWindow(sub);
    sub->show();
}

void Workspace::removeFromMdiArea(QMdiSubWindow *sub)
{
    m_mdi->removeSubWindow(sub);
}

int Workspace::getNumberOfSubWindows() const
{
    return m_mdi->subWindowList().size();
}

QByteArray Workspace::saveMdiGeometry()
{
    return qCompress(m_mdi->saveGeometry());
}

void Workspace::restoreMdiGeometry(const QByteArray& blob)
{
    m_mdi->restoreGeometry(qUncompress(blob));
}
