///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/flowlayout.h"
#include "gui/scidoublespinbox.h"

#include "ui_remotecontrolgui.h"
#include "remotecontrol.h"
#include "remotecontrolgui.h"
#include "remotecontrolsettingsdialog.h"

RemoteControlGUI* RemoteControlGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    RemoteControlGUI* gui = new RemoteControlGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void RemoteControlGUI::destroy()
{
    delete this;
}

void RemoteControlGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RemoteControlGUI::serialize() const
{
    return m_settings.serialize();
}

bool RemoteControlGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(true);
        on_update_clicked();

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool RemoteControlGUI::handleMessage(const Message& message)
{
    if (RemoteControl::MsgConfigureRemoteControl::match(message))
    {
        qDebug("RemoteControlGUI::handleMessage: RemoteControl::MsgConfigureRemoteControl");
        const RemoteControl::MsgConfigureRemoteControl& cfg = (RemoteControl::MsgConfigureRemoteControl&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (RemoteControl::MsgDeviceStatus::match(message))
    {
        const RemoteControl::MsgDeviceStatus& msg = (RemoteControl::MsgDeviceStatus&) message;
        deviceUpdated(msg.getProtocol(), msg.getDeviceId(), msg.getStatus());
        return true;
    }
    else if (RemoteControl::MsgDeviceError::match(message))
    {
        const RemoteControl::MsgDeviceError& msg = (RemoteControl::MsgDeviceError&) message;
        QMessageBox::critical(this,  "Remote Control Error", msg.getErrorMessage());
        return true;
    }
    else if (RemoteControl::MsgDeviceUnavailable::match(message))
    {
        const RemoteControl::MsgDeviceUnavailable& msg = (RemoteControl::MsgDeviceUnavailable&) message;
        deviceUnavailable(msg.getProtocol(), msg.getDeviceId());
        return true;
    }

    return false;
}

void RemoteControlGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void RemoteControlGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

RemoteControlGUI::RemoteControlGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::RemoteControlGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/remotecontrol/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->startStop->setStyleSheet("QToolButton { background-color : blue; }"
                                 "QToolButton:checked { background-color : green; }"
                                 "QToolButton:disabled { background-color : gray; }");

    m_startStopIcon.addFile(":/play.png", QSize(16, 16), QIcon::Normal, QIcon::Off);
    m_startStopIcon.addFile(":/stop.png", QSize(16, 16), QIcon::Normal, QIcon::On);

    m_remoteControl = reinterpret_cast<RemoteControl*>(feature);
    m_remoteControl->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    applySettings(true);
    makeUIConnections();
}

RemoteControlGUI::~RemoteControlGUI()
{
    qDeleteAll(m_deviceGUIs);
    m_deviceGUIs.clear();
    delete ui;
}

void RemoteControlGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void RemoteControlGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteControlGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    createGUI();
    blockApplySettings(true);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void RemoteControlGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void RemoteControlGUI::createControls(RemoteControlDeviceGUI *gui, QBoxLayout *vBox, FlowLayout *flow, int &widgetCnt)
{
    // Create buttons to control the device
    QGridLayout *controlsGrid = nullptr;

    if (gui->m_rcDevice->m_verticalControls)
    {
        controlsGrid = new QGridLayout();
        vBox->addLayout(controlsGrid);
    }
    else if (!flow)
    {
        flow = new FlowLayout(2, 6, 6);
        vBox->addItem(flow);
    }

    int row = 0;
    for (auto const &control : gui->m_rcDevice->m_controls)
    {
        if (!gui->m_rcDevice->m_verticalControls && (widgetCnt > 0))
        {
            QFrame *line = new QFrame();
            line->setFrameShape(QFrame::VLine);
            line->setFrameShadow(QFrame::Sunken);
            flow->addWidget(line);
        }

        DeviceDiscoverer::ControlInfo *info = gui->m_rcDevice->m_info.getControl(control.m_id);
        if (!info)
        {
            qDebug() << "RemoteControlGUI::createControls: Info missing for " << control.m_id;
            continue;
        }

        if (!control.m_labelLeft.isEmpty())
        {
            QLabel *controlLabelLeft = new QLabel(control.m_labelLeft);
            if (gui->m_rcDevice->m_verticalControls)
            {
                controlsGrid->addWidget(controlLabelLeft, row, 0);
                controlsGrid->setColumnStretch(row, 0);
            }
            else
            {
                flow->addWidget(controlLabelLeft);
            }
        }

        QList<QWidget *> widgets;
        QWidget *widget = nullptr;

        switch (info->m_type)
        {
        case DeviceDiscoverer::BOOL:
            {
                ButtonSwitch *button = new ButtonSwitch();
                button->setToolTip("Start/stop " + info->m_name);
                button->setIcon(m_startStopIcon);
                button->setStyleSheet("QToolButton { background-color : blue; }"
                                      "QToolButton:checked { background-color : green; }"
                                      "QToolButton:disabled { background-color : gray; }");
                connect(button, &ButtonSwitch::toggled,
                    [=] (bool toggled)
                    {
                        RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                 gui->m_rcDevice->m_info.m_id,
                                                                                                 control.m_id,
                                                                                                 toggled);
                        m_remoteControl->getInputMessageQueue()->push(message);
                    }
                );
                widgets.append(button);
                widget = button;
            }
            break;

        case DeviceDiscoverer::INT:
            {
                QSpinBox *spinBox = new QSpinBox();

                spinBox->setToolTip("Set value for " + info->m_name);
                spinBox->setMinimum((int)info->m_min);
                spinBox->setMaximum((int)info->m_max);
                connect(spinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                    [=] (int value)
                    {
                        RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                 gui->m_rcDevice->m_info.m_id,
                                                                                                 control.m_id,
                                                                                                 value);
                        m_remoteControl->getInputMessageQueue()->push(message);
                    }
                );
                widgets.append(spinBox);
                widget = spinBox;
            }
            break;

        case DeviceDiscoverer::FLOAT:
            {
                switch (info->m_widgetType)
                {
                case DeviceDiscoverer::SPIN_BOX:
                    {
                        QDoubleSpinBox *spinBox = new SciDoubleSpinBox();

                        spinBox->setToolTip("Set value for " + info->m_name);
                        spinBox->setMinimum(info->m_min);
                        spinBox->setMaximum(info->m_max);
                        spinBox->setDecimals(info->m_precision);
                        connect(spinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                            [=] (double value)
                            {
                                RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                         gui->m_rcDevice->m_info.m_id,
                                                                                                         control.m_id,
                                                                                                         (float)value * info->m_scale);
                                m_remoteControl->getInputMessageQueue()->push(message);
                            }
                        );
                        widgets.append(spinBox);
                        widget = spinBox;
                    }
                    break;

                case DeviceDiscoverer::DIAL:
                    {
                        widget = new QWidget();
                        QHBoxLayout *layout = new QHBoxLayout();
                        layout->setContentsMargins(0, 0, 0, 0);
                        widget->setLayout(layout);

                        QDial *dial = new QDial();
                        dial->setMaximumSize(24, 24);
                        dial->setToolTip("Set value for " + info->m_name);
                        dial->setMinimum(info->m_min);
                        dial->setMaximum(info->m_max);

                        connect(dial, static_cast<void (QDial::*)(int)>(&QDial::valueChanged),
                            [=] (int value)
                            {
                                RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                         gui->m_rcDevice->m_info.m_id,
                                                                                                         control.m_id,
                                                                                                         ((float)value) * info->m_scale);
                                m_remoteControl->getInputMessageQueue()->push(message);
                            }
                        );
                        widgets.append(dial);
                        layout->addWidget(dial);

                        QLabel *label = new QLabel(QString::number(dial->value()));
                        label->setToolTip("Value for " + info->m_name);
                        widgets.append(label);
                        layout->addWidget(label);
                    }
                    break;

                case DeviceDiscoverer::SLIDER:
                    {
                        widget = new QWidget();
                        QHBoxLayout *layout = new QHBoxLayout();
                        layout->setContentsMargins(0, 0, 0, 0);
                        widget->setLayout(layout);

                        QSlider *slider = new QSlider(Qt::Horizontal);
                        slider->setToolTip("Set value for " + info->m_name);
                        slider->setMinimum(info->m_min);
                        slider->setMaximum(info->m_max);

                        connect(slider, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
                            [=] (int value)
                            {
                                RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                         gui->m_rcDevice->m_info.m_id,
                                                                                                         control.m_id,
                                                                                                         ((float)value) * info->m_scale);
                                m_remoteControl->getInputMessageQueue()->push(message);
                            }
                        );
                        widgets.append(slider);
                        layout->addWidget(slider);

                        QLabel *label = new QLabel(QString::number(slider->value()));
                        label->setToolTip("Value for " + info->m_name);
                        widgets.append(label);
                        layout->addWidget(label);
                    }
                    break;

                }
            }
            break;

        case DeviceDiscoverer::STRING:
            {
                QLineEdit *lineEdit = new QLineEdit();

                lineEdit->setToolTip("Set value for " + info->m_name);

                connect(lineEdit, &QLineEdit::editingFinished,
                    [=] ()
                    {
                        QString text = lineEdit->text();
                        RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                 gui->m_rcDevice->m_info.m_id,
                                                                                                 control.m_id,
                                                                                                 text);
                        m_remoteControl->getInputMessageQueue()->push(message);
                    }
                );
                widgets.append(lineEdit);
                widget = lineEdit;
            }
            break;

        case DeviceDiscoverer::LIST:
            {
                QComboBox *combo = new QComboBox();

                combo->setToolTip("Set value for " + info->m_name);
                combo->insertItems(0, info->m_values);
                connect(combo, &QComboBox::currentTextChanged,
                    [=] (const QString &text)
                    {
                        RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                 gui->m_rcDevice->m_info.m_id,
                                                                                                 control.m_id,
                                                                                                 text);
                        m_remoteControl->getInputMessageQueue()->push(message);
                    }
                );
                widgets.append(combo);
                widget = combo;
            }
            break;

        case DeviceDiscoverer::BUTTON:
            {
                QString label = info->m_name;
                if (info->m_values.size() > 0) {
                    label = info->m_values[0];
                }
                QToolButton *button = new QToolButton();
                button->setText(label);
                button->setToolTip("Trigger " + info->m_name);

                connect(button, &QToolButton::clicked,
                    [=] (bool checked)
                    {
                        (void) checked;
                        RemoteControl::MsgDeviceSetState *message = RemoteControl::MsgDeviceSetState::create(gui->m_rcDevice->m_protocol,
                                                                                                 gui->m_rcDevice->m_info.m_id,
                                                                                                 control.m_id,
                                                                                                 1);
                        m_remoteControl->getInputMessageQueue()->push(message);
                    }
                );
                widgets.append(button);
                widget = button;
            }
            break;

        default:
            qDebug() << "RemoteControlGUI::createControls: Unexpected type for control.";
            break;

        }
        gui->m_controls.insert(control.m_id, widgets);
        if (gui->m_rcDevice->m_verticalControls) {
            controlsGrid->addWidget(widget, row, 1);
        } else {
            flow->addWidget(widget);
        }

        if (!control.m_labelRight.isEmpty())
        {
            QLabel *controlLabelRight = new QLabel(control.m_labelRight);
            if (gui->m_rcDevice->m_verticalControls)
            {
                controlsGrid->addWidget(controlLabelRight, row, 2);
                controlsGrid->setColumnStretch(row, 2);
            }
            else
            {
                flow->addWidget(controlLabelRight);
            }
        }

        widgetCnt++;
        row++;
    }
}

