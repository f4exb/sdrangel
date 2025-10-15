#include "tcpiqplotgui.h"
#include "ui_tcpiqplotgui.h"
#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/devicesamplesource.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include <QDebug>
#include <QTimer>
#include <iostream>

TcpIqPlotGUI* TcpIqPlotGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx)
{
    if (!channelRx) {
        return nullptr;
    }
    return new TcpIqPlotGUI(pluginAPI, deviceUISet, channelRx);
}

void TcpIqPlotGUI::destroy()
{
    delete this;
}

TcpIqPlotGUI::TcpIqPlotGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *channelRx, QWidget *parent) :
    ChannelGUI(parent),
    ui(new Ui::TcpIqPlotGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_tcpIqPlot(dynamic_cast<TcpIqPlot*>(channelRx)),
    m_doApplySettings(false)
{
    // Force immediate output
    if (!m_tcpIqPlot) {
        qCritical() << "TcpIqPlotGUI: dynamic_cast failed - channelRx is not a TcpIqPlot*";
        return;
    }
    
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/tcpiqplot/readme.md";
    
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    
    ui->plotWidget->setPlotMode(IqPlotWidget::MODE_TIME_DOMAIN);
    
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    
    // Connect TcpIqPlot signal to plot widget slot for live data plotting
    // Using new-style connection (function pointers) instead of SIGNAL/SLOT macros
    if (m_tcpIqPlot && ui->plotWidget) {
        bool connected = connect(m_tcpIqPlot, &TcpIqPlot::newDataAvailable, 
                                 ui->plotWidget, &IqPlotWidget::onNewData, 
                                 Qt::QueuedConnection);
        fprintf(stderr, "TcpIqPlotGUI: Signal connection %s (new-style)\n", connected ? "SUCCEEDED" : "FAILED");
        fprintf(stderr, "  m_tcpIqPlot=%p, ui->plotWidget=%p\n", (void*)m_tcpIqPlot, (void*)ui->plotWidget);
        fflush(stderr);
    }
    
    displaySettings();
    makeUIConnections();
    
    // Update frequency labels
    updateFrequencyLabels();
    
    // Setup timer to periodically update frequency labels (every 100ms for responsive updates)
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &TcpIqPlotGUI::updateFrequencyLabels);
    m_updateTimer->start(100); // Update every 100ms (10 times per second)
    
    m_doApplySettings = true;
    applySettings(true);
    std::cerr.flush();
    qDebug().noquote() << "=== TcpIqPlotGUI CONSTRUCTOR COMPLETE ===";
}

TcpIqPlotGUI::~TcpIqPlotGUI()
{
    // TODO: Re-enable when setMessageQueueToGUI issue is fixed
    // m_tcpIqPlot->setMessageQueueToGUI(nullptr);
    delete ui;
}

void TcpIqPlotGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray TcpIqPlotGUI::serialize() const
{
    return m_settings.serialize();
}

bool TcpIqPlotGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool TcpIqPlotGUI::handleMessage(const Message& message)
{
    if (TcpIqPlot::MsgStatus::match(message)) {
        const TcpIqPlot::MsgStatus& statusMsg = (const TcpIqPlot::MsgStatus&)message;
        ui->statusLabel->setText(statusMsg.getStatus());
        return true;
    }
    if (TcpIqPlot::MsgNewData::match(message)) {
        const TcpIqPlot::MsgNewData& dataMsg = (const TcpIqPlot::MsgNewData&)message;
        // Send data to plot widget
        ui->plotWidget->onNewData(dataMsg.getIData(), dataMsg.getQData());
        return true;
    }
    return false;
}

