///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QStyle>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizeGrip>
#include <QTextEdit>
#include <QObjectCleanupHandler>
#include <QDesktopServices>

#include "mainwindow.h"
#include "gui/workspaceselectiondialog.h"
#include "gui/samplingdevicedialog.h"
#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "devicegui.h"

DeviceGUI::DeviceGUI(QWidget *parent) :
    QMdiSubWindow(parent),
    m_deviceUISet(nullptr),
    m_deviceType(DeviceRx),
    m_deviceSetIndex(0),
    m_contextMenuType(ContextMenuNone),
    m_resizer(this),
    m_drag(false),
    m_currentDeviceIndex(-1),
    m_channelAddDialog(this)
{
    qDebug("DeviceGUI::DeviceGUI: %p", parent);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setObjectName("DeviceGUI");
    setStyleSheet(QString(tr("#DeviceGUI { border: 1px solid %1; background-color: %2; }")
        .arg(palette().highlight().color().darker(115).name()))
        .arg(palette().dark().color().darker(115).name()));

    m_indexLabel = new QLabel();
    m_indexLabel->setFixedSize(32, 16);
    m_indexLabel->setStyleSheet("QLabel { background-color: rgb(128, 128, 128); qproperty-alignment: AlignCenter; }");
    m_indexLabel->setText(tr("X:%1").arg(m_deviceSetIndex));
    m_indexLabel->setToolTip("Device type and set index");

    m_settingsButton = new QPushButton();
    m_settingsButton->setFixedSize(20, 20);
    QIcon settingsIcon(":/gear.png");
    m_settingsButton->setIcon(settingsIcon);
    m_settingsButton->setToolTip("Common settings");

    m_changeDeviceButton = new QPushButton();
    m_changeDeviceButton->setFixedSize(20, 20);
    QIcon changeDeviceIcon(":/swap.png");
    m_changeDeviceButton->setIcon(changeDeviceIcon);
    m_changeDeviceButton->setToolTip("Change device");

    m_reloadDeviceButton = new QPushButton();
    m_reloadDeviceButton->setFixedSize(20, 20);
    QIcon reloadDeviceIcon(":/recycle.png");
    m_reloadDeviceButton->setIcon(reloadDeviceIcon);
    m_reloadDeviceButton->setToolTip("Reload device");

    m_addChannelsButton = new QPushButton();
    m_addChannelsButton->setFixedSize(20, 20);
    QIcon addChannelsIcon(":/channels_add.png");
    m_addChannelsButton->setIcon(addChannelsIcon);
    m_addChannelsButton->setToolTip("Add channels");

    m_deviceSetPresetsButton = new QPushButton();
    m_deviceSetPresetsButton->setFixedSize(20, 20);
    QIcon deviceSetPresetsIcon(":/star.png");
    m_deviceSetPresetsButton->setIcon(deviceSetPresetsIcon);
    m_deviceSetPresetsButton->setToolTip("Device set presets");

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
    m_helpButton->setToolTip("Show device documentation in browser");

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

    m_closeButton = new QPushButton();
    m_closeButton->setFixedSize(20, 20);
    QIcon closeIcon(":/cross.png");
    m_closeButton->setIcon(closeIcon);
    m_closeButton->setToolTip("Close device");

    m_statusLabel = new QLabel();
    // m_statusLabel->setText("OK"); // for future use
    m_statusLabel->setFixedHeight(20);
    m_statusLabel->setMinimumWidth(20);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    m_statusLabel->setToolTip("Device status");

    m_showSpectrumButton = new QPushButton();
    m_showSpectrumButton->setFixedSize(20, 20);
    QIcon showSpectrumIcon(":/dsb.png");
    m_showSpectrumButton->setIcon(showSpectrumIcon);
    m_showSpectrumButton->setToolTip("Show main spectrum");

    m_showAllChannelsButton = new QPushButton();
    m_showAllChannelsButton->setFixedSize(20, 20);
    QIcon showAllChannelsIcon(":/channels.png");
    m_showAllChannelsButton->setIcon(showAllChannelsIcon);
    m_showAllChannelsButton->setToolTip("Show all channels");

    m_layouts = new QVBoxLayout();
    m_layouts->setContentsMargins(m_resizer.m_gripSize, m_resizer.m_gripSize, m_resizer.m_gripSize, m_resizer.m_gripSize);
    m_layouts->setSpacing(0);

    m_topLayout = new QHBoxLayout();
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->addWidget(m_indexLabel);
    m_topLayout->addWidget(m_settingsButton);
    m_topLayout->addWidget(m_changeDeviceButton);
    m_topLayout->addWidget(m_reloadDeviceButton);
    m_topLayout->addWidget(m_deviceSetPresetsButton);
    m_topLayout->addWidget(m_addChannelsButton);
    m_topLayout->addWidget(m_titleLabel);
    // m_topLayout->addStretch(1);
    m_topLayout->addWidget(m_helpButton);
    m_topLayout->addWidget(m_moveButton);
    m_topLayout->addWidget(m_shrinkButton);
    m_topLayout->addWidget(m_maximizeButton);
    m_topLayout->addWidget(m_closeButton);

    m_centerLayout = new QVBoxLayout();
    m_centerLayout->setContentsMargins(0, 0, 0, 0);
    m_contents = new QWidget(); // Do not delete! Done in child's destructor with "delete ui"
    m_centerLayout->addWidget(m_contents);

    m_bottomLayout = new QHBoxLayout();
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->addWidget(m_showSpectrumButton);
    m_bottomLayout->addWidget(m_showAllChannelsButton);
    m_bottomLayout->addWidget(m_statusLabel);
    m_sizeGripBottomRight = new QSizeGrip(this);
    m_sizeGripBottomRight->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // m_bottomLayout->addStretch(1);
    m_bottomLayout->addWidget(m_sizeGripBottomRight, 0, Qt::AlignBottom | Qt::AlignRight);

    m_layouts->addLayout(m_topLayout);
    m_layouts->addLayout(m_centerLayout);
    m_layouts->addLayout(m_bottomLayout);

    QObjectCleanupHandler().add(layout());
    setLayout(m_layouts);

    connect(m_settingsButton, SIGNAL(clicked()), this, SLOT(activateSettingsDialog()));
    connect(m_changeDeviceButton, SIGNAL(clicked()), this, SLOT(openChangeDeviceDialog()));
    connect(m_reloadDeviceButton, SIGNAL(clicked()), this, SLOT(deviceReload()));
    connect(m_addChannelsButton, SIGNAL(clicked()), this, SLOT(openAddChannelsDialog()));
    connect(m_deviceSetPresetsButton, SIGNAL(clicked()), this, SLOT(deviceSetPresetsDialog()));
    connect(m_helpButton, SIGNAL(clicked()), this, SLOT(showHelp()));
    connect(m_moveButton, SIGNAL(clicked()), this, SLOT(openMoveToWorkspaceDialog()));
    connect(m_shrinkButton, SIGNAL(clicked()), this, SLOT(shrinkWindow()));
    connect(m_maximizeButton, SIGNAL(clicked()), this, SLOT(maximizeWindow()));
    connect(this, SIGNAL(forceShrink()), this, SLOT(shrinkWindow()));
    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(m_showSpectrumButton, SIGNAL(clicked()), this, SLOT(showSpectrumHandler()));
    connect(m_showAllChannelsButton, SIGNAL(clicked()), this, SLOT(showAllChannelsHandler()));

    QObject::connect(
        &m_channelAddDialog,
        &ChannelAddDialog::addChannel,
        this,
        &DeviceGUI::addChannelEmitted
    );
}