void RemoteControlGUI::createChart(RemoteControlDeviceGUI *gui, QVBoxLayout *vBox, const QString &id, const QString &units)
{
    if (gui->m_chart == nullptr)
    {
        // Create a chart to plot the sensor data
        gui->m_chart = new QChart();
        gui->m_chart->setTitle("");
        gui->m_chart->legend()->hide();
        gui->m_chart->layout()->setContentsMargins(0, 0, 0, 0);
        gui->m_chart->setMargins(QMargins(1, 1, 1, 1));
        gui->m_chart->setTheme(QChart::ChartThemeDark);
        QLineSeries *series = new QLineSeries();
        gui->m_series.insert(id, series);
        QLineSeries *onePointSeries = new QLineSeries();
        gui->m_onePointSeries.insert(id, onePointSeries);
        gui->m_chart->addSeries(series);
        QValueAxis *yAxis = new QValueAxis();
        QDateTimeAxis *xAxis = new QDateTimeAxis();
        xAxis->setFormat(QString("hh:mm:ss"));
        yAxis->setTitleText(units);
        gui->m_chart->addAxis(xAxis, Qt::AlignBottom);
        gui->m_chart->addAxis(yAxis, Qt::AlignLeft);
        series->attachAxis(xAxis);
        series->attachAxis(yAxis);
        gui->m_chartView = new QChartView();
        gui->m_chartView->setChart(gui->m_chart);
        if (m_settings.m_chartHeightFixed)
        {
            gui->m_chartView->setMinimumSize(300, m_settings.m_chartHeightPixels);
            gui->m_chartView->setMaximumSize(16777215, m_settings.m_chartHeightPixels);
            gui->m_chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        else
        {
            gui->m_chartView->setMinimumSize(300, 130); // 130 is enough to display axis labels
            gui->m_chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            gui->m_chartView->setSceneRect(0, 0, 300, 130); // This determines m_chartView->sizeHint() - default is 640x480, which is a bit big
        }
        QBoxLayout *chartLayout = new QVBoxLayout();
        gui->m_chartView->setLayout(chartLayout);

        vBox->addWidget(gui->m_chartView);
    }
    else
    {
        // Add new series
        QLineSeries *series = new QLineSeries();
        gui->m_series.insert(id, series);
        QLineSeries *onePointSeries = new QLineSeries();
        gui->m_onePointSeries.insert(id, onePointSeries);
        gui->m_chart->addSeries(series);
        if (!gui->m_rcDevice->m_commonYAxis)
        {
            // Use per series Y axis
            QValueAxis *yAxis = new QValueAxis();
            yAxis->setTitleText(units);
            gui->m_chart->addAxis(yAxis, Qt::AlignRight);
            series->attachAxis(yAxis);
        }
        else
        {
            // Use common y axis
            QAbstractAxis *yAxis = gui->m_chart->axes(Qt::Vertical)[0];
            // Only display units if all the same
            if (yAxis->titleText() != units) {
                yAxis->setTitleText("");
            }
            series->attachAxis(yAxis);
        }
        series->attachAxis(gui->m_chart->axes(Qt::Horizontal)[0]);
    }
}

void RemoteControlGUI::createSensors(RemoteControlDeviceGUI *gui, QVBoxLayout *vBox, FlowLayout *flow, int &widgetCnt, bool &hasCharts)
{
    // Table doesn't seem to expand in a QHBoxLayout, so we have to use a GridLayout
    QGridLayout *grid = nullptr;
    QTableWidget *table = nullptr;
    if (gui->m_rcDevice->m_verticalSensors)
    {
        grid = new QGridLayout();
        grid->setColumnStretch(0, 1);
        vBox->addLayout(grid);
        table = new QTableWidget(gui->m_rcDevice->m_sensors.size(), 3);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);  // Needed so table->sizeHint matches minimumSize set below
    }
    else if (!flow)
    {
        flow = new FlowLayout(2, 6, 6);
        vBox->addItem(flow);
    }

    int row = 0;
    bool hasUnits = false;
    for (auto const &sensor : gui->m_rcDevice->m_sensors)
    {
        // For vertical layout, we use a table
        // For horizontal, we use HBox of labels separated with bars
        if (gui->m_rcDevice->m_verticalSensors)
        {
            if (!sensor.m_labelLeft.isEmpty())
            {
                QTableWidgetItem *sensorLabel = new QTableWidgetItem(sensor.m_labelLeft);
                sensorLabel->setFlags(Qt::ItemIsEnabled);
                table->setItem(row, COL_LABEL, sensorLabel);
            }
            QTableWidgetItem *valueItem = new QTableWidgetItem("-");
            table->setItem(row, COL_VALUE, valueItem);
            valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valueItem->setFlags(Qt::ItemIsEnabled);
            if (!sensor.m_labelRight.isEmpty())
            {
                QTableWidgetItem *unitsItem = new QTableWidgetItem(sensor.m_labelRight);
                unitsItem->setFlags(Qt::ItemIsEnabled);
                table->setItem(row, COL_UNITS, unitsItem);
                hasUnits = true;
            }
            gui->m_sensorValueItems.insert(sensor.m_id, valueItem);
            grid->addWidget(table, 0, 0);
        }
        else
        {
            if (widgetCnt > 0)
            {
                QFrame *line = new QFrame();
                line->setFrameShape(QFrame::VLine);
                line->setFrameShadow(QFrame::Sunken);
                flow->addWidget(line);
            }
            if (!sensor.m_labelLeft.isEmpty())
            {
                QLabel *sensorLabel = new QLabel(sensor.m_labelLeft);
                flow->addWidget(sensorLabel);
            }
            QLabel *sensorValue = new QLabel("-");
            flow->addWidget(sensorValue);
            if (!sensor.m_labelRight.isEmpty())
            {
                QLabel *sensorUnits = new QLabel(sensor.m_labelRight);
                flow->addWidget(sensorUnits);
            }
            gui->m_sensorValueLabels.insert(sensor.m_id, sensorValue);
        }

        if (sensor.m_plot)
        {
            createChart(gui, vBox, sensor.m_id, sensor.m_labelRight);
            hasCharts = true;
        }

        widgetCnt++;
        row++;
    }

    if (table)
    {
        table->resizeColumnToContents(COL_LABEL);
        if (hasUnits) {
            table->resizeColumnToContents(COL_UNITS);
        } else {
            table->hideColumn(COL_UNITS);
        }

        int tableWidth = 0;
        for (int i = 0; i < table->columnCount(); i++){
           tableWidth += table->columnWidth(i);
        }
        int tableHeight = 0;
        for (int i = 0; i < table->rowCount(); i++){
           tableHeight += table->rowHeight(i);
        }
        table->setMinimumWidth(tableWidth);
        table->setMinimumHeight(tableHeight+2);
    }
}

