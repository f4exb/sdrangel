#include "geoscandemodgui.h"
#include "ui_geoscandemodsettings.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDataStream>
#include <QIODevice>
#include <cmath>

GeoscanDemodGUI::GeoscanDemodGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget *parent) :
    ChannelGUI(parent), 
    m_deviceUISet(deviceUISet),
    m_geoscanDemod(nullptr),
    m_updateTimer(new QTimer(this)),
    m_udpSocket(new QUdpSocket(this)),
    m_channelMarker(new ChannelMarker(this))
{
    ui = new Ui::GeoscanDemodGUI();
    ui->setupUi(this);

    if (rxChannel) {
        m_geoscanDemod = dynamic_cast<GeoscanDemod*>(rxChannel);
    }

    connect(m_updateTimer, &QTimer::timeout, this, &GeoscanDemodGUI::on_timer_timeout);
    m_updateTimer->start(200);

    // Подключение сигналов
    connect(ui->deltaFSlider, &QSlider::valueChanged, this, &GeoscanDemodGUI::on_deltaF_valueChanged);
    connect(ui->bwSlider, &QSlider::valueChanged, this, &GeoscanDemodGUI::on_bw_valueChanged);
    connect(ui->devSlider, &QSlider::valueChanged, this, &GeoscanDemodGUI::on_dev_valueChanged);
    connect(ui->thresholdSlider, &QSlider::valueChanged, this, &GeoscanDemodGUI::on_threshold_valueChanged);
    connect(ui->udpAddressEdit, &QLineEdit::textChanged, this, &GeoscanDemodGUI::on_udpAddress_textChanged);
    connect(ui->udpPortSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &GeoscanDemodGUI::on_udpPort_valueChanged);
    connect(ui->decoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeoscanDemodGUI::on_decoder_currentIndexChanged);
    connect(ui->tleFileButton, &QPushButton::clicked, this, &GeoscanDemodGUI::on_tleFileButton_clicked);
    connect(ui->satelliteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeoscanDemodGUI::on_satelliteCombo_currentIndexChanged);
    connect(ui->dopplerCheck, &QCheckBox::toggled, this, &GeoscanDemodGUI::on_dopplerEnable_toggled);
    connect(ui->udpFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeoscanDemodGUI::on_udpFormat_currentIndexChanged);
    connect(ui->startButton, &QPushButton::clicked, this, &GeoscanDemodGUI::on_startDecoderButton_clicked);
    connect(ui->stopButton, &QPushButton::clicked, this, &GeoscanDemodGUI::on_stopDecoderButton_clicked);

    displaySettings(m_settings, true);
}

GeoscanDemodGUI::~GeoscanDemodGUI()
{
    delete m_channelMarker;
    delete ui;
}

void GeoscanDemodGUI::displaySettings(const GeoscanSettings& settings, bool force)
{
    if (force || (m_settings.deltaF != settings.deltaF))
        ui->deltaFSlider->setValue(qRound(settings.deltaF * 100));
    if (force || (m_settings.bw != settings.bw))
        ui->bwSlider->setValue(qRound(settings.bw));
    if (force || (m_settings.dev != settings.dev))
        ui->devSlider->setValue(qRound(settings.dev));
    if (force || (m_settings.threshold != settings.threshold))
        ui->thresholdSlider->setValue(settings.threshold);
    if (force || (m_settings.udpAddress != settings.udpAddress))
        ui->udpAddressEdit->setText(settings.udpAddress);
    if (force || (m_settings.udpPort != settings.udpPort))
        ui->udpPortSpin->setValue(settings.udpPort);
    if (force || (m_settings.decoder != settings.decoder))
        ui->decoderCombo->setCurrentIndex(settings.decoder);

    m_settings = settings;
    updateDecoderPanel(m_settings.decoder);
}

void GeoscanDemodGUI::updateSettings() { applySettingsToDemod(); }

void GeoscanDemodGUI::applySettingsToDemod()
{
    if (!m_geoscanDemod) return;
    auto* msg = GeoscanDemod::MsgConfigureGeoscan::create(m_settings);
    m_geoscanDemod->pushMessage(msg);
}

