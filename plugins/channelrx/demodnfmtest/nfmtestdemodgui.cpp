#include "nfmtestdemodgui.h"

#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "ui_nfmtestdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "nfmdemodreport.h"
#include "nfmtestdemod.h"

NFMDemodTestGUI* NFMDemodTestGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	NFMDemodTestGUI* gui = new NFMDemodTestGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void NFMDemodTestGUI::destroy()
{
	delete this;
}

void NFMDemodTestGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString NFMDemodTestGUI::getName() const
{
	return objectName();
}

qint64 NFMDemodTestGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void NFMDemodTestGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void NFMDemodTestGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray NFMDemodTestGUI::serialize() const
{
    return m_settings.serialize();
}

bool NFMDemodTestGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool NFMDemodTestGUI::handleMessage(const Message& message)
{
    if (NFMDemodReport::MsgReportCTCSSFreq::match(message))
    {
        NFMDemodReport::MsgReportCTCSSFreq& report = (NFMDemodReport::MsgReportCTCSSFreq&) message;
        setCtcssFreq(report.getFrequency());
        return true;
    }
    else if (NFMDemodTest::MsgConfigureNFMDemod::match(message))
    {
        qDebug("NFMDemodTestGUI::handleMessage: NFMDemodTest::MsgConfigureNFMDemod");
        const NFMDemodTest::MsgConfigureNFMDemod& cfg = (NFMDemodTest::MsgConfigureNFMDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }

    return false;
}

void NFMDemodTestGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void NFMDemodTestGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMDemodTestGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void NFMDemodTestGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMDemodTestGUI::on_rfBW_currentIndexChanged(int index)
{
	qDebug() << "NFMDemodTestGUI::on_rfBW_currentIndexChanged" << index;
	//ui->rfBWText->setText(QString("%1 k").arg(m_rfBW[value] / 1000.0));
	m_channelMarker.setBandwidth(NFMDemodTestSettings::getRFBW(index));
	m_settings.m_rfBandwidth = NFMDemodTestSettings::getRFBW(index);
	m_settings.m_fmDeviation = NFMDemodTestSettings::getFMDev(index);
	applySettings();
}

void NFMDemodTestGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 k").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void NFMDemodTestGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void NFMDemodTestGUI::on_squelchGate_valueChanged(int value)
{
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0f, 0, 'f', 0));
    m_settings.m_squelchGate = value;
	applySettings();
}

