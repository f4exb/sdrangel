#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "ui_nfmdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dcscodes.h"
#include "maincore.h"

#include "nfmdemodreport.h"
#include "nfmdemod.h"
#include "nfmdemodgui.h"

NFMDemodGUI* NFMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	NFMDemodGUI* gui = new NFMDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void NFMDemodGUI::destroy()
{
	delete this;
}

void NFMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings();
}

QByteArray NFMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool NFMDemodGUI::deserialize(const QByteArray& data)
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

bool NFMDemodGUI::handleMessage(const Message& message)
{
    if (NFMDemodReport::MsgReportCTCSSFreq::match(message))
    {
        NFMDemodReport::MsgReportCTCSSFreq& report = (NFMDemodReport::MsgReportCTCSSFreq&) message;
        setCtcssFreq(report.getFrequency());
        return true;
    }
    else if (NFMDemodReport::MsgReportDCSCode::match(message))
    {
        NFMDemodReport::MsgReportDCSCode& report = (NFMDemodReport::MsgReportDCSCode&) message;
        m_reportedDcsCode = report.getCode();
        setDcsCode(report.getCode());
        return true;
    }
    else if (NFMDemod::MsgConfigureNFMDemod::match(message))
    {
        qDebug("NFMDemodGUI::handleMessage: NFMDemod::MsgConfigureNFMDemod");
        const NFMDemod::MsgConfigureNFMDemod& cfg = (NFMDemod::MsgConfigureNFMDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }

    return false;
}

void NFMDemodGUI::handleInputMessages()
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

void NFMDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void NFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NFMDemodGUI::on_channelSpacingApply_clicked()
{
    int index = ui->channelSpacing->currentIndex();
    qDebug("NFMDemodGUI::on_channelSpacing_currentIndexChanged: %d", index);
	m_settings.m_rfBandwidth = NFMDemodSettings::getRFBW(index);
    m_settings.m_afBandwidth = NFMDemodSettings::getAFBW(index);
    m_settings.m_fmDeviation = 2.0 * NFMDemodSettings::getFMDev(index);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    ui->rfBW->blockSignals(true);
    ui->afBW->blockSignals(true);
    ui->fmDev->blockSignals(true);
    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->afBWText->setText(QString("%1k").arg(m_settings.m_afBandwidth / 1000.0, 0, 'f', 1));
    ui->afBW->setValue(m_settings.m_afBandwidth / 100.0);
    ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(m_settings.m_fmDeviation / 2000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 200.0);
    ui->rfBW->blockSignals(false);
    ui->afBW->blockSignals(false);
    ui->fmDev->blockSignals(false);
	applySettings();
}

void NFMDemodGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_rfBandwidth = value * 100.0;
	m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);

    ui->channelSpacing->blockSignals(true);
    ui->channelSpacing->setCurrentIndex(NFMDemodSettings::getChannelSpacingIndex(m_settings.m_rfBandwidth));
    ui->channelSpacing->update();
    ui->channelSpacing->blockSignals(false);

	applySettings();
}

void NFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_afBandwidth = value * 100.0;
	applySettings();
}

void NFMDemodGUI::on_fmDev_valueChanged(int value)
{
	ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(value / 10.0, 0, 'f', 1));
	m_settings.m_fmDeviation = value * 200.0;
	applySettings();
}

void NFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value));
	m_settings.m_volume = value / 100.0;
	applySettings();
}

void NFMDemodGUI::on_squelchGate_valueChanged(int value)
{
    ui->squelchGateText->setText(QString("%1").arg(value * 10.0f, 0, 'f', 0));
    m_settings.m_squelchGate = value;
	applySettings();
}

void NFMDemodGUI::on_deltaSquelch_toggled(bool checked)
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

void NFMDemodGUI::on_squelch_valueChanged(int value)
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

void NFMDemodGUI::on_ctcssOn_toggled(bool checked)
{
	m_settings.m_ctcssOn = checked;
	applySettings();
}

void NFMDemodGUI::on_dcsOn_toggled(bool checked)
{
    m_settings.m_dcsOn = checked;
    applySettings();
}

void NFMDemodGUI::on_dcsCode_currentIndexChanged(int index)
{
    if (index == 0)
    {
        m_settings.m_dcsCode = 0;
        applySettings();
    }
    else
    {
        QString dcsText = ui->dcsCode->currentText();
        bool positive = (dcsText[3] == 'P');
        dcsText.chop(1);
        bool ok;
        int dcsCode = dcsText.toInt(&ok, 8);

        if (ok)
        {
            m_settings.m_dcsCode = dcsCode;
            m_settings.m_dcsPositive = positive;
            applySettings();
        }
    }
}

void NFMDemodGUI::on_highPassFilter_toggled(bool checked)
{
    m_settings.m_highPass = checked;
    applySettings();
}

void NFMDemodGUI::on_audioMute_toggled(bool checked)
{
	m_settings.m_audioMute = checked;
	applySettings();
}

void NFMDemodGUI::on_ctcss_currentIndexChanged(int index)
{
	m_settings.m_ctcssIndex = index;
	applySettings();
}

void NFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
	/*
	if((widget == ui->spectrumContainer) && (m_nfmDemod != NULL))
		m_nfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void NFMDemodGUI::onMenuDialogCalled(const QPoint &p)
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

NFMDemodGUI::NFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::NFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
    m_reportedDcsCode(0),
    m_dcsShowPositive(false),
	m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_nfmDemod = reinterpret_cast<NFMDemod*>(rxChannel);
	m_nfmDemod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect()));

    blockApplySettings(true);

    ui->channelSpacing->clear();

    for (int i = 0; i < NFMDemodSettings::m_nbChannelSpacings; i++) {
        ui->channelSpacing->addItem(QString("%1").arg(NFMDemodSettings::getChannelSpacing(i) / 1000.0, 0, 'f', 2));
    }

    ui->channelSpacing->setCurrentIndex(NFMDemodSettings::getChannelSpacingIndex(12500));

    int ctcss_nbTones;
    const Real *ctcss_tones = m_nfmDemod->getCtcssToneSet(ctcss_nbTones);

    ui->ctcss->addItem("--");

    for (int i=0; i<ctcss_nbTones; i++) {
        ui->ctcss->addItem(QString("%1").arg(ctcss_tones[i]));
    }

    ui->dcsOn->setChecked(m_settings.m_dcsOn);
    QList<unsigned int> dcsCodes;
    DCSCodes::getCanonicalCodes(dcsCodes);
    ui->dcsCode->addItem("--");

    for (auto dcsCode : dcsCodes)
    {
        ui->dcsCode->addItem(QString("%1P").arg(dcsCode, 3, 8, QLatin1Char('0')));
        ui->dcsCode->addItem(QString("%1N").arg(dcsCode, 3, 8, QLatin1Char('0')));
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
    m_channelMarker.setTitle("NFM Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_settings.setChannelMarker(&m_channelMarker);

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

NFMDemodGUI::~NFMDemodGUI()
{
	delete ui;
}

void NFMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		qDebug() << "NFMDemodGUI::applySettings";

        NFMDemod::MsgConfigureNFMDemod* message = NFMDemod::MsgConfigureNFMDemod::create( m_settings, force);
        m_nfmDemod->getInputMessageQueue()->push(message);
	}
}

void NFMDemodGUI::displaySettings()
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

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->afBWText->setText(QString("%1k").arg(m_settings.m_afBandwidth / 1000.0, 0, 'f', 1));
    ui->afBW->setValue(m_settings.m_afBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1%2k").arg(QChar(0xB1, 0x00)).arg(m_settings.m_fmDeviation / 2000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 200.0);

    ui->channelSpacing->blockSignals(true);
    ui->channelSpacing->setCurrentIndex(NFMDemodSettings::getChannelSpacingIndex(m_settings.m_rfBandwidth));
    ui->channelSpacing->blockSignals(false);

    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume*100.0, 0, 'f', 0));
    ui->volume->setValue(m_settings.m_volume * 100.0);

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
    ui->dcsOn->setChecked(m_settings.m_dcsOn);

    if (m_settings.m_dcsCode == 0) {
        ui->dcsCode->setCurrentText(tr("--"));
    } else {
        ui->dcsCode->setCurrentText(tr("%1%2")
            .arg(m_settings.m_dcsCode, 3, 8, QLatin1Char('0'))
            .arg(m_settings.m_dcsPositive ? "P" : "N")
        );
    }

    setDcsCode(m_reportedDcsCode);
    displayStreamIndex();

    blockApplySettings(false);
}

void NFMDemodGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void NFMDemodGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void NFMDemodGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void NFMDemodGUI::setCtcssFreq(Real ctcssFreq)
{
	if (ctcssFreq == 0) {
		ui->ctcssText->setText("--");
	} else {
		ui->ctcssText->setText(QString("%1").arg(ctcssFreq));
	}
}

void NFMDemodGUI::setDcsCode(unsigned int dcsCode)
{
	if (dcsCode == 0)
    {
		ui->dcsText->setText("--");
	}
    else
    {
        unsigned int normalizedCode;
        bool showPositive = ui->dcsPositive->isChecked();
        normalizedCode = DCSCodes::m_toCanonicalCode[dcsCode];
        normalizedCode = showPositive ? normalizedCode : DCSCodes::m_signFlip[normalizedCode];
		ui->dcsText->setText(tr("%1").arg(normalizedCode, 3, 8, QLatin1Char('0')));
	}
}

void NFMDemodGUI::blockApplySettings(bool block)
{
	m_doApplySettings = !block;
}

void NFMDemodGUI::audioSelect()
{
    qDebug("NFMDemodGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void NFMDemodGUI::tick()
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

    int audioSampleRate = m_nfmDemod->getAudioSampleRate();
    bool squelchOpen = m_nfmDemod->getSquelchOpen();

	if ((audioSampleRate != m_audioSampleRate) || (squelchOpen != m_squelchOpen))
	{
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
			ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
		} else {
			ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}

        m_audioSampleRate = audioSampleRate;
        m_squelchOpen = squelchOpen;
	}

	m_tickCount++;
}