void TcpIqPlotGUI::handleSourceMessages()
{
    Message* message;
    while ((message = getInputMessageQueue()->pop()) != nullptr) {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void TcpIqPlotGUI::applySettings(bool force)
{
    std::cerr << "applySettings() called, m_doApplySettings=" << m_doApplySettings << std::endl;
    if (m_doApplySettings) {
        std::cerr << "  SKIPPING message creation/push to test if GUI appears" << std::endl;
        // TODO: Fix the message queue push crash
        // TcpIqPlot::MsgConfigure* message = TcpIqPlot::MsgConfigure::create(m_settings, force);
        // m_tcpIqPlot->getInputMessageQueue()->push(message);
    }
    std::cerr << "applySettings() completed" << std::endl;
}

void TcpIqPlotGUI::displaySettings()
{
    blockApplySettings(true);
    ui->port->setValue(m_settings.m_port);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);
    blockApplySettings(false);
}

void TcpIqPlotGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void TcpIqPlotGUI::on_port_editingFinished()
{
    m_settings.m_port = ui->port->value();
    applySettings();
}

void TcpIqPlotGUI::on_plotModeCombo_currentIndexChanged(int index)
{
    qDebug() << "Plot mode changed to index" << index;
    IqPlotWidget::PlotMode mode;
    
    switch (index) {
        case 0: mode = IqPlotWidget::MODE_TIME_DOMAIN; break;
        case 1: mode = IqPlotWidget::MODE_CONSTELLATION; break;
        case 2: mode = IqPlotWidget::MODE_MAGNITUDE; break;
        case 3: mode = IqPlotWidget::MODE_PHASE; break;
        case 4: mode = IqPlotWidget::MODE_FFT_SPECTRUM; break;
        default: mode = IqPlotWidget::MODE_TIME_DOMAIN; break;
    }
    
    ui->plotWidget->setPlotMode(mode);
}

void TcpIqPlotGUI::on_startButton_clicked()
{
    if (!m_tcpIqPlot || !ui->statusLabel) {
        return;
    }
    
    int port = m_settings.m_port;

    m_tcpIqPlot->startServer(port);
    ui->statusLabel->setText(m_tcpIqPlot->getStatusText());
}

void TcpIqPlotGUI::on_stopButton_clicked()
{
    m_tcpIqPlot->stopServer();
    ui->statusLabel->setText(m_tcpIqPlot->getStatusText());
}

void TcpIqPlotGUI::on_persistenceCheckBox_toggled(bool checked)
{
    if (ui->plotWidget) {
        ui->plotWidget->setPersistenceMode(checked);
    }
}

void TcpIqPlotGUI::makeUIConnections()
{
    QObject::connect(ui->port, &QSpinBox::editingFinished, this, &TcpIqPlotGUI::on_port_editingFinished);
    QObject::connect(ui->plotModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TcpIqPlotGUI::on_plotModeCombo_currentIndexChanged);
    QObject::connect(ui->startButton, &QPushButton::clicked, this, &TcpIqPlotGUI::on_startButton_clicked);
    QObject::connect(ui->stopButton, &QPushButton::clicked, this, &TcpIqPlotGUI::on_stopButton_clicked);
    QObject::connect(ui->persistenceCheckBox, &QCheckBox::toggled, this, &TcpIqPlotGUI::on_persistenceCheckBox_toggled);
}

void TcpIqPlotGUI::updateFrequencyLabels()
{
    if (m_deviceUISet && ui->plotWidget)
    {
        // Get device sample rate and center frequency
        DeviceSampleSource *source = m_deviceUISet->m_deviceAPI->getSampleSource();
        double sampleRate = 1e6;  // Default 1 MHz
        double centerFreq = 100e6; // Default 100 MHz
        
        if (source) {
            sampleRate = source->getSampleRate();
            centerFreq = source->getCenterFrequency();
        }
        
        // Pass to plot widget
        ui->plotWidget->setSampleRate(sampleRate);
        ui->plotWidget->setCenterFrequency(centerFreq);
        
        fprintf(stderr, "Updated frequency labels: SR=%.2f MHz, CF=%.2f MHz\n", 
                sampleRate / 1e6, centerFreq / 1e6);
        fflush(stderr);
    }
}
