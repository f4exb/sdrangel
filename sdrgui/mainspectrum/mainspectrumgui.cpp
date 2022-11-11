///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizeGrip>
#include <QObjectCleanupHandler>
#include <QDesktopServices>

#include "mainwindow.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "gui/workspaceselectiondialog.h"
#include "dsp/spectrumvis.h"
#include "mainspectrumgui.h"

MainSpectrumGUI::MainSpectrumGUI(GLSpectrum *spectrum, GLSpectrumGUI *spectrumGUI, QWidget *parent) :
    QMdiSubWindow(parent),
    m_spectrum(spectrum),
    m_spectrumGUI(spectrumGUI),
    m_deviceType(DeviceRx),
    m_deviceSetIndex(0),
    m_drag(false),
    m_resizer(this)
{
    qDebug("MainSpectrumGUI::MainSpectrumGUI: %p", parent);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    m_helpURL = "sdrgui/mainspectrum/readme.md";
    setObjectName("MainSpectrumGUI");
    setStyleSheet(QString(tr("#MainSpectrumGUI { border: 1px solid %1; background-color: %2; }")
        .arg(palette().highlight().color().darker(115).name()))
        .arg(palette().window().color().name()));

    setMinimumSize(m_MinimumWidth, m_MinimumHeight);

    m_indexLabel = new QLabel();
    m_indexLabel->setFixedSize(32, 16);
    m_indexLabel->setStyleSheet("QLabel { background-color: rgb(128, 128, 128); qproperty-alignment: AlignCenter; }");
    m_indexLabel->setText(tr("X:%1").arg(m_deviceSetIndex));
    m_indexLabel->setToolTip("Device type and set index");

    m_spacerLabel = new QLabel();
    m_spacerLabel->setFixedWidth(5);

    m_titleLabel = new QLabel();
    m_titleLabel->setText("Device");
    m_titleLabel->setToolTip("Device identification");
    m_titleLabel->setFixedHeight(20);
    m_titleLabel->setMinimumWidth(20);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

    m_helpButton = new QPushButton();
    m_helpButton->setFixedSize(20, 20);
    QIcon helpIcon(":/help.png");
    m_helpButton->setIcon(helpIcon);
    m_helpButton->setToolTip("Show spectrum window documentation in browser");

    m_moveButton = new QPushButton();
    m_moveButton->setFixedSize(20, 20);
    QIcon moveIcon(":/exit.png");
    m_moveButton->setIcon(moveIcon);
    m_moveButton->setToolTip("Move to another workspace");

    m_shrinkButton = new QPushButton();
    m_shrinkButton->setFixedSize(20, 20);
    QIcon shrinkIcon(":/shrink.png");
    m_shrinkButton->setIcon(shrinkIcon);
    m_shrinkButton->setToolTip("Adjust window to minimum size");

    m_maximizeButton = new QPushButton();
    m_maximizeButton->setFixedSize(20, 20);
    QIcon maximizeIcon(":/maximize.png");
    m_maximizeButton->setIcon(maximizeIcon);
    m_maximizeButton->setToolTip("Adjust window to maximum size");

    m_hideButton = new QPushButton();
    m_hideButton->setFixedSize(20, 20);
    QIcon hideIcon(":/hide.png");
    m_hideButton->setIcon(hideIcon);
    m_hideButton->setToolTip("Hide device");

    m_statusLabel = new QLabel();
    // m_statusLabel->setText("OK"); // for future use
    m_statusLabel->setFixedHeight(10);
    m_statusLabel->setMinimumWidth(10);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    // m_statusLabel->setToolTip("Spectrum status");

    m_layouts = new QVBoxLayout();
    m_layouts->setContentsMargins(m_resizer.m_gripSize, m_resizer.m_gripSize, m_resizer.m_gripSize, m_resizer.m_gripSize);
    m_layouts->setSpacing(0);

    m_topLayout = new QHBoxLayout();
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->addWidget(m_indexLabel);
    m_topLayout->addWidget(m_spacerLabel);
    m_topLayout->addWidget(m_titleLabel);
    // m_topLayout->addStretch(1);
    m_topLayout->addWidget(m_helpButton);
    m_topLayout->addWidget(m_moveButton);
    m_topLayout->addWidget(m_shrinkButton);
    m_topLayout->addWidget(m_maximizeButton);
    m_topLayout->addWidget(m_hideButton);

    m_spectrumLayout = new QHBoxLayout();
    m_spectrumLayout->addWidget(spectrum);
    spectrum->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_spectrumGUILayout = new QHBoxLayout();
    m_spectrumGUILayout->addWidget(spectrumGUI);

    m_bottomLayout = new QHBoxLayout();
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->addWidget(m_statusLabel);
    m_sizeGripBottomRight = new QSizeGrip(this);
    m_sizeGripBottomRight->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_sizeGripBottomRight->setFixedHeight(10);
    //m_bottomLayout->addStretch(1);
    m_bottomLayout->addWidget(m_sizeGripBottomRight, 0, Qt::AlignBottom | Qt::AlignRight);

    m_layouts->addLayout(m_topLayout);
    m_layouts->addLayout(m_spectrumLayout);
    m_layouts->addLayout(m_spectrumGUILayout);
    m_layouts->addLayout(m_bottomLayout);

    QObjectCleanupHandler().add(layout());
    setLayout(m_layouts);

    connect(m_helpButton, SIGNAL(clicked()), this, SLOT(showHelp()));
    connect(m_moveButton, SIGNAL(clicked()), this, SLOT(openMoveToWorkspaceDialog()));
    connect(m_shrinkButton, SIGNAL(clicked()), this, SLOT(shrinkWindow()));
    connect(m_maximizeButton, SIGNAL(clicked()), this, SLOT(maximizeWindow()));
    connect(this, SIGNAL(forceShrink()), this, SLOT(shrinkWindow()));
    connect(m_hideButton, SIGNAL(clicked()), this, SLOT(hide()));

    connect(spectrum->getSpectrumView(), &GLSpectrumView::requestCenterFrequency, this, &MainSpectrumGUI::onRequestCenterFrequency);
    connect(spectrumGUI, &GLSpectrumGUI::requestCenterFrequency, this, &MainSpectrumGUI::onRequestCenterFrequency);

    m_resizer.enableChildMouseTracking();
    shrinkWindow();
}