void NFMDemodTestGUI::on_deltaSquelch_toggled(bool checked)
{
    if (checked)
    {
        ui->squelchText->setText(QString("%1").arg((-ui->squelch->value()) / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch AF balance threshold (%)"));
        ui->squelch->setToolTip(tr("Squelch AF balance threshold (%)"));
    }
    else
    {
        ui->squelchText->setText(QString("%1").arg(ui->squelch->value() / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch power threshold (dB)"));
        ui->squelch->setToolTip(tr("Squelch power threshold (dB)"));
    }
    m_settings.m_deltaSquelch = checked;
    applySettings();
}

void NFMDemodTestGUI::on_squelch_valueChanged(int value)
{
    if (ui->deltaSquelch->isChecked())
    {
        ui->squelchText->setText(QString("%1").arg(-value / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch AF balance threshold (%)"));
        ui->squelch->setToolTip(tr("Squelch AF balance threshold (%)"));
    }
    else
    {
        ui->squelchText->setText(QString("%1").arg(value / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch power threshold (dB)"));
        ui->squelch->setToolTip(tr("Squelch power threshold (dB)"));
    }
    m_settings.m_squelch = value * 1.0;
	applySettings();
}

void NFMDemodTestGUI::on_ctcssOn_toggled(bool checked)
{
	m_settings.m_ctcssOn = checked;
	applySettings();
}

void NFMDemodTestGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPass = checked;
    applySettings();
}

void NFMDemodTestGUI::on_audioMute_toggled(bool checked)
{
	m_settings.m_audioMute = checked;
	applySettings();
}

void NFMDemodTestGUI::on_ctcss_currentIndexChanged(int index)
{
	m_settings.m_ctcssIndex = index;
	applySettings();
}

void NFMDemodTestGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
	/*
	if((widget == ui->spectrumContainer) && (m_nfmDemod != NULL))
		m_nfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void NFMDemodTestGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_nfmDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

NFMDemodTestGUI::NFMDemodTestGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::NFMDemodTestGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_squelchOpen(false),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_nfmDemod = reinterpret_cast<NFMDemodTest*>(rxChannel); //new NFMDemodTest(m_deviceUISet->m_deviceSourceAPI);
	m_nfmDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_nfmDemod->propagateMessageQueueToGUI();

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    blockApplySettings(true);

    ui->rfBW->clear();

    for (int i = 0; i < NFMDemodTestSettings::m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(NFMDemodTestSettings::getRFBW(i) / 1000.0, 0, 'f', 2));
    }

    int ctcss_nbTones;
    const Real *ctcss_tones = m_nfmDemod->getCtcssToneSet(ctcss_nbTones);

    ui->ctcss->addItem("--");

    for (int i=0; i<ctcss_nbTones; i++)
    {
        ui->ctcss->addItem(QString("%1").arg(ctcss_tones[i]));
    }

    blockApplySettings(false);

	ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }"); // squelch closed

	ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(5000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("NFM Test Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

    m_deviceUISet->registerRxChannelInstance(NFMDemodTest::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	QChar delta = QChar(0x94, 0x03);
	ui->deltaSquelch->setText(delta);

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	displaySettings();
	applySettings(true);
}

NFMDemodTestGUI::~NFMDemodTestGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_nfmDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete ui;
}

void NFMDemodTestGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "NFMDemodTestGUI::applySettings";

        NFMDemodTest::MsgConfigureNFMDemod* message = NFMDemodTest::MsgConfigureNFMDemod::create( m_settings, force);
        m_nfmDemod->getInputMessageQueue()->push(message);
	}
}

void NFMDemodTestGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setCurrentIndex(NFMDemodTestSettings::getRFBWIndex(m_settings.m_rfBandwidth));

    ui->afBWText->setText(QString("%1 k").arg(m_settings.m_afBandwidth / 1000.0));
    ui->afBW->setValue(m_settings.m_afBandwidth / 1000.0);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));
    ui->volume->setValue(m_settings.m_volume * 10.0);

    ui->squelchGateText->setText(QString("%1").arg(m_settings.m_squelchGate * 10.0f, 0, 'f', 0));
    ui->squelchGate->setValue(m_settings.m_squelchGate);

    ui->deltaSquelch->setChecked(m_settings.m_deltaSquelch);
    ui->squelch->setValue(m_settings.m_squelch * 1.0);

    if (m_settings.m_deltaSquelch)
    {
        ui->squelchText->setText(QString("%1").arg((-m_settings.m_squelch) / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch AF balance threshold (%)"));
        ui->squelch->setToolTip(tr("Squelch AF balance threshold (%)"));
    }
    else
    {
        ui->squelchText->setText(QString("%1").arg(m_settings.m_squelch / 1.0, 0, 'f', 0));
        ui->squelchText->setToolTip(tr("Squelch power threshold (dB)"));
        ui->squelch->setToolTip(tr("Squelch power threshold (dB)"));
    }

    ui->ctcssOn->setChecked(m_settings.m_ctcssOn);
    ui->highPassFilter->setChecked(m_settings.m_highPass);
    ui->audioMute->setChecked(m_settings.m_audioMute);

    ui->ctcss->setCurrentIndex(m_settings.m_ctcssIndex);

    displayStreamIndex();

    blockApplySettings(false);
}

void NFMDemodTestGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void NFMDemodTestGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void NFMDemodTestGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void NFMDemodTestGUI::setCtcssFreq(Real ctcssFreq)
{
	if (ctcssFreq == 0)
	{
		ui->ctcssText->setText("--");
	}
	else
	{
		ui->ctcssText->setText(QString("%1").arg(ctcssFreq));
	}
}

void NFMDemodTestGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void NFMDemodTestGUI::audioSelect()
{
    qDebug("NFMDemodTestGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void NFMDemodTestGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_nfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    bool squelchOpen = m_nfmDemod->getSquelchOpen();

	if (squelchOpen != m_squelchOpen)
	{
		if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_squelchOpen = squelchOpen;
	}

	m_tickCount++;
}
