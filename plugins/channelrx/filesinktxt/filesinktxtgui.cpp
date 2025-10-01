#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>

#include "device/deviceuiset.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_filesinktxtgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "maincore.h"

#include "filesinktxtgui.h"
#include "filesinktxt.h"

FileSinkTxtGUI* FileSinkTxtGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    FileSinkTxtGUI* gui = new FileSinkTxtGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void FileSinkTxtGUI::destroy()
{
    delete this;
}

void FileSinkTxtGUI::resetToDefaults()
{
    blockApplySettings(true);
    ui->filenameEdit->setText("output.txt");
    blockApplySettings(false);
    applySettings(true);
}

FileSinkTxtGUI::FileSinkTxtGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::FileSinkTxtGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/filesinktxt/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    
    m_fileSinkTxt = reinterpret_cast<FileSinkTxt*>(rxChannel);
    m_fileSinkTxt->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *rightClickEnabler = new CRightClickEnabler(ui->recordPushButton);
    connect(rightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_channelMarker.setColor(Qt::green);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("File Sink Text");
    m_channelMarker.setVisible(true);
    
    setTitleColor(m_channelMarker.getColor());
    m_channelMarker.setMovable(false);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    makeUIConnections();
    
    ui->filenameEdit->setText(m_fileSinkTxt->getFileName());
    ui->statusLabel->setText("Ready");
    
    displaySettings();
    applySettings(true);
    
    // DialPopup::addPopupsToChildDials(this); // Not needed for minimal plugin
    m_resizer.enableChildMouseTracking();
}

FileSinkTxtGUI::~FileSinkTxtGUI()
{
    delete ui;
}

void FileSinkTxtGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void FileSinkTxtGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        m_fileSinkTxt->setFileName(ui->filenameEdit->text());
    }
}

void FileSinkTxtGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.blockSignals(false);
}

bool FileSinkTxtGUI::handleMessage(const Message& message)
{
    (void) message;
    return false;
}

void FileSinkTxtGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr) {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void FileSinkTxtGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
    
    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void FileSinkTxtGUI::onMenuDialogCalled(const QPoint &p)
{
    (void) p;
    // BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    // dialog.setUseReverseAPI(false);
    // dialog.setReverseAPIAddress("127.0.0.1");
    // dialog.setReverseAPIPort(8888);
    // dialog.setReverseAPIDeviceIndex(0);
    // dialog.setReverseAPIChannelIndex(0);
    // dialog.setDefaultTitle(m_displayedName);

    // if (dialog.hasChanged())
    // {
    //     setWindowTitle(dialog.getTitle());
    //     setTitle(dialog.getTitle());
    //     setTitleColor(dialog.getColor());
    // }
    
    resetContextMenuType();
}

void FileSinkTxtGUI::makeUIConnections()
{
    QObject::connect(ui->browsePushButton, &QPushButton::clicked, this, &FileSinkTxtGUI::on_browsePushButton_clicked);
    QObject::connect(ui->recordPushButton, &QPushButton::toggled, this, &FileSinkTxtGUI::on_recordPushButton_toggled);
    QObject::connect(ui->savePushButton, &QPushButton::clicked, this, &FileSinkTxtGUI::on_savePushButton_clicked);
    QObject::connect(ui->loadPushButton, &QPushButton::clicked, this, &FileSinkTxtGUI::on_loadPushButton_clicked);
}

void FileSinkTxtGUI::tick()
{
    if (m_fileSinkTxt->isRecording()) {
        ui->statusLabel->setText("Recording...");
    } else {
        ui->statusLabel->setText("Ready");
    }
}

void FileSinkTxtGUI::on_browsePushButton_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save I/Q File"), ui->filenameEdit->text(), tr("Text Files (*.txt)"));
    
    if (!fileName.isEmpty()) {
        ui->filenameEdit->setText(fileName);
        applySettings();
    }
}

void FileSinkTxtGUI::on_recordPushButton_toggled(bool checked)
{
    if (checked) {
        m_fileSinkTxt->startRecording();
        ui->recordPushButton->setText("Stop");
    } else {
        m_fileSinkTxt->stopRecording();
        ui->recordPushButton->setText("Record");
    }
}

void FileSinkTxtGUI::on_savePushButton_clicked()
{
    m_fileSinkTxt->saveToFile();
    ui->statusLabel->setText("Saved");
}

void FileSinkTxtGUI::on_loadPushButton_clicked()
{
    m_fileSinkTxt->loadFromFile();
    ui->statusLabel->setText("Loaded");
}
