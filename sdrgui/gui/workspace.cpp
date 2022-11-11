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

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QFrame>
#include <QDebug>
#include <QApplication>

#include "gui/samplingdevicedialog.h"
#include "gui/rollupcontents.h"
#include "gui/buttonswitch.h"
#include "channel/channelgui.h"
#include "feature/featuregui.h"
#include "device/devicegui.h"
#include "device/deviceset.h"
#include "mainspectrum/mainspectrumgui.h"
#include "workspace.h"
#include "maincore.h"

Workspace::Workspace(int index, QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags),
    m_index(index),
    m_featureAddDialog(this),
    m_stacking(false),
    m_userChannelMinWidth(0)
{
    m_mdi = new QMdiArea(this);
    m_mdi->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdi->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setWidget(m_mdi);

    setWindowTitle(tr("W%1").arg(m_index));
    setObjectName(tr("W%1").arg(m_index));

    m_titleBar = new QWidget();
    m_titleBarLayout = new QHBoxLayout();
    m_titleBarLayout->setContentsMargins(QMargins());
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

    m_startStopButton = new ButtonSwitch();
    m_startStopButton->setCheckable(true);
    updateStartStopButton(false);
    m_startStopButton->setFixedSize(20, 20);

    m_vline1 = new QFrame();
    m_vline1->setFrameShape(QFrame::VLine);
    m_vline1->setFrameShadow(QFrame::Sunken);

    m_addFeatureButton = new QPushButton();
    QIcon addFeatureIcon(":/tool_add.png");
    m_addFeatureButton->setIcon(addFeatureIcon);
    m_addFeatureButton->setToolTip("Add features");
    m_addFeatureButton->setFixedSize(20, 20);

    m_featurePresetsButton = new QPushButton();
    QIcon presetsIcon(":/tool_star.png");
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

    m_stackSubWindows = new QPushButton("S");
    //QIcon stackSubWindowsIcon(":/stack.png");     // FIXME
    //m_stackSubWindows->setIcon(stackSubWindowsIcon);
    m_stackSubWindows->setToolTip("Stack sub windows");
    m_stackSubWindows->setFixedSize(20, 20);

    m_autoStackSubWindows = new ButtonSwitch();
    m_autoStackSubWindows->setText("AS");
    m_autoStackSubWindows->setCheckable(true);
    //QIcon autoStackSubWindowsIcon(":/autostack.png"); // FIXME
    //m_autoStackSubWindows->setIcon(autoStackSubWindowsIcon);
    m_autoStackSubWindows->setToolTip("Automatically stack sub windows");
    m_autoStackSubWindows->setFixedSize(20, 20);

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
    m_titleBarLayout->addWidget(m_startStopButton);
    m_titleBarLayout->addWidget(m_vline1);
    m_titleBarLayout->addWidget(m_addFeatureButton);
    m_titleBarLayout->addWidget(m_featurePresetsButton);
    m_titleBarLayout->addWidget(m_vline2);
    m_titleBarLayout->addWidget(m_cascadeSubWindows);
    m_titleBarLayout->addWidget(m_tileSubWindows);
    m_titleBarLayout->addWidget(m_stackSubWindows);
    m_titleBarLayout->addWidget(m_autoStackSubWindows);
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
        m_stackSubWindows,
        &QPushButton::clicked,
        this,
        &Workspace::stackSubWindows
    );

    QObject::connect(
        m_startStopButton,
        &ButtonSwitch::clicked,
        this,
        &Workspace::startStopClicked
    );

    QObject::connect(
        m_autoStackSubWindows,
        &QPushButton::clicked,
        this,
        &Workspace::autoStackSubWindows
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

    QObject::connect(
        MainCore::instance(),
        &MainCore::deviceStateChanged,
        this,
        &Workspace::deviceStateChanged
    );

}