MainSpectrumGUI::~MainSpectrumGUI()
{
    qDebug("MainSpectrumGUI::~MainSpectrumGUI");
    m_spectrumLayout->removeWidget(m_spectrum);
    m_spectrumGUILayout->removeWidget(m_spectrumGUI);
    delete m_sizeGripBottomRight;
    delete m_bottomLayout;
    delete m_spectrumGUILayout;
    delete m_spectrumLayout;
    delete m_topLayout;
    delete m_layouts;
    delete m_statusLabel;
    delete m_hideButton;
    delete m_shrinkButton;
    delete m_maximizeButton;
    delete m_moveButton;
    delete m_helpButton;
    delete m_titleLabel;
    delete m_spacerLabel;
    delete m_indexLabel;
    qDebug("MainSpectrumGUI::~MainSpectrumGUI: end");
}

void MainSpectrumGUI::setWorkspaceIndex(int index)
{
    m_workspaceIndex = index;
    m_spectrum->getSpectrumVis()->setWorkspaceIndex(index);
}

void MainSpectrumGUI::closeEvent(QCloseEvent *event)
{
    qDebug("MainSpectrumGUI::closeEvent");
    emit closing();
    event->accept();
}

void MainSpectrumGUI::mousePressEvent(QMouseEvent* event)
{
    if ((event->button() == Qt::LeftButton) && isOnMovingPad())
    {
        m_drag = true;
        m_DragPosition = event->globalPos() - pos();
        event->accept();
    }
    else
    {
        m_resizer.mousePressEvent(event);
    }
}

void MainSpectrumGUI::mouseReleaseEvent(QMouseEvent* event)
{
    m_resizer.mouseReleaseEvent(event);
}

void MainSpectrumGUI::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && isOnMovingPad())
    {
        move(event->globalPos() - m_DragPosition);
        event->accept();
    }
    else
    {
        m_resizer.mouseMoveEvent(event);
    }
}

void MainSpectrumGUI::leaveEvent(QEvent* event)
{
    m_resizer.leaveEvent(event);
    QMdiSubWindow::leaveEvent(event);
}

void MainSpectrumGUI::showHelp()
{
    if (m_helpURL.isEmpty()) {
        return;
    }

    QString url;

    if (m_helpURL.startsWith("http")) {
        url = m_helpURL;
    } else {
        url = QString("https://github.com/f4exb/sdrangel/blob/master/%1").arg(m_helpURL); // Something like "plugins/channelrx/chanalyzer/readme.md"
    }

    QDesktopServices::openUrl(QUrl(url));
}

void MainSpectrumGUI::openMoveToWorkspaceDialog()
{
    int numberOfWorkspaces = MainWindow::getInstance()->getNumberOfWorkspaces();
    WorkspaceSelectionDialog dialog(numberOfWorkspaces, this);
    dialog.exec();

    if (dialog.hasChanged()) {
        emit moveToWorkspace(dialog.getSelectedIndex());
    }
}

void MainSpectrumGUI::maximizeWindow()
{
    showMaximized();
}

void MainSpectrumGUI::shrinkWindow()
{
    qDebug("MainSpectrumGUI::shrinkWindow");
    if (isMaximized())
    {
        showNormal();
    }
    else
    {
        adjustSize();
        resize(width(), m_MinimumHeight);
    }
}

void MainSpectrumGUI::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

QString MainSpectrumGUI::getTitle() const
{
    return m_titleLabel->text();
}

bool MainSpectrumGUI::isOnMovingPad()
{
    return m_indexLabel->underMouse() ||
        m_spacerLabel->underMouse() ||
        m_titleLabel->underMouse() ||
        m_statusLabel->underMouse();
}

void MainSpectrumGUI::setIndex(int index)
{
    m_deviceSetIndex = index;
    m_indexLabel->setText(tr("%1:%2").arg(getDeviceTypeTag()).arg(m_deviceSetIndex));
}

void MainSpectrumGUI::setDeviceType(DeviceType type)
{
    m_deviceType = type;
    m_indexLabel->setStyleSheet(tr("QLabel { background-color: %1; qproperty-alignment: AlignCenter; }").arg(getDeviceTypeColor()));
}

void MainSpectrumGUI::setToolTip(const QString& tooltip)
{
    m_titleLabel->setToolTip(tooltip);
}

QString MainSpectrumGUI::getDeviceTypeColor()
{
    switch(m_deviceType)
    {
        case DeviceRx:
            return "rgb(0, 128, 0)";
        case DeviceTx:
            return "rgb(204, 0, 0)";
        case DeviceMIMO:
            return "rgb(0, 0, 192)";
        default:
            return "rgb(128, 128, 128)";
    }
}

QString MainSpectrumGUI::getDeviceTypeTag()
{
    switch(m_deviceType)
    {
        case DeviceRx:
            return "R";
        case DeviceTx:
            return "T";
        case DeviceMIMO:
            return "M";
        default:
            return "X";
    }
}

// Handle request from GLSpectrum/GLSpectrumGUI to adjust center frequency
void MainSpectrumGUI::onRequestCenterFrequency(qint64 frequency)
{
    emit requestCenterFrequency(m_deviceSetIndex, frequency);
}
