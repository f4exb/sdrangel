#include <QColorDialog>

#include "dsp/channelmarker.h"
#include "gui/pluginpresetsdialog.h"
#include "gui/dialogpositioner.h"
#include "channel/channelapi.h"
#include "channel/channelgui.h"
#include "maincore.h"

#include "basicchannelsettingsdialog.h"
#include "ui_basicchannelsettingsdialog.h"

BasicChannelSettingsDialog::BasicChannelSettingsDialog(ChannelMarker* marker, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BasicChannelSettingsDialog),
    m_channelMarker(marker),
    m_hasChanged(false)
{
    ui->setupUi(this);
    ui->title->setText(m_channelMarker->getTitle());
    m_color = m_channelMarker->getColor();
    ui->fScaleDisplayType->setCurrentIndex((int) m_channelMarker->getFrequencyScaleDisplayType());
    setUseReverseAPI(false);
    setReverseAPIAddress("127.0.0.1");
    setReverseAPIPort(8888);
    setReverseAPIDeviceIndex(0);
    setReverseAPIChannelIndex(0);
    paintColor();
}

BasicChannelSettingsDialog::~BasicChannelSettingsDialog()
{
    delete ui;
}

void BasicChannelSettingsDialog::setUseReverseAPI(bool useReverseAPI)
{
    m_useReverseAPI = useReverseAPI;
    ui->reverseAPI->setChecked(m_useReverseAPI);
}

void BasicChannelSettingsDialog::setReverseAPIAddress(const QString& address)
{
    m_reverseAPIAddress = address;
    ui->reverseAPIAddress->setText(m_reverseAPIAddress);
}

void BasicChannelSettingsDialog::setReverseAPIPort(uint16_t port)
{
    if (port < 1024) {
        return;
    } else {
        m_reverseAPIPort = port;
    }

    ui->reverseAPIPort->setText(tr("%1").arg(m_reverseAPIPort));
}

void BasicChannelSettingsDialog::setReverseAPIDeviceIndex(uint16_t deviceIndex)
{
    m_reverseAPIDeviceIndex = deviceIndex > 99 ? 99 : deviceIndex;
    ui->reverseAPIDeviceIndex->setText(tr("%1").arg(m_reverseAPIDeviceIndex));
}

void BasicChannelSettingsDialog::setReverseAPIChannelIndex(uint16_t channelIndex)
{
    m_reverseAPIChannelIndex = channelIndex > 99 ? 99 : channelIndex;
    ui->reverseAPIChannelIndex->setText(tr("%1").arg(m_reverseAPIChannelIndex));
}

void BasicChannelSettingsDialog::setNumberOfStreams(int numberOfStreams)
{
    ui->streamIndex->setMaximum(numberOfStreams - 1);
    ui->streamIndex->setEnabled(true);
}

void BasicChannelSettingsDialog::setStreamIndex(int index)
{
    m_streamIndex = index;
    ui->streamIndex->setValue(index);
}

void BasicChannelSettingsDialog::paintColor()
{
    QPixmap pm(24, 24);
    pm.fill(m_color);
    ui->colorBtn->setIcon(pm);
    ui->colorText->setText(tr("#%1%2%3")
        .arg(m_color.red(), 2, 16, QChar('0'))
        .arg(m_color.green(), 2, 16, QChar('0'))
        .arg(m_color.blue(), 2, 16, QChar('0')));
}

void BasicChannelSettingsDialog::on_colorBtn_clicked()
{
    QColor c = m_color;
    c = QColorDialog::getColor(c, this, tr("Select Color for Channel"), QColorDialog::DontUseNativeDialog);

    if (c.isValid())
    {
        m_color = c;
        paintColor();
    }
}

void BasicChannelSettingsDialog::on_reverseAPI_toggled(bool checked)
{
    m_useReverseAPI = checked;
}

void BasicChannelSettingsDialog::on_reverseAPIAddress_editingFinished()
{
    m_reverseAPIAddress = ui->reverseAPIAddress->text();
}

void BasicChannelSettingsDialog::on_reverseAPIPort_editingFinished()
{
    bool dataOk;
    int reverseAPIPort = ui->reverseAPIPort->text().toInt(&dataOk);

    if((!dataOk) || (reverseAPIPort < 1024) || (reverseAPIPort > 65535)) {
        return;
    } else {
        m_reverseAPIPort = reverseAPIPort;
    }
}

void BasicChannelSettingsDialog::on_reverseAPIDeviceIndex_editingFinished()
{
    bool dataOk;
    int reverseAPIDeviceIndex = ui->reverseAPIDeviceIndex->text().toInt(&dataOk);

    if ((!dataOk) || (reverseAPIDeviceIndex < 0)) {
        return;
    } else {
        m_reverseAPIDeviceIndex = reverseAPIDeviceIndex;
    }
}

void BasicChannelSettingsDialog::on_reverseAPIChannelIndex_editingFinished()
{
    bool dataOk;
    int reverseAPIChannelIndex = ui->reverseAPIChannelIndex->text().toInt(&dataOk);

    if ((!dataOk) || (reverseAPIChannelIndex < 0)) {
        return;
    } else {
        m_reverseAPIChannelIndex = reverseAPIChannelIndex;
    }
}

void BasicChannelSettingsDialog::on_streamIndex_valueChanged(int value)
{
    m_streamIndex = value;
}

void BasicChannelSettingsDialog::on_titleReset_clicked()
{
    ui->title->setText(m_defaultTitle);
}

void BasicChannelSettingsDialog::on_presets_clicked()
{
    ChannelGUI *channelGUI = qobject_cast<ChannelGUI *>(parent());
    if (!channelGUI)
    {
        qDebug() << "BasicChannelSettingsDialog::on_presets_clicked: parent not a ChannelGUI";
        return;
    }
    ChannelAPI *channel = MainCore::instance()->getChannel(channelGUI->getDeviceSetIndex(), channelGUI->getIndex());
    const QString& id = channel->getURI();

    PluginPresetsDialog dialog(id);
    dialog.setPresets(MainCore::instance()->getMutableSettings().getPluginPresets());
    dialog.setSerializableInterface(channelGUI);
    dialog.populateTree();
    new DialogPositioner(&dialog, true);
    dialog.exec();
    if (dialog.wasPresetLoaded()) {
        QDialog::reject(); // Settings may have changed, so GUI will be inconsistent. Just close it
    }
}

void BasicChannelSettingsDialog::accept()
{
    m_channelMarker->blockSignals(true);
    m_channelMarker->setTitle(ui->title->text());

    if(m_color.isValid()) {
        m_channelMarker->setColor(m_color);
    }

    m_channelMarker->setFrequencyScaleDisplayType((ChannelMarker::frequencyScaleDisplay_t) ui->fScaleDisplayType->currentIndex());
    m_channelMarker->blockSignals(false);

    m_hasChanged = true;
    QDialog::accept();
}