RemoteControlGUI::RemoteControlDeviceGUI *RemoteControlGUI::createDeviceGUI(RemoteControlDevice *rcDevice)
{
    // Create the UI for the device
    RemoteControlDeviceGUI *gui = new RemoteControlDeviceGUI(rcDevice);

    bool hasCharts = false;

    gui->m_container = new QWidget(getRollupContents());
    gui->m_container->setWindowTitle(gui->m_rcDevice->m_label);
    bool vertical = gui->m_rcDevice->m_verticalControls || gui->m_rcDevice->m_verticalSensors;
    QVBoxLayout *vBox = new QVBoxLayout();
    vBox->setContentsMargins(2, 2, 2, 2);
    FlowLayout *flow = nullptr;

    if (!vertical)
    {
        flow = new FlowLayout(2, 6, 6);
        vBox->addItem(flow);
    }
    int widgetCnt = 0;

    // Create buttons to control the device
    createControls(gui, vBox, flow, widgetCnt);

    if (gui->m_rcDevice->m_verticalControls) {
        widgetCnt = 0;
    }

    // Create widgets to display the sensor label and its value
    createSensors(gui, vBox, flow, widgetCnt, hasCharts);

    gui->m_container->setLayout(vBox);

    if (hasCharts && !m_settings.m_chartHeightFixed) {
        gui->m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    gui->m_container->show();
    return gui;
}

void RemoteControlGUI::createGUI()
{
    // Delete existing elements
    for (auto gui : m_deviceGUIs)
    {
        delete gui->m_container;
        gui->m_container = nullptr;
    }
    qDeleteAll(m_deviceGUIs);
    m_deviceGUIs.clear();

    // Create new GUIs for each device
    bool expanding = false;
    for (auto device : m_settings.m_devices)
    {
        RemoteControlDeviceGUI *gui = createDeviceGUI(device);
        m_deviceGUIs.append(gui);
        if (gui->m_container->sizePolicy().verticalPolicy() == QSizePolicy::Expanding) {
            expanding = true;
        }
    }
    if (expanding)
    {
        getRollupContents()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    else
    {
        getRollupContents()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    // FIXME: Why are these three steps needed to get the window
    // to resize to the newly added widgets?
    getRollupContents()->arrangeRollups(); // Recalc rollup size
    layout()->activate(); // Get QMdiSubWindow to recalc its sizeHint
    resize(sizeHint());

    // Need to do it twice when FlowLayout is used!
    getRollupContents()->arrangeRollups();
    layout()->activate();
    resize(sizeHint());
}

void RemoteControlGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RemoteControl::MsgStartStop *message = RemoteControl::MsgStartStop::create(checked);
        m_remoteControl->getInputMessageQueue()->push(message);
    }
}

void RemoteControlGUI::on_update_clicked()
{
    RemoteControl::MsgDeviceGetState *message = RemoteControl::MsgDeviceGetState::create();
    m_remoteControl->getInputMessageQueue()->push(message);
}

void RemoteControlGUI::on_settings_clicked()
{
    // Display settings dialog
    RemoteControlSettingsDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        createGUI();
        applySettings();
        on_update_clicked();
    }
}