Workspace::~Workspace()
{
    qDebug("Workspace::~Workspace");
    delete m_closeButton;
    delete m_normalButton;
    delete m_autoStackSubWindows;
    delete m_stackSubWindows;
    delete m_tileSubWindows;
    delete m_cascadeSubWindows;
    delete m_vline2;
    delete m_vline1;
    delete m_startStopButton;
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

void Workspace::setIndex(int index)
{
    m_index = index;
    setWindowTitle(tr("W%1").arg(m_index));
    setObjectName(tr("W%1").arg(m_index));
    m_titleLabel->setText(windowTitle());
}

QList<QMdiSubWindow *> Workspace::getSubWindowList() const
{
    return m_mdi->subWindowList();
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
    m_autoStackSubWindows->setChecked(false);
    m_mdi->cascadeSubWindows();
}

void Workspace::tileSubWindows()
{
    m_autoStackSubWindows->setChecked(false);
    m_mdi->tileSubWindows();
}

void Workspace::orderByIndex(QList<ChannelGUI *> &list)
{
    std::sort(list.begin(), list.end(),
        [](const ChannelGUI *a, const ChannelGUI *b) -> bool
        {
            if (a->getDeviceSetIndex() == b->getDeviceSetIndex()) {
                return a->getIndex() < b->getIndex();
            } else {
                return a->getDeviceSetIndex() < b->getDeviceSetIndex();
            }
        });
}

void Workspace::orderByIndex(QList<FeatureGUI *> &list)
{
    std::sort(list.begin(), list.end(),
        [](const FeatureGUI *a, const FeatureGUI *b) -> bool
        {
            return a->getIndex() < b->getIndex();
        });
}

void Workspace::orderByIndex(QList<DeviceGUI *> &list)
{
    std::sort(list.begin(), list.end(),
        [](const DeviceGUI *a, const DeviceGUI *b) -> bool
        {
            return a->getIndex() < b->getIndex();
        });
}

void Workspace::orderByIndex(QList<MainSpectrumGUI *> &list)
{
    std::sort(list.begin(), list.end(),
        [](const MainSpectrumGUI *a, const MainSpectrumGUI *b) -> bool
        {
            return a->getIndex() < b->getIndex();
        });
}

// Try to arrange windows somewhat like in earlier versions of SDRangel
// Devices and fixed size features stacked on left
// Spectrum and expandable features stacked in centre
// Channels stacked on right
void Workspace::stackSubWindows()
{
    // Set a flag so event handler knows if it's this code or the user that
    // resizes a window
    m_stacking = true;

    // Categorise windows according to type
    QList<QMdiSubWindow *> windows = m_mdi->subWindowList(QMdiArea::CreationOrder);
    QList<DeviceGUI *> devices;
    QList<MainSpectrumGUI *> spectrums;
    QList<ChannelGUI *> channels;
    QList<FeatureGUI *> fixedFeatures;
    QList<FeatureGUI *> features;

    for (auto window : windows)
    {
        if (window->isVisible() && !window->isMaximized())
        {
            if (window->inherits("DeviceGUI")) {
                devices.append(qobject_cast<DeviceGUI *>(window));
            } else if (window->inherits("MainSpectrumGUI")) {
                spectrums.append(qobject_cast<MainSpectrumGUI *>(window));
            } else if (window->inherits("ChannelGUI")) {
                channels.append(qobject_cast<ChannelGUI *>(window));
            } else if (window->inherits("FeatureGUI")) {
                if (window->sizePolicy().verticalPolicy() == QSizePolicy::Fixed) {  // Test vertical, as horizontal can be adjusted a little bit
                    fixedFeatures.append(qobject_cast<FeatureGUI *>(window));
                } else {
                    features.append(qobject_cast<FeatureGUI *>(window));
                }
            }
        }
    }

    // Order windows by device/feature/channel index
    orderByIndex(devices);
    orderByIndex(spectrums);
    orderByIndex(channels);
    orderByIndex(fixedFeatures);
    orderByIndex(features);

    // Spacing between windows
    const int spacing = 2;

    // Shrink devices to minimum size, in case they have been maximized
    for (auto window : devices)
    {
        QSize size = window->minimumSizeHint();
        size = size.expandedTo(window->minimumSize());
        window->resize(size);
    }

    // Calculate width and height needed for devices
    int deviceMinWidth = 0;
    int deviceTotalMinHeight = 0;
    for (auto window : devices)
    {
        int winMinWidth = std::max(window->minimumSizeHint().width(), window->minimumWidth());
        deviceMinWidth = std::max(deviceMinWidth, winMinWidth);
        deviceTotalMinHeight += window->minimumSizeHint().height() + spacing;
    }

    // Calculate width & height needed for spectrums
    int spectrumMinWidth = 0;
    int spectrumTotalMinHeight = 0;
    int expandingSpectrums = 0;
    for (auto window : spectrums)
    {
        int winMinWidth = std::max(window->minimumSizeHint().width(), window->minimumWidth());
        spectrumMinWidth = std::max(spectrumMinWidth, winMinWidth);
        int winMinHeight = std::max(window->minimumSizeHint().height(), window->minimumSize().height());
        spectrumTotalMinHeight += winMinHeight + spacing;
        expandingSpectrums++;
    }

    // Restrict user defined channel width, to width of largest channel
    if (channels.size() == 0)
    {
        m_userChannelMinWidth = 0;
    }
    else
    {
        int channelMaxWidth = 0;
        for (auto window : channels) {
            channelMaxWidth = std::max(channelMaxWidth, window->maximumWidth());
        }
        m_userChannelMinWidth = std::min(m_userChannelMinWidth, channelMaxWidth);
    }

    // Calculate width & height needed for channels
    int channelMinWidth = m_userChannelMinWidth; // channels.size() > 0 ? m_userChannelMinWidth : 0;
    int channelTotalMinHeight = 0;
    int expandingChannels = 0;
    for (auto window : channels)
    {
        int winMinWidth = std::max(window->minimumSizeHint().width(), window->minimumWidth());
        channelMinWidth = std::max(channelMinWidth, winMinWidth);
        channelTotalMinHeight += window->minimumSizeHint().height() + spacing;
        if (window->sizePolicy().verticalPolicy() == QSizePolicy::Expanding) {
            expandingChannels++;
        }
    }

    // Calculate width & height needed for features
    // These are spilt in to two groups - fixed size and expandable
    int fixedFeaturesWidth = 0;
    int fixedFeaturesTotalMinHeight = 0;
    int featuresMinWidth = 0;
    int featuresTotalMinHeight = 0;
    int expandingFeatures = 0;
    for (auto window : fixedFeatures)
    {
        int winMinWidth = std::max(window->minimumSizeHint().width(), window->minimumWidth());
        fixedFeaturesWidth = std::max(fixedFeaturesWidth, winMinWidth);
        fixedFeaturesTotalMinHeight += window->minimumSizeHint().height() + spacing;
    }
    for (auto window : features)
    {
        int winMinWidth = std::max(window->minimumSizeHint().width(), window->minimumWidth());
        featuresMinWidth = std::max(featuresMinWidth, winMinWidth);
        featuresTotalMinHeight += window->minimumSizeHint().height() + spacing;
        expandingFeatures++;
    }

    // Calculate width for left hand column
    int devicesFeaturesWidth = std::max(deviceMinWidth, fixedFeaturesWidth);
    // Calculate min width for centre column
    int spectrumFeaturesMinWidth = std::max(spectrumMinWidth, featuresMinWidth);

    // Calculate spacing between columns
    int spacing1 = devicesFeaturesWidth > 0 ? spacing : 0;
    int spacing2 = spectrumFeaturesMinWidth > 0 ? spacing : 0;

    // Will we need scroll bars?
    QSize mdiSize = m_mdi->size();
    int minWidth = devicesFeaturesWidth + spacing1 + spectrumFeaturesMinWidth + spacing2 + channelMinWidth;
    int minHeight = std::max(std::max(deviceTotalMinHeight + fixedFeaturesTotalMinHeight, channelTotalMinHeight), spectrumTotalMinHeight + featuresTotalMinHeight);

    bool requiresHScrollBar = minWidth > mdiSize.width();
    bool requiresVScrollBar = minHeight > mdiSize.height();

    // Reduce available size if scroll bars needed
    int sbWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    if (requiresVScrollBar) {
        mdiSize.setWidth(mdiSize.width() - sbWidth);
    }
    if (requiresHScrollBar) {
        mdiSize.setHeight(mdiSize.height() - sbWidth);
    }

    // If no spectrum/features, expand channels
    if ((spectrumFeaturesMinWidth == 0) && expandingChannels > 0) {
        channelMinWidth = mdiSize.width() - devicesFeaturesWidth - spacing1;
    }

    // Now position the windows
    int x = 0;
    int y = 0;

    // Put devices down left hand side
    for (auto window : devices)
    {
        window->move(x, y);
        y += window->size().height() + spacing;
    }

    // Put fixed height features underneath devices
    // Resize them to be same width
    for (auto window : fixedFeatures)
    {
        window->move(x, y);
        window->resize(devicesFeaturesWidth, window->size().height());
        y += window->size().height() + spacing;
    }

    // Calculate width needed for spectrum and features in the centre - use all available space
    int spectrumFeaturesWidth = std::max(mdiSize.width() - channelMinWidth - devicesFeaturesWidth - spacing1 - spacing2, spectrumFeaturesMinWidth);

    // Put channels on right hand side
    // Try to resize them horizontally so they are the same width
    // Share any available vertical space between expanding channels

    x = devicesFeaturesWidth + spacing1 + spectrumFeaturesWidth + spacing2;
    y = 0;
    int extraSpacePerWindow;
    int extraSpaceFirstWindow;
    if ((channelTotalMinHeight < mdiSize.height()) && (expandingChannels > 0))
    {
        extraSpacePerWindow = (mdiSize.height() - channelTotalMinHeight) / expandingChannels;
        extraSpaceFirstWindow = (mdiSize.height() - channelTotalMinHeight) % expandingChannels;
    }
    else
    {
        extraSpacePerWindow = 0;
        extraSpaceFirstWindow = 0;
    }

    for (auto window : channels)
    {
        window->move(x, y);
        int channelHeight = window->minimumSizeHint().height();
        if (window->sizePolicy().verticalPolicy() == QSizePolicy::Expanding)
        {
            channelHeight += extraSpacePerWindow + extraSpaceFirstWindow;
            extraSpaceFirstWindow = 0;
        }
        window->resize(channelMinWidth, channelHeight);
        y += window->size().height() + spacing;
    }

    // Split remaining space in the middle between spectrums and expandable features, with spectrums stacked on top
    x = devicesFeaturesWidth + spacing1;
    y = 0;
    if ((spectrumTotalMinHeight + featuresTotalMinHeight < mdiSize.height()) && (expandingSpectrums + expandingFeatures > 0))
    {
        int h = mdiSize.height() - spectrumTotalMinHeight - featuresTotalMinHeight;
        int f = expandingSpectrums + expandingFeatures;
        extraSpacePerWindow = h / f;
        extraSpaceFirstWindow = h % f;
    }
    else
    {
        extraSpacePerWindow = 0;
        extraSpaceFirstWindow = 0;
    }

    for (auto window : spectrums)
    {
        window->move(x, y);
        int w = spectrumFeaturesWidth;
        int minHeight = std::max(window->minimumSizeHint().height(), window->minimumSize().height());
        int h = minHeight + extraSpacePerWindow + extraSpaceFirstWindow;
        window->resize(w, h);
        extraSpaceFirstWindow = 0;
        y += window->size().height() + spacing;
    }
    for (auto window : features)
    {
        window->move(x, y);
        int w = spectrumFeaturesWidth;
        int h = window->minimumSizeHint().height() + extraSpacePerWindow + extraSpaceFirstWindow;
        window->resize(w, h);
        extraSpaceFirstWindow = 0;
        y += window->size().height() + spacing;
    }

    m_stacking = false;
}

void Workspace::autoStackSubWindows()
{
    if (m_autoStackSubWindows->isChecked()) {
        stackSubWindows();
    }
}

// Start/stop all devices in workspace
void Workspace::startStopClicked(bool checked)
{
    if (!checked) {
        emit stopAllDevices(this);
    } else {
        emit startAllDevices(this);
    }
    updateStartStopButton(checked);
}

void Workspace::updateStartStopButton(bool checked)
{
    if (!checked)
    {
        QIcon startIcon(":/play.png");
        m_startStopButton->setIcon(startIcon);
        m_startStopButton->setStyleSheet("QToolButton { background-color : blue; }");
        m_startStopButton->setToolTip("Start all devices in workspace");
    }
    else
    {
        QIcon stopIcon(":/stop.png");
        m_startStopButton->setIcon(stopIcon);
        m_startStopButton->setStyleSheet("QToolButton { background-color : green; }");
        m_startStopButton->setToolTip("Stop all devices in workspace");
    }
}

void Workspace::deviceStateChanged(int index, DeviceAPI *deviceAPI)
{
    if (deviceAPI->getWorkspaceIndex() == m_index)
    {
        // Check state of all devices in workspace, to see if any are running or have errors
        bool running = false;
        bool error = false;
        std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
        for (auto deviceSet : deviceSets)
        {
            DeviceAPI::EngineState state = deviceSet->m_deviceAPI->state();
            if (state == DeviceAPI::StRunning) {
                running = true;
            } else if (state == DeviceAPI::StError) {
                error = true;
            }
        }
        // Update start/stop button to reflect current state of devices
        updateStartStopButton(running);
        m_startStopButton->setChecked(running);
        if (error) {
            m_startStopButton->setStyleSheet("QToolButton { background-color : red; }");
        }
    }
}

void Workspace::resizeEvent(QResizeEvent *event)
{
    QDockWidget::resizeEvent(event);
    autoStackSubWindows();
}

void Workspace::addToMdiArea(QMdiSubWindow *sub)
{
    // Add event handler to auto-stack when sub window shown or hidden
    sub->installEventFilter(this);
    // Can't use Close event, as it's before window is closed, so
    // catch sub-window destroyed signal instead
    connect(sub, &QObject::destroyed, this, &Workspace::autoStackSubWindows);
    m_mdi->addSubWindow(sub);
    sub->show();
    // Auto-stack when sub-window's  widgets are rolled up
    ChannelGUI *channel = qobject_cast<ChannelGUI *>(sub);
    if (channel) {
        connect(channel->getRollupContents(), &RollupContents::widgetRolled, this, &Workspace::autoStackSubWindows);
    }
    FeatureGUI *feature = qobject_cast<FeatureGUI *>(sub);
    if (feature) {
        connect(feature->getRollupContents(), &RollupContents::widgetRolled, this, &Workspace::autoStackSubWindows);
    }
}

void Workspace::removeFromMdiArea(QMdiSubWindow *sub)
{
    m_mdi->removeSubWindow(sub);
    sub->removeEventFilter(this);
    disconnect(sub, &QObject::destroyed, this, &Workspace::autoStackSubWindows);
    ChannelGUI *channel = qobject_cast<ChannelGUI *>(sub);
    if (channel) {
        disconnect(channel->getRollupContents(), &RollupContents::widgetRolled, this, &Workspace::autoStackSubWindows);
    }
    FeatureGUI *feature = qobject_cast<FeatureGUI *>(sub);
    if (feature) {
        disconnect(feature->getRollupContents(), &RollupContents::widgetRolled, this, &Workspace::autoStackSubWindows);
    }
}

bool Workspace::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Show)
    {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (!widget->isMaximized()) {
            autoStackSubWindows();
        }
    }
    else if (event->type() == QEvent::Hide)
    {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (!widget->isMaximized()) {
            autoStackSubWindows();
        }
    }
    else if (event->type() == QEvent::Resize)
    {
        if (!m_stacking && m_autoStackSubWindows->isChecked())
        {
            QWidget *widget = qobject_cast<QWidget *>(obj);
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            ChannelGUI *channel = qobject_cast<ChannelGUI *>(obj);
            if (channel && !widget->isMaximized())
            {
                // When maximizing, we can get resize event where isMaximized is false, even though it should be true,
                // but we can tell as window size matches mdi size
                if (m_mdi->size() != resizeEvent->size())
                {
                    // Allow width of channels column to be set by user when they
                    // resize a channel window
                    m_userChannelMinWidth = resizeEvent->size().width();
                    stackSubWindows();
                }
            }
        }
    }
    return QDockWidget::eventFilter(obj, event);
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
    m_mdi->restoreGeometry(qUncompress(blob));
}

bool Workspace::getAutoStackOption() const
{
    return m_autoStackSubWindows->isChecked();
}

void Workspace::setAutoStackOption(bool autoStack)
{
    m_autoStackSubWindows->doToggle(autoStack);
}

void Workspace::adjustSubWindowsAfterRestore()
{
    QList<QMdiSubWindow *> subWindowList = m_mdi->subWindowList();

    for (auto& subWindow : subWindowList)
    {
        if ((subWindow->y() >= 20) && (subWindow->y() < 40)) {
            subWindow->move(subWindow->x(), subWindow->y() - 20);
        }

        if (qobject_cast<ChannelGUI*>(subWindow)) {
            subWindow->resize(subWindow->width(), subWindow->height() - 22);
        }

        if (qobject_cast<FeatureGUI*>(subWindow)) {
            subWindow->resize(subWindow->width(), subWindow->height() - 8);
        }
    }
}