DeviceGUI::~DeviceGUI()
{
    qDebug("DeviceGUI::~DeviceGUI");
    delete m_sizeGripBottomRight;
    delete m_bottomLayout;
    delete m_centerLayout;
    delete m_topLayout;
    delete m_layouts;
    delete m_showAllChannelsButton;
    delete m_showSpectrumButton;
    delete m_statusLabel;
    delete m_closeButton;
    delete m_shrinkButton;
    delete m_maximizeButton;
    delete m_moveButton;
    delete m_helpButton;
    delete m_titleLabel;
    delete m_deviceSetPresetsButton;
    delete m_addChannelsButton;
    delete m_reloadDeviceButton;
    delete m_changeDeviceButton;
    delete m_settingsButton;
    delete m_indexLabel;
    qDebug("DeviceGUI::~DeviceGUI: end");
}

// Size the window according to the size of the m_contents widget
// This allows the window min/max size and size policy to be set by the .ui file
void DeviceGUI::sizeToContents()
{
    // Set window size policy to the size policy of m_contents widget
    QSizePolicy policy = getContents()->sizePolicy();
    setSizePolicy(policy);

    // If size policy is fixed, hide widgets that resize the window
    if ((policy.verticalPolicy() == QSizePolicy::Fixed) && (policy.horizontalPolicy() == QSizePolicy::Fixed))
    {
        m_shrinkButton->hide();
        m_maximizeButton->hide();
        // The resize grip can magically reappear after being maximized, so delete it, to prevent this
        delete m_sizeGripBottomRight;
        m_sizeGripBottomRight = nullptr;
    }

    // Calculate min/max size for window. This is min/max size of contents, plus
    // extra needed for window frame and title bar
    QSize size;
    size = getContents()->maximumSize();
    size.setHeight(std::min(size.height() + getAdditionalHeight(), QWIDGETSIZE_MAX));
    size.setWidth(std::min(size.width() + m_resizer.m_gripSize * 2, QWIDGETSIZE_MAX));
    setMaximumSize(size);
    size = getContents()->minimumSize();
    size.setHeight(std::min(size.height() + getAdditionalHeight(), QWIDGETSIZE_MAX));
    size.setWidth(std::min(size.width() + m_resizer.m_gripSize * 2, QWIDGETSIZE_MAX));
    setMinimumSize(size);

    // Adjust to minimum size needed for widgets
    adjustSize();
}