void RemoteControlGUI::on_clearData_clicked()
{
    // Clear data in all charts
    for (auto deviceGUI : m_deviceGUIs)
    {
        for (auto series : deviceGUI->m_series) {
            series->clear();
        }
        for (auto series : deviceGUI->m_onePointSeries) {
            series->clear();
        }
    }
}

// Update a control widget with latest state value
void RemoteControlGUI::updateControl(QWidget *widget, const DeviceDiscoverer::ControlInfo *controlInfo, const QString &key, const QVariant &value)
{
    if (ButtonSwitch *button = qobject_cast<ButtonSwitch *>(widget))
    {
        if ((QMetaType::Type)value.type() == QMetaType::QString)
        {
            if (value.toString() == "unavailable")
            {
                button->setStyleSheet("QToolButton { background-color : gray; }"
                                      "QToolButton:checked { background-color : gray; }"
                                      "QToolButton:disabled { background-color : gray; }");
            }
            else if (value.toString() == "error")
            {
                button->setStyleSheet("QToolButton { background-color : red; }"
                                      "QToolButton:checked { background-color : red; }"
                                      "QToolButton:disabled { background-color : red; }");
            }
            else
            {
                qDebug() << "RemoteControlGUI::updateControl: String value for button " << key << value;
            }
        }
        else
        {
            int state = value.toInt();
            int prev = button->blockSignals(true);
            button->setChecked(state != 0);
            button->blockSignals(prev);
            button->setStyleSheet("QToolButton { background-color : blue; }"
                                  "QToolButton:checked { background-color : green; }"
                                  "QToolButton:disabled { background-color : gray; }");
        }
    }
    else if (QSpinBox *spinBox = qobject_cast<QSpinBox *>(widget))
    {
        int prev = spinBox->blockSignals(true);
        if (value.toString() == "unavailable")
        {
            spinBox->setStyleSheet("QSpinBox { background-color : gray; }");
        }
        else if (value.toString() == "error")
        {
            spinBox->setStyleSheet("QSpinBox { background-color : red; }");
        }
        else
        {
            int state = value.toInt();
            bool outOfRange = (state < spinBox->minimum()) || (state > spinBox->maximum());
            spinBox->setValue(state);
            if (outOfRange) {
                spinBox->setStyleSheet("QSpinBox { background-color : red; }");
            } else {
                spinBox->setStyleSheet("");
            }
        }
        spinBox->blockSignals(prev);
    }
    else if (QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox *>(widget))
    {
        int prev = spinBox->blockSignals(true);
        if (value.toString() == "unavailable")
        {
            spinBox->setStyleSheet("QDoubleSpinBox { background-color : gray; }");
        }
        else if (value.toString() == "error")
        {
            spinBox->setStyleSheet("QDoubleSpinBox { background-color : red; }");
        }
        else
        {
            double state = value.toDouble();
            if (controlInfo) {
                state = state / controlInfo->m_scale;
            }
            bool outOfRange = (state < spinBox->minimum()) || (state > spinBox->maximum());
            spinBox->setValue(state);
            if (outOfRange) {
                spinBox->setStyleSheet("QDoubleSpinBox { background-color : red; }");
            } else {
                spinBox->setStyleSheet("");
            }
        }
        spinBox->blockSignals(prev);
    }
    else if (QDial *dial = qobject_cast<QDial *>(widget))
    {
        int prev = dial->blockSignals(true);
        if (value.toString() == "unavailable")
        {
            dial->setStyleSheet("QDial { background-color : gray; }");
        }
        else if (value.toString() == "error")
        {
            dial->setStyleSheet("QDial { background-color : red; }");
        }
        else
        {
            double state = value.toDouble();
            if (controlInfo) {
                state = state / controlInfo->m_scale;
            }
            bool outOfRange = (state < dial->minimum()) || (state > dial->maximum());
            dial->setValue(state);
            if (outOfRange) {
                dial->setStyleSheet("QDial { background-color : red; }");
            } else {
                dial->setStyleSheet("");
            }
        }
        dial->blockSignals(prev);
    }
    else if (QSlider *slider = qobject_cast<QSlider *>(widget))
    {
        int prev = slider->blockSignals(true);
        if (value.toString() == "unavailable")
        {
            slider->setStyleSheet("QSlider { background-color : gray; }");
        }
        else if (value.toString() == "error")
        {
            slider->setStyleSheet("QSlider { background-color : red; }");
        }
        else
        {
            double state = value.toDouble();
            if (controlInfo) {
                state = state / controlInfo->m_scale;
            }
            bool outOfRange = (state < slider->minimum()) || (state > slider->maximum());
            slider->setValue(state);
            if (outOfRange) {
                slider->setStyleSheet("QSlider { background-color : red; }");
            } else {
                slider->setStyleSheet("");
            }
        }
        slider->blockSignals(prev);
    }
    else if (QComboBox *comboBox = qobject_cast<QComboBox *>(widget))
    {
        int prev = comboBox->blockSignals(true);
        QString string = value.toString();
        int index = comboBox->findText(string);
        if (index != -1)
        {
            comboBox->setCurrentIndex(index);
            comboBox->setStyleSheet("");
        }
        else
        {
            comboBox->setStyleSheet("QComboBox { background-color : red; }");
        }
        comboBox->blockSignals(prev);
    }
    else if (QLineEdit *lineEdit = qobject_cast<QLineEdit *>(widget))
    {
        lineEdit->setText(value.toString());
    }
    else if (QLabel *label = qobject_cast<QLabel *>(widget))
    {
        label->setText(value.toString());
    }
    else
    {
        qDebug() << "RemoteControlGUI::updateControl: Unexpected widget type";
    }
}