// === Слоты (принимают int, как требует QSlider) ===
void GeoscanDemodGUI::on_deltaF_valueChanged(int value)
{
    if (m_dopplerEnabled) return;
    m_settings.deltaF = value / 100.0f;
    ui->deltaFValue->setText(QString("%1 Hz").arg(m_settings.deltaF, 0, 'f', 1));
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_bw_valueChanged(int value)
{
    m_settings.bw = value;
    ui->bwValue->setText(QString("%1").arg(m_settings.bw, 0, 'f', 0));
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_dev_valueChanged(int value)
{
    m_settings.dev = value;
    ui->devValue->setText(QString("%1").arg(m_settings.dev, 0, 'f', 0));
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_threshold_valueChanged(int value)
{
    m_settings.threshold = value;
    ui->thValue->setText(QString("%1").arg(m_settings.threshold));
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_udpAddress_textChanged(const QString& text) {
    m_settings.udpAddress = text; applySettingsToDemod();
}
void GeoscanDemodGUI::on_udpPort_valueChanged(int value) {
    m_settings.udpPort = value; applySettingsToDemod();
}
void GeoscanDemodGUI::on_decoder_currentIndexChanged(int index) {
    m_settings.decoder = index;
    updateDecoderPanel(index);
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_tleFileButton_clicked()
{
    // Исправлено: getOpenFileName вместо несуществующего getOpenFileLabel
    QString filePath = QFileDialog::getOpenFileName(this, "Выберите TLE файл", "", "TLE Files (*.tle *.txt)");
    if (!filePath.isEmpty()) {
        parseTLEFile(filePath);
        // Исправлено: fileName() вместо fileLabel()
        ui->tleFileName->setText(QFileInfo(filePath).fileName());
    }
}

void GeoscanDemodGUI::parseTLEFile(const QString& filePath)
{
    m_tleList.clear();
    ui->satelliteCombo->clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine().trimmed());

    for (int i = 0; i < lines.size() - 2; i += 3) {
        if (lines[i].startsWith("1") && lines[i+1].startsWith("2")) {
            TLEData tle;
            // Исправлено: поле называется name, а не Label
            tle.name = lines[i-1].isEmpty() ? "Satellite" : lines[i-1];
            m_tleList.append(tle);
            ui->satelliteCombo->addItem(tle.name);
        }
    }
}

void GeoscanDemodGUI::on_satelliteCombo_currentIndexChanged(int index)
{
    if (index >= 0 && index < m_tleList.size() && m_dopplerEnabled) {
        // Пересчёт доплера
    }
}

void GeoscanDemodGUI::on_dopplerEnable_toggled(bool checked)
{
    m_dopplerEnabled = checked;
    ui->deltaFSlider->setEnabled(!checked);
    ui->deltaFValue->setEnabled(!checked);
    if (checked && !m_tleList.isEmpty()) applySettingsToDemod();
}

void GeoscanDemodGUI::on_udpFormat_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    applySettingsToDemod();
}

void GeoscanDemodGUI::on_startDecoderButton_clicked() {
    QMessageBox::information(this, "Декодер", "Декодер запущен");
}
void GeoscanDemodGUI::on_stopDecoderButton_clicked() {
    QMessageBox::information(this, "Декодер", "Декодер остановлен");
}

void GeoscanDemodGUI::on_timer_timeout()
{
    if (m_geoscanDemod) {
        float snr = m_geoscanDemod->getSnr();
        ui->snrLabel->setText(QString("SNR: %1 dB").arg(snr, 0, 'f', 1));
    }
}

// Исправлено: используем готовый QStackedWidget из .ui файла
void GeoscanDemodGUI::updateDecoderPanel(int decoderIndex)
{
    ui->decoderStack->setCurrentIndex(decoderIndex);
}

void GeoscanDemodGUI::sendPacketToUDP(const QByteArray& packet)
{
    if (m_udpSocket && m_settings.udpPort > 0) {
        m_udpSocket->writeDatagram(packet, QHostAddress(m_settings.udpAddress), m_settings.udpPort);
    }
}

void GeoscanDemodGUI::resetToDefaults()
{
    m_settings = GeoscanSettings();
    displaySettings(m_settings, true); // Исправлено: передача структуры настроек
    applySettingsToDemod();
}

QByteArray GeoscanDemodGUI::serialize() const
{
    QByteArray ba;
    // Исправлено: явное указание устройства и режима записи
    QDataStream stream(&ba, QIODevice::WriteOnly);
    stream << m_settings.deltaF << m_settings.bw << m_settings.dev
           << m_settings.threshold << m_settings.udpAddress << m_settings.udpPort
           << m_settings.decoder;
    return ba;
}

bool GeoscanDemodGUI::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    if (stream.status() != QDataStream::Ok) return false;
    stream >> m_settings.deltaF >> m_settings.bw >> m_settings.dev
           >> m_settings.threshold >> m_settings.udpAddress >> m_settings.udpPort
           >> m_settings.decoder;
    displaySettings(m_settings, true);
    applySettingsToDemod();
    return true;
}