void DeviceGUI::setWorkspaceIndex(int index)
{
    m_workspaceIndex = index;

    if (m_deviceUISet) {
        m_deviceUISet->m_deviceAPI->setWorkspaceIndex(index);
    }
}

void DeviceGUI::closeEvent(QCloseEvent *event)
{
    qDebug("DeviceGUI::closeEvent");
    emit closing();
    event->ignore(); // Don't automatically delete the GUI -  MainWindow::RemoveDeviceSetFSM::removeUI will do it
}

void DeviceGUI::mousePressEvent(QMouseEvent* event)
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

void DeviceGUI::mouseReleaseEvent(QMouseEvent* event)
{
    m_resizer.mouseReleaseEvent(event);
}

void DeviceGUI::mouseMoveEvent(QMouseEvent* event)
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

void DeviceGUI::leaveEvent(QEvent* event)
{
    m_resizer.leaveEvent(event);
    QMdiSubWindow::leaveEvent(event);
}

void DeviceGUI::openChangeDeviceDialog()
{
    SamplingDeviceDialog dialog((int) m_deviceType, this);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_currentDeviceIndex = dialog.getSelectedDeviceIndex();
        dialog.setParent(nullptr);
        emit deviceChange(m_currentDeviceIndex);
    }
}

void DeviceGUI::deviceReload()
{
    if (m_currentDeviceIndex >= 0) {
        emit deviceChange(m_currentDeviceIndex);
    }
}

void DeviceGUI::activateSettingsDialog()
{
    QPoint p = QCursor::pos();
    m_contextMenuType = ContextMenuDeviceSettings;
    emit customContextMenuRequested(p);
}

void DeviceGUI::showHelp()
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

void DeviceGUI::openMoveToWorkspaceDialog()
{
    int numberOfWorkspaces = MainWindow::getInstance()->getNumberOfWorkspaces();
    WorkspaceSelectionDialog dialog(numberOfWorkspaces, getWorkspaceIndex(), this);
    dialog.exec();

    if (dialog.hasChanged()) {
        emit moveToWorkspace(dialog.getSelectedIndex());
    }
}

void DeviceGUI::openAddChannelsDialog()
{
    //m_channelAddDialog.exec();
    m_channelAddDialog.open();
}

void DeviceGUI::showSpectrumHandler()
{
    emit showSpectrum(m_deviceSetIndex);
}

void DeviceGUI::showAllChannelsHandler()
{
    emit showAllChannels(m_deviceSetIndex);
}

void DeviceGUI::shrinkWindow()
{
    qDebug("DeviceGUI::shrinkWindow");
    showNormal(); // In case it had been maximized
    adjustSize();
}

void DeviceGUI::maximizeWindow()
{
    showMaximized();
}

void DeviceGUI::deviceSetPresetsDialog()
{
    QPoint p = mapFromGlobal(QCursor::pos());
    emit deviceSetPresetsDialogRequested(p, this);
}

void DeviceGUI::setTitle(const QString& title)
{
    setWindowTitle(title + " Device");
    m_titleLabel->setText(title);
}

QString DeviceGUI::getTitle() const
{
    return m_titleLabel->text();
}

bool DeviceGUI::isOnMovingPad()
{
    return m_indexLabel->underMouse() || m_titleLabel->underMouse() || m_statusLabel->underMouse();
}

void DeviceGUI::setIndex(int index)
{
    m_deviceSetIndex = index;
    m_indexLabel->setText(tr("%1:%2").arg(getDeviceTypeTag()).arg(m_deviceSetIndex));
}

void DeviceGUI::setDeviceType(DeviceType type)
{
    m_deviceType = type;
    m_indexLabel->setStyleSheet(tr("QLabel { background-color: %1; qproperty-alignment: AlignCenter; }").arg(getDeviceTypeColor()));
}

void DeviceGUI::setToolTip(const QString& tooltip)
{
    m_titleLabel->setToolTip(tooltip);
}

QString DeviceGUI::getDeviceTypeColor()
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

QString DeviceGUI::getDeviceTypeTag()
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