void RemoteControlGUI::updateChart(RemoteControlDeviceGUI *deviceGUI, const QString &key, const QVariant &value)
{
    // Format the value for display
    bool ok = false;
    double d = value.toDouble(&ok);
    bool iOk = false;
    int iValue = value.toInt(&iOk);
    QString formattedValue;
    RemoteControlSensor *sensor = deviceGUI->m_rcDevice->getSensor(key);
    QString format = sensor->m_format.trimmed();
    if (format.contains("%s"))
    {
        formattedValue = QString::asprintf(format.toUtf8(), value.toString().toUtf8().data());
    }
    else if (format.contains("%d") || format.contains("%u") || format.contains("%x") || format.contains("%X"))
    {
        formattedValue = QString::asprintf(format.toUtf8(), value.toInt());
    }
    else if (((QMetaType::Type)value.type() == QMetaType::Double) || ((QMetaType::Type)value.type() == QMetaType::Float))
    {
        if (format.isEmpty()) {
            format = "%.1f";
        }
        formattedValue = QString::asprintf(format.toUtf8(), value.toDouble());
    }
    else if (iOk)
    {
        formattedValue = QString::asprintf("%d", iValue);
    }
    else
    {
        formattedValue = value.toString();
    }

    // Update sensor value widget to display the latest value
    if (deviceGUI->m_sensorValueLabels.contains(key)) {
        deviceGUI->m_sensorValueLabels.value(key)->setText(formattedValue);
    } else {
        deviceGUI->m_sensorValueItems.value(key)->setText(formattedValue);
    }

    // Plot value on chart
    if (deviceGUI->m_series.contains(key))
    {
        QLineSeries *onePointSeries = deviceGUI->m_onePointSeries.value(key);
        QLineSeries *series = deviceGUI->m_series.value(key);
        QDateTime dt = QDateTime::currentDateTime();
        if (ok)
        {
            // Charts aren't displayed properly if series has only one point,
            // so we save the first point in an additional series: onePointSeries
            if (onePointSeries->count() == 0)
            {
                onePointSeries->append(dt.toMSecsSinceEpoch(), d);
            }
            else
            {
                if (series->count() == 0) {
                    series->append(onePointSeries->at(0));
                }
                series->append(dt.toMSecsSinceEpoch(), d);
                QList<QAbstractAxis *> axes = deviceGUI->m_chart->axes(Qt::Horizontal, series);
                QDateTimeAxis *dtAxis = (QDateTimeAxis *)axes[0];
                QDateTime start = QDateTime::fromMSecsSinceEpoch(series->at(0).x());
                QDateTime end = QDateTime::fromMSecsSinceEpoch(series->at(series->count() - 1).x());
                if (start.date() == end.date())
                {
                    if (start.secsTo(end) < 60*5) {
                        dtAxis->setFormat(QString("hh:mm:ss"));
                    } else {
                        dtAxis->setFormat(QString("hh:mm"));
                    }
                }
                else
                {
                    dtAxis->setFormat(QString("%1 hh:mm").arg(QLocale::system().dateFormat(QLocale::ShortFormat)));
                }
                dtAxis->setRange(start, end);
                axes = deviceGUI->m_chart->axes(Qt::Vertical, series);
                QValueAxis *yAxis = (QValueAxis *)axes[0];
                if (series->count() == 2)
                {
                    double y1 = series->at(0).y();
                    double y2 = series->at(1).y();
                    double yMin = std::min(y1, y2);
                    double yMax = std::max(y1, y2);
                    double min = (yMin >= 0.0) ? yMin * 0.9 : yMin * 1.1;
                    double max = (yMax >= 0.0) ? yMax * 1.1 : yMax * 0.9;
                    yAxis->setRange(min, max);
                }
                else
                {
                    double min = (d >= 0.0) ? d * 0.9 : d * 1.1;
                    double max = (d >= 0.0) ? d * 1.1 : d * 0.9;
                    if (min < yAxis->min()) {
                        yAxis->setMin(min);
                    }
                    if (max > yAxis->max()) {
                        yAxis->setMax(max);
                    }
                }
            }
        }
        else
        {
            qDebug() << "RemoteControlGUI::deviceUpdated: Error converting " << key << value;
        }
    }
}

void RemoteControlGUI::deviceUpdated(const QString &protocol, const QString &deviceId, const QHash<QString, QVariant> &status)
{
    for (auto deviceGUI : m_deviceGUIs)
    {
        if (   (protocol == deviceGUI->m_rcDevice->m_protocol)
            && (deviceId == deviceGUI->m_rcDevice->m_info.m_id))
        {
            deviceGUI->m_container->setEnabled(true);

            QHashIterator<QString, QVariant> itr(status);

            while (itr.hasNext())
            {
                itr.next();
                QString key = itr.key();
                QVariant value = itr.value();

                if (deviceGUI->m_controls.contains(key))
                {
                    // Update control(s) to display latest state
                    QList<QWidget *> widgets = deviceGUI->m_controls.value(key);
                    DeviceDiscoverer::ControlInfo *control = deviceGUI->m_rcDevice->m_info.getControl(key);

                    for (auto widget : widgets) {
                        updateControl(widget, control, key, value);
                    }
                }
                else if (deviceGUI->m_sensorValueLabels.contains(key) || deviceGUI->m_sensorValueItems.contains(key))
                {
                    // Plot on chart
                    updateChart(deviceGUI, key, value);
                }
                else
                {
                    qDebug() << "RemoteControlGUI::deviceUpdated: Unexpected status key " << key << value;
                }
            }
        }
    }
}

void RemoteControlGUI::deviceUnavailable(const QString &protocol, const QString &deviceId)
{
    for (auto deviceGUI : m_deviceGUIs)
    {
        if (   (protocol == deviceGUI->m_rcDevice->m_protocol)
            && (deviceId == deviceGUI->m_rcDevice->m_info.m_id))
        {
            deviceGUI->m_container->setEnabled(false);
        }
    }
}

void RemoteControlGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        RemoteControl::MsgConfigureRemoteControl* message = RemoteControl::MsgConfigureRemoteControl::create(m_settings, force);
        m_remoteControl->getInputMessageQueue()->push(message);
    }
}

void RemoteControlGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RemoteControlGUI::on_startStop_toggled);
    QObject::connect(ui->update, &QToolButton::clicked, this, &RemoteControlGUI::on_update_clicked);
    QObject::connect(ui->settings, &QToolButton::clicked, this, &RemoteControlGUI::on_settings_clicked);
    QObject::connect(ui->clearData, &QToolButton::clicked, this, &RemoteControlGUI::on_clearData_clicked);
}
