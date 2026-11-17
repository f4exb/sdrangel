///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextDocument>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "radiopagingdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "ui_radiopagingdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/flowlayout.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/pagingcomposer.h"
#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "maincore.h"
#include "util/csv.h"
#include "util/units.h"
#include "gui/messagedialog.h"
#include "SWGMapItem.h"

#include "radiopagingdemod.h"
#include "radiopagingdemodnotificationdialog.h"
#include "radiopagingpagerformat.h"
#include "radiopagingvoicestore.h"

const int RadioPagingDemodGUI::m_maxRows;

RadioPagingDemodGUI* RadioPagingDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    RadioPagingDemodGUI* gui = new RadioPagingDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void RadioPagingDemodGUI::destroy()
{
    delete this;
}

void RadioPagingDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray RadioPagingDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadioPagingDemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool RadioPagingDemodGUI::handleMessage(const Message& message)
{
    if (RadioPagingDemod::MsgConfigureRadioPagingDemod::match(message))
    {
        qDebug("RadioPagingDemodGUI::handleMessage: RadioPagingDemod::MsgConfigureRadioPagingDemod");
        const RadioPagingDemod::MsgConfigureRadioPagingDemod& cfg = (const RadioPagingDemod::MsgConfigureRadioPagingDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (MainCore::MsgMessageRecord::match(message))
    {
        const MainCore::MsgMessageRecord& msg = (const MainCore::MsgMessageRecord&) message;
        showRecord(msg.getRecord());
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        // The dial clamps its value to a new range and signals it as a change: the range of a sample rate that is
        // not yet the device's (a configuration being loaded, a file not yet opened) would replace the offset. The
        // offset stays in the settings; the dial shows it within the range
        ui->deltaFrequency->blockSignals(true);
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
        ui->deltaFrequency->blockSignals(false);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void RadioPagingDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void RadioPagingDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void RadioPagingDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void RadioPagingDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void RadioPagingDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings(QStringList("rfBandwidth"));
}

void RadioPagingDemodGUI::on_pocsagDeviation_valueChanged(int value)
{
    m_settings.m_pocsagDeviation = value;
    applySettings(QStringList("pocsagDeviation"));
}

void RadioPagingDemodGUI::on_flexMaxMessageNumber_valueChanged(int value)
{
    m_settings.m_flexMaxMessageNumber = value;
    applySettings(QStringList("flexMaxMessageNumber"));
}

void RadioPagingDemodGUI::on_flexDeviation_valueChanged(int value)
{
    m_settings.m_flexDeviation = value;
    applySettings(QStringList("flexDeviation"));
}

void RadioPagingDemodGUI::on_pocsagEnabled_clicked(bool checked)
{
    m_settings.m_pocsagEnabled = checked;
    applySettings(QStringList("pocsagEnabled"));
}

void RadioPagingDemodGUI::on_flexEnabled_clicked(bool checked)
{
    m_settings.m_flexEnabled = checked;
    applySettings(QStringList("flexEnabled"));
}

void RadioPagingDemodGUI::on_gscEnabled_clicked(bool checked)
{
    m_settings.m_gscEnabled = checked;
    applySettings(QStringList("gscEnabled"));
}

void RadioPagingDemodGUI::on_gscVoiceTimeout_valueChanged(int value)
{
    m_settings.m_gscVoiceTimeout = value;
    applySettings(QStringList("gscVoiceTimeout"));
}

void RadioPagingDemodGUI::on_gscDeviation_valueChanged(int value)
{
    m_settings.m_gscDeviation = value;
    applySettings(QStringList("gscDeviation"));
}

void RadioPagingDemodGUI::on_gscAudio_toggled(bool checked)
{
    m_settings.m_gscAudio = checked;
    applySettings(QStringList("gscAudio"));
}

void RadioPagingDemodGUI::on_gscVoiceRecord_toggled(bool checked)
{
    m_settings.m_gscVoiceRecord = checked;
    applySettings(QStringList("gscVoiceRecord"));
}

namespace {

QString sizeText(qint64 bytes)
{
    return RadioPagingVoiceStore::sizeText(bytes);
}

} // namespace

// The voice files: the statistics of the directory, the files kept, the deletion of the old ones
void RadioPagingDemodGUI::on_gscVoiceFiles_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Voice files");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout();
    QSpinBox *days = new QSpinBox(&dialog);
    QSpinBox *minimum = new QSpinBox(&dialog);
    QLabel *stats = new QLabel(&dialog);
    QPushButton *clean = new QPushButton("Delete the old files...", &dialog);
    QPushButton *close = new QPushButton("Close", &dialog);

    days->setRange(0, 3650);
    days->setValue(m_settings.m_gscVoiceKeepDays);
    days->setSpecialValueText("Keep all");
    days->setSuffix(" days");
    days->setToolTip("Files older than this are deleted, a few at a time, when a new voice file is written.\n"
        "Keep all: no file is deleted for its age");
    minimum->setRange(0, 100000);
    minimum->setValue(m_settings.m_gscVoiceKeepMin);
    minimum->setToolTip("The newest files kept whatever their age");
    form->addRow("Delete after", days);
    form->addRow("Always keep the newest", minimum);
    layout->addLayout(form);
    layout->addWidget(stats);
    layout->addWidget(clean);
    layout->addWidget(close);

    auto refresh = [this, &dialog, days, minimum, stats, clean]()
    {
        RadioPagingVoiceStore::Stats s = RadioPagingVoiceStore::stats(m_settings.voiceDirectory(), days->value(), minimum->value());
        stats->setText(QString("Directory: %1\nFiles: %2, %3\nOldest: %4\nNewest: %5\nTo delete now: %6 files, %7\nFree space on the disk: %8")
            .arg(m_settings.voiceDirectory())
            .arg(s.m_files).arg(sizeText(s.m_bytes))
            .arg(s.m_oldest.isValid() ? s.m_oldest.toString("yyyy-MM-dd HH:mm") : QString("-"))
            .arg(s.m_newest.isValid() ? s.m_newest.toString("yyyy-MM-dd HH:mm") : QString("-"))
            .arg(s.m_expired).arg(sizeText(s.m_expiredBytes))
            .arg(sizeText(s.m_freeBytes)));
        clean->setEnabled(s.m_expired > 0);
        (void) dialog;
    };

    QObject::connect(days, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [this, refresh](int value) {
        m_settings.m_gscVoiceKeepDays = value;
        applySettings(QStringList("gscVoiceKeepDays"));
        refresh();
    });
    QObject::connect(minimum, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [this, refresh](int value) {
        m_settings.m_gscVoiceKeepMin = value;
        applySettings(QStringList("gscVoiceKeepMin"));
        refresh();
    });
    QObject::connect(clean, &QPushButton::clicked, &dialog, [this, &dialog, refresh]()
    {
        RadioPagingVoiceStore::Stats s = RadioPagingVoiceStore::stats(m_settings.voiceDirectory(),
            m_settings.m_gscVoiceKeepDays, m_settings.m_gscVoiceKeepMin);

        if (QMessageBox::question(&dialog, "Delete the old files",
            QString("Delete %1 voice files (%2) from\n%3 ?\nThis cannot be undone.").arg(s.m_expired).arg(sizeText(s.m_expiredBytes))
            .arg(m_settings.voiceDirectory())) == QMessageBox::Yes)
        {
            RadioPagingVoiceStore::cleanup(m_settings.voiceDirectory(), m_settings.m_gscVoiceKeepDays, m_settings.m_gscVoiceKeepMin, -1);
        }

        refresh();
    });
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

    refresh();
    dialog.exec();
}

void RadioPagingDemodGUI::voiceDirectorySelect(const QPoint& p)
{
    (void) p;
    QString directory = QFileDialog::getExistingDirectory(this, "Directory of the voice files", m_settings.voiceDirectory());

    if (!directory.isEmpty())
    {
        m_settings.m_gscVoiceDirectory = directory;
        displayVoiceRecordToolTip();
        applySettings(QStringList("gscVoiceDirectory"));
    }
}

void RadioPagingDemodGUI::displayVoiceRecordToolTip()
{
    ui->gscVoiceRecord->setToolTip(QString("Left: record the voice message of GSC voice pages to a WAV file;\n"
        "its path is in the message record (voice_file).\n"
        "Right: select the directory of the files, used from the next voice message. Now: %1")
        .arg(m_settings.voiceDirectory()));
}

void RadioPagingDemodGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString::number(value));
    m_settings.m_volume = value / 100.0f;
    applySettings(QStringList("volume"));
}

void RadioPagingDemodGUI::audioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        ui->messages->setAudioDevice(m_settings.m_audioDeviceName);
        applySettings(QStringList("audioDeviceName"));
    }
}

void RadioPagingDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
    clearFromMap();
}

void RadioPagingDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings(QStringList("udpEnabled"));
}

void RadioPagingDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings(QStringList("udpAddress"));
}

void RadioPagingDemodGUI::on_udpPort_valueChanged(int value)
{
    m_settings.m_udpPort = value;
    applySettings(QStringList("udpPort"));
}

void RadioPagingDemodGUI::on_udpFormat_currentIndexChanged(int index)
{
    m_settings.m_udpFormat = index;
    applySettings(QStringList("udpFormat"));
}

void RadioPagingDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings(QStringList("scopeCh1"));
}

void RadioPagingDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings(QStringList("scopeCh2"));
}

void RadioPagingDemodGUI::on_notifications_clicked()
{
    RadioPagingDemodNotificationDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);

    if (dialog.exec() == QDialog::Accepted)
    {
        enableSpeechIfNeeded();
        applySettings(QStringList("notificationSettings"));
    }
}

void RadioPagingDemodGUI::fillCodepageLists()
{
    QComboBox *combos[3] = {ui->pocsagCharset, ui->flexCharset, ui->gscCharset};
    const char *protocols[3] = {"pocsag", "flex", "gsc"};
    const RadioPagingCharset::Key keys[3] = {m_settings.m_pocsagCharset, m_settings.m_flexCharset, m_settings.m_gscCharset};
    const RadioPagingCharset::Key custom = RadioPagingCustomCodepage::key();

    for (int p = 0; p < 3; p++)
    {
        PagingComposer::fillCodepages(combos[p], protocols[p], false);
        QSignalBlocker blocker(combos[p]);
        combos[p]->addItem(tr("Custom"), QStringList({QString::fromStdString(custom.m_vendor), QString::fromStdString(custom.m_model),
            QString::fromStdString(custom.m_codepage)}));
        PagingComposer::selectCodepage(combos[p], keys[p]);
    }
}

// The custom codepage of a protocol: each code of the default table of the protocol with its character, one for one
void RadioPagingDemodGUI::editCustomCodepage(int protocol)
{
    const char *protocols[3] = {"pocsag", "flex", "gsc"};
    const char *names[3] = {"POCSAG", "FLEX", "GSC"};
    const char *codesKeys[3] = {"pocsagCustomCodes", "flexCustomCodes", "gscCustomCodes"};
    const char *charsetKeys[3] = {"pocsagCharset", "flexCharset", "gscCharset"};
    QString *codes[3] = {&m_settings.m_pocsagCustomCodes, &m_settings.m_flexCustomCodes, &m_settings.m_gscCustomCodes};
    RadioPagingCharset::Key *charsets[3] = {&m_settings.m_pocsagCharset, &m_settings.m_flexCharset, &m_settings.m_gscCharset};
    const RadioPagingCharset::Codepage *base = RadioPagingCharset::defaultFor(protocols[protocol]);

    if (!base) {
        return;
    }

    const int size = base->codes();
    RadioPagingCustomCodepage current(protocols[protocol], codes[protocol]->toStdString());
    QDialog dialog(this);
    dialog.setWindowTitle(tr("%1 custom codepage").arg(names[protocol]));
    dialog.resize(420, 560);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *intro = new QLabel(tr("The character a pager shows for each code. ASCII is the start: change the character "
        "(or its Unicode code point in hex) of the codes the pager shows differently. A code without a character is shown as <0xNN>."),
        &dialog);
    intro->setWordWrap(true);
    QTableWidget *table = new QTableWidget(size, 3, &dialog);
    table->setHorizontalHeaderLabels({tr("Code"), tr("Character"), tr("Unicode")});
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    QCheckBox *rtl = new QCheckBox(tr("Right-to-left: the text is shown in the reverse order of the codes"), &dialog);
    QPushButton *ascii = new QPushButton(tr("ASCII"), &dialog);
    ascii->setToolTip(tr("Every code back to its ASCII character"));
    QPushButton *exportButton = new QPushButton(tr("Export..."), &dialog);
    QPushButton *importButton = new QPushButton(tr("Import..."), &dialog);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QHBoxLayout *tools = new QHBoxLayout();
    tools->addWidget(ascii);
    tools->addWidget(exportButton);
    tools->addWidget(importButton);
    tools->addStretch(1);
    layout->addWidget(intro);
    layout->addWidget(table, 1);
    layout->addWidget(rtl);
    layout->addLayout(tools);
    layout->addWidget(buttons);

    // A row: the character and its code point, from one or the other
    auto setRow = [table](int code, const QString& character) {
        QSignalBlocker blocker(table);
        const QVector<uint> points = character.toUcs4();

        if (!table->item(code, 0))
        {
            QTableWidgetItem *codeItem = new QTableWidgetItem(QString("%1").arg(code, 2, 16, QChar('0')).toUpper());
            codeItem->setFlags(codeItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(code, 0, codeItem);
            table->setItem(code, 1, new QTableWidgetItem());
            table->setItem(code, 2, new QTableWidgetItem());
        }

        table->item(code, 1)->setText(character);
        table->item(code, 2)->setText(points.size() == 1 ? QString("%1").arg(points[0], 4, 16, QChar('0')).toUpper() : QString());
    };
    auto show = [&](const RadioPagingCharset::Codepage& codepage) {
        for (int code = 0; code < size; code++)
        {
            const char *glyph = codepage.glyph(code);
            setRow(code, glyph ? QString::fromUtf8(glyph) : QString());
        }

        rtl->setChecked(codepage.m_rtl);
    };

    show(current.codepage());
    table->resizeColumnsToContents();

    connect(table, &QTableWidget::cellChanged, &dialog, [table, setRow](int row, int column) {
        if (column == 1)
        {
            setRow(row, table->item(row, 1)->text());
        }
        else if (column == 2)
        {
            bool ok;
            const uint point = table->item(row, 2)->text().trimmed().toUInt(&ok, 16);
            setRow(row, (ok && (point > 0) && (point <= 0x10FFFF) && ((point < 0xD800) || (point > 0xDFFF)))
                ? QString::fromUcs4(&point, 1) : QString());
        }
    });
    connect(ascii, &QPushButton::clicked, &dialog, [&]() {
        show(*base);
        rtl->setChecked(false);
    });

    // The codes that are not those of ASCII, and the codes that are not one character
    auto collect = [&](RadioPagingCustomCodepage::Codes& mapped, QStringList& wrong) {
        for (int code = 0; code < size; code++)
        {
            const QString character = table->item(code, 1)->text();
            const char *glyph = base->glyph(code);
            const QVector<uint> points = character.toUcs4();

            if (character == (glyph ? QString::fromUtf8(glyph) : QString())) {
                continue;
            }
            if (points.size() != 1) {
                wrong.append(QString("%1").arg(code, 2, 16, QChar('0')).toUpper());
            } else {
                mapped.push_back(std::make_pair(code, (uint32_t) points[0]));
            }
        }
    };

    // The file: every code with its character (none: empty), the protocol and the direction
    connect(exportButton, &QPushButton::clicked, &dialog, [&]() {
        QString fileName = QFileDialog::getSaveFileName(&dialog, tr("Export the custom codepage"), QString("%1_codepage.json").arg(protocols[protocol]),
            tr("JSON (*.json)"));

        if (fileName.isEmpty()) {
            return;
        }

        QJsonObject map;

        for (int code = 0; code < size; code++) {
            map.insert(table->item(code, 0)->text(), table->item(code, 2)->text());
        }

        QJsonObject json;
        json.insert("protocol", protocols[protocol]);
        json.insert("rtl", rtl->isChecked());
        json.insert("codes", map);
        QFile file(fileName);

        if (!file.open(QIODevice::WriteOnly) || (file.write(QJsonDocument(json).toJson()) < 0)) {
            QMessageBox::warning(&dialog, tr("Export"), tr("%1 cannot be written").arg(fileName));
        }
    });
    connect(importButton, &QPushButton::clicked, &dialog, [&]() {
        QString fileName = QFileDialog::getOpenFileName(&dialog, tr("Import a custom codepage"), QString(), tr("JSON (*.json)"));

        if (fileName.isEmpty()) {
            return;
        }

        QFile file(fileName);
        const QJsonObject json = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();

        if (!json.value("codes").isObject())
        {
            QMessageBox::warning(&dialog, tr("Import"), tr("%1 is not a custom codepage").arg(fileName));
            return;
        }
        if (json.value("protocol").toString() != protocols[protocol]) {
            QMessageBox::information(&dialog, tr("Import"), tr("The codepage is of %1: its codes of %2 are taken")
                .arg(json.value("protocol").toString().toUpper(), names[protocol]));
        }

        const QJsonObject map = json.value("codes").toObject();
        show(*base);

        for (int code = 0; code < size; code++)
        {
            const QString key = table->item(code, 0)->text();

            if (map.contains(key))
            {
                bool ok;
                const uint point = map.value(key).toString().toUInt(&ok, 16);
                setRow(code, (ok && (point > 0) && (point <= 0x10FFFF)) ? QString::fromUcs4(&point, 1) : QString());
            }
        }

        rtl->setChecked(json.value("rtl").toBool());
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        RadioPagingCustomCodepage::Codes mapped;
        QStringList wrong;
        collect(mapped, wrong);

        if (!wrong.isEmpty())
        {
            QMessageBox::warning(&dialog, dialog.windowTitle(), tr("A code is one character: %1").arg(wrong.join(" ")));
            return;
        }

        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // Saved, and chosen: the codepage of the protocol is its custom codepage
    RadioPagingCustomCodepage::Codes mapped;
    QStringList wrong;
    collect(mapped, wrong);
    *codes[protocol] = QString::fromStdString(RadioPagingCustomCodepage::text(mapped, rtl->isChecked()));
    *charsets[protocol] = RadioPagingCustomCodepage::key();
    displaySettings();
    applySettings(QStringList({codesKeys[protocol], charsetKeys[protocol]}));
}

void RadioPagingDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings(QStringList("logEnabled"));
}

void RadioPagingDemodGUI::on_logFilename_clicked()
{
    QFileDialog fileDialog(nullptr, tr("Select the file to log the messages received to"), "", "*.csv");
    fileDialog.setDefaultSuffix("csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(tr(".csv log file: %1").arg(m_settings.m_logFilename));
            applySettings(QStringList("logFilename"));
        }
    }
}

// The messages of a log written by this demodulator, shown in the table. They are not notified again: they were
// when they were received, and running the commands of a long log again could be a lot
void RadioPagingDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, tr("Select the .csv log file to read"), "", "*.csv");

    if (!fileDialog.exec()) {
        return;
    }

    QStringList fileNames = fileDialog.selectedFiles();

    if (fileNames.isEmpty()) {
        return;
    }

    QFile file(fileNames[0]);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        MessageDialog::critical(this, "Radio Paging Demod", tr("Failed to open file %1").arg(fileNames[0]));
        return;
    }

    QTextStream in(&file);
    QString error;
    QHash<QString, int> columns = CSV::readHeader(in, {"id", "timestamp", "protocol", "kind"}, error);

    if (!error.isEmpty())
    {
        MessageDialog::critical(this, "Radio Paging Demod", error);
        return;
    }

    QMessageBox dialog(this);
    dialog.setText(tr("Reading messages"));
    dialog.addButton(QMessageBox::Cancel);
    dialog.show();
    QApplication::processEvents();

    int count = 0;
    bool cancelled = false;
    QStringList row;
    m_loadingData = true;

    while (!cancelled && CSV::readRow(in, &row))
    {
        MessageRecord record;

        if (MessageRecord::fromCSV(columns, row, record) && (record.m_kind == MessageRecord::KindMessage)) {
            showRecord(record);
        }

        if ((++count % 500) == 0)
        {
            QApplication::processEvents();
            cancelled = (dialog.clickedButton() != nullptr);
        }
    }

    m_loadingData = false;
    dialog.close();
}

// The address, or the address of a member of the group of a group message, matches the filter
bool RadioPagingDemodGUI::addressMatchesFilter(const QStringList& addresses) const
{
    QString pattern = ui->filterAddress->text().trimmed();

    if (pattern.isEmpty()) {
        return true;
    }

    // As the address filter of the Pager demodulator: the whole address must match
    QRegularExpression regExp(QRegularExpression::anchoredPattern(pattern));

    if (!regExp.isValid()) {
        return true;
    }

    for (const QString& address : addresses)
    {
        if (regExp.match(address).hasMatch()) {
            return true;
        }
    }

    return false;
}

void RadioPagingDemodGUI::applyAddressFilter()
{
    for (int row = 0; row < ui->messages->rowCount(); row++) {
        ui->messages->setRowHidden(row, !addressMatchesFilter(ui->messages->rowAddresses(row)));
    }
}

void RadioPagingDemodGUI::on_filterAddress_editingFinished()
{
    // An expression that is not valid filters nothing, shown in red with its error
    QRegularExpression regExp(QRegularExpression::anchoredPattern(ui->filterAddress->text().trimmed()));
    bool valid = ui->filterAddress->text().trimmed().isEmpty() || regExp.isValid();
    ui->filterAddress->setStyleSheet(valid ? QString() : QString("QLineEdit { color: red; }"));
    ui->filterAddress->setToolTip(valid ? tr("Show only the messages whose whole address matches this regular expression,\n"
        "e.g. 1234567A, 12345.., 001001/. Empty: all") : regExp.errorString());
    applyAddressFilter();
}

// A message goes in the table; a new one is given to the notifications too
void RadioPagingDemodGUI::showRecord(const MessageRecord& record)
{
    ui->messages->addRecord(record);

    int row = ui->messages->findRow(record.m_id);

    if ((row >= 0) && !addressMatchesFilter(record.addresses())) {
        ui->messages->setRowHidden(row, true);
    }

    if (!m_loadingData) {
        checkNotifications(record);
    }
}

QStringList RadioPagingDemodGUI::matchTexts(const MessageRecord& record, int field)
{
    typedef RadioPagingDemodSettings::NotificationSettings Notification;

    switch (field)
    {
    case Notification::MatchAddress:
        return record.addresses();
    case Notification::MatchMessage:
        return QStringList(record.m_text);
    case Notification::MatchProtocol:
        return QStringList(record.m_protocol);
    case Notification::MatchType:
        return QStringList(record.m_msgType);
    case Notification::MatchChannel:
        return QStringList(record.m_channel);
    case Notification::MatchAny:
        return QStringList((record.addresses() + QStringList({record.m_text, record.m_protocol, record.m_msgType, record.m_channel})).join(' '));
    default:
        return QStringList();
    }
}

// Only a message received for the first time is notified: a duplicate was, and the end of a voice message is not one
void RadioPagingDemodGUI::checkNotifications(const MessageRecord& record)
{
    if ((record.m_kind != MessageRecord::KindMessage) || (record.m_duplicate != MessageRecord::DuplicateNone)
        || RadioPagingPagerFormat::isCompletion(record)) {
        return;
    }

    for (const auto& notification : m_settings.m_notificationSettings)
    {
        if (!notification->m_regularExpression.isValid()) {
            continue;
        }

        // The first text of the field that matches: on the address, a member of a group message too
        QRegularExpressionMatch matchResult;

        for (const QString& match : matchTexts(record, notification->m_matchField))
        {
            if (!match.isEmpty() && (matchResult = notification->m_regularExpression.match(match)).hasMatch()) {
                break;
            }
        }

        if (!matchResult.hasMatch()) {
            continue;
        }

        if (notification->m_highlight)
        {
            QColor color = QColor::fromRgba(notification->m_highlightColor);
            color.setAlpha(110);
            ui->messages->setRowBackground(record.m_id, color);
        }
        if (!notification->m_speech.isEmpty()) {
            speechNotification(subStrings(record, matchResult, notification->m_speech));
        }
        if (!notification->m_command.isEmpty()) {
            commandNotification(subStrings(record, matchResult, notification->m_command));
        }
        if (notification->m_plotOnMap)
        {
            float latitude;
            float longitude;

            if (Units::stringToLatitudeAndLongitude(record.m_text, latitude, longitude, false)) {
                sendToMap(record, latitude, longitude);
            }
        }
    }
}

QString RadioPagingDemodGUI::subStrings(const MessageRecord& record, const QRegularExpressionMatch& match, const QString& string)
{
    QString s = string;

    s.replace("${address}", record.m_address);
    s.replace("${message}", record.m_text);
    s.replace("${protocol}", record.m_protocol);
    s.replace("${type}", record.m_msgType);
    s.replace("${channel}", record.m_channel);

    for (int i = 0; i < match.capturedTexts().size(); i++) {
        s.replace(QString("${%1}").arg(i), match.capturedTexts()[i]);
    }

    return s;
}

// The text to speech engine takes 10 seconds to start on some versions of Linux: only started if a
// notification speaks
void RadioPagingDemodGUI::enableSpeechIfNeeded()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        return;
    }

    for (const auto& notification : m_settings.m_notificationSettings)
    {
        if (!notification->m_speech.isEmpty())
        {
            qDebug() << "RadioPagingDemodGUI: enabling text to speech";
            m_speech = new QTextToSpeech(this);
            return;
        }
    }
#endif
}

void RadioPagingDemodGUI::speechNotification(const QString& speech)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        m_speech->say(speech);
    } else {
        qWarning() << "RadioPagingDemodGUI::speechNotification: unable to say" << speech;
    }
#else
    qWarning() << "RadioPagingDemodGUI::speechNotification: text to speech not supported, unable to say" << speech;
#endif
}

void RadioPagingDemodGUI::commandNotification(const QString& command)
{
#if QT_CONFIG(process)
    QStringList allArgs = QProcess::splitCommand(command);

    if (allArgs.size() > 0)
    {
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
#else
    qWarning() << "RadioPagingDemodGUI::commandNotification: QProcess not supported, can't run" << command;
#endif
}

void RadioPagingDemodGUI::sendToMap(const MessageRecord& record, float latitude, float longitude)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_radioPagingDemod, "mapitems", mapPipes);
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(record.m_timestamp);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(record.m_address));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(0);
        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
        swgMapItem->setFixedPosition(false);
        swgMapItem->setPositionDateTime(new QString(dateTime.toUTC().toString(Qt::ISODateWithMs)));
        swgMapItem->setImageRotation(0);
        swgMapItem->setText(new QString(record.m_text));
        swgMapItem->setImage(new QString("pager.png"));

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_radioPagingDemod, swgMapItem);
        messageQueue->push(msg);
    }

    m_mapItems.insert(record.m_address);
}

void RadioPagingDemodGUI::clearFromMap()
{
    for (const auto& address : m_mapItems)
    {
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_radioPagingDemod, "mapitems", mapPipes);

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(address));
            swgMapItem->setImage(new QString(""));

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_radioPagingDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }

    m_mapItems.clear();
}

void RadioPagingDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    // The waveforms are computed only while they are shown
    if (widget == ui->scopeContainer) {
        m_radioPagingDemod->setScopeEnabled(rollDown);
    }

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void RadioPagingDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_radioPagingDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(QStringList({"title", "rgbColor", "useReverseAPI", "reverseAPIAddress",
            "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "streamIndex"}));
    }

    resetContextMenuType();
}

RadioPagingDemodGUI::RadioPagingDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RadioPagingDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_scopeVis(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodradiopaging/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    // A new window shows a few messages: the table is the part that grows with the window (the waveforms do not).
    // Set before the rollups are arranged, so that the window is sized for it
    ui->messages->setMinimumHeight(ui->messages->horizontalHeader()->sizeHint().height()
        + 6 * ui->messages->verticalHeader()->defaultSectionSize() + 2 * ui->messages->frameWidth());
    // The protocol settings, the details, output and waveforms are rolled up until the user opens them (a state saved
    // before wins)
    ui->protocolsContainer->hide();
    ui->detailsContainer->hide();
    ui->outputContainer->hide();
    ui->scopeContainer->hide();
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_radioPagingDemod = reinterpret_cast<RadioPagingDemod*>(rxChannel);
    m_radioPagingDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_radioPagingDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    m_scopeVis->setLiveRate(RadioPagingDemodSettings::m_channelSampleRate);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setSampleRate(RadioPagingDemodSettings::m_channelSampleRate);

    for (int signal = 0; signal < RadioPagingDemodSink::ScopeSignalCount; signal++)
    {
        ui->channel1->addItem(RadioPagingDemodSink::scopeSignalName(signal));
        ui->channel2->addItem(RadioPagingDemodSink::scopeSignalName(signal));
    }

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(QColor(255, 170, 0));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Radio Paging Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->messages->setMaxRows(m_maxRows);
    ui->messages->setDefaultColumns({PagingMessageTable::ColDate, PagingMessageTable::ColTime, PagingMessageTable::ColProtocol, PagingMessageTable::ColRate,
        PagingMessageTable::ColPolarity, PagingMessageTable::ColAddress, PagingMessageTable::ColType, PagingMessageTable::ColMessage,
        PagingMessageTable::ColFragment, PagingMessageTable::ColInfo, PagingMessageTable::ColErrors});
    connect(ui->messages, &PagingMessageTable::columnsChanged, this, [this]() {
        m_settings.m_messageColumns = ui->messages->columns();
        applySettings(QStringList("messageColumns"));
    });
    // The voice files are played on the output of the voice audio
    connect(ui->messages, &PagingMessageTable::audioDeviceChanged, this, [this](const QString& deviceName) {
        m_settings.m_audioDeviceName = deviceName;
        applySettings(QStringList("audioDeviceName"));
    });

    CRightClickEnabler *audioRightClickEnabler = new CRightClickEnabler(ui->gscAudio);
    connect(audioRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));
    CRightClickEnabler *recordRightClickEnabler = new CRightClickEnabler(ui->gscVoiceRecord);
    connect(recordRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(voiceDirectorySelect(const QPoint &)));
    // The list of the custom POCSAG fill codes
    CRightClickEnabler *fillRightClickEnabler = new CRightClickEnabler(ui->pocsagFill);
    connect(fillRightClickEnabler, &CRightClickEnabler::rightClick, this, [this](const QPoint&) {
        bool ok;
        QString codes = QInputDialog::getText(this, tr("POCSAG fill codes"),
            tr("Hex codes 00 to 7F, separated by spaces or commas"), QLineEdit::Normal, m_settings.m_pocsagFillCodes, &ok);
        std::vector<uint8_t> parsed;

        if (!ok) {
            return;
        }
        if (!RadioPagingDemodSettings::parseFillCodes(codes, parsed))
        {
            QMessageBox::warning(this, tr("POCSAG fill codes"), tr("%1 is not a list of hex codes 00 to 7F").arg(codes));
            return;
        }

        m_settings.m_pocsagFillCodes = codes.trimmed();
        m_settings.m_pocsagFill = RadioPagingDemodSettings::FillCustom;
        displaySettings();
        applySettings(QStringList({"pocsagFill", "pocsagFillCodes"}));
    });

    // The codepages of each protocol and its custom codepage
    fillCodepageLists();

    layoutProtocols();
    m_statusClock.start();

    displaySettings();
    m_radioPagingDemod->setScopeEnabled(!ui->scopeContainer->isHidden());
    makeUIConnections();
    applySettings(QStringList(), true);
    m_resizer.enableChildMouseTracking();
}

RadioPagingDemodGUI::~RadioPagingDemodGUI()
{
    delete ui;
}

void RadioPagingDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadioPagingDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        RadioPagingDemod::MsgConfigureRadioPagingDemod* message =
            RadioPagingDemod::MsgConfigureRadioPagingDemod::create(settingsKeys, m_settings, force);
        m_radioPagingDemod->getInputMessageQueue()->push(message);
    }
}

void RadioPagingDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    // Shown without signalling a change. Before the device has given its sample rate (a configuration being loaded)
    // the range may be narrower than the offset: it is widened to show the offset, and set by the next sample rate
    ui->deltaFrequency->blockSignals(true);
    const qint64 halfRange = std::max<qint64>(m_basebandSampleRate / 2, std::abs((qint64) m_settings.m_inputFrequencyOffset));
    ui->deltaFrequency->setValueRange(false, 7, -halfRange, halfRange);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->deltaFrequency->blockSignals(false);
    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);
    ui->pocsagDeviation->setValue(m_settings.m_pocsagDeviation);
    ui->flexDeviation->setValue(m_settings.m_flexDeviation);
    ui->flexMaxMessageNumber->setValue(m_settings.m_flexMaxMessageNumber);
    PagingComposer::selectCodepage(ui->pocsagCharset, m_settings.m_pocsagCharset);
    ui->pocsagFill->setCurrentIndex(m_settings.m_pocsagFill);
    ui->pocsagFill->setItemData(RadioPagingDemodSettings::FillCustom,
        tr("Custom fill codes: %1 (right-click to edit)").arg(m_settings.m_pocsagFillCodes), Qt::ToolTipRole);
    PagingComposer::selectCodepage(ui->flexCharset, m_settings.m_flexCharset);
    PagingComposer::selectCodepage(ui->gscCharset, m_settings.m_gscCharset);
    ui->pocsagEnabled->setChecked(m_settings.m_pocsagEnabled);
    ui->flexEnabled->setChecked(m_settings.m_flexEnabled);
    ui->gscEnabled->setChecked(m_settings.m_gscEnabled);
    ui->pocsagDcCorrection->setChecked(m_settings.m_pocsagDcCorrection);
    ui->flexDcCorrection->setChecked(m_settings.m_flexDcCorrection);
    ui->gscDcCorrection->setChecked(m_settings.m_gscDcCorrection);
    ui->gscVoiceTimeout->setValue(m_settings.m_gscVoiceTimeout);
    ui->gscDeviation->setValue(m_settings.m_gscDeviation);
    ui->gscAudio->setChecked(m_settings.m_gscAudio);
    ui->gscVoiceRecord->setChecked(m_settings.m_gscVoiceRecord);
    displayVoiceRecordToolTip();
    ui->volume->setValue(std::round(m_settings.m_volume * 100.0f));
    ui->volumeText->setText(QString::number(ui->volume->value()));

    // The columns the user chose; the first time the usual ones
    if (!ui->messages->setColumns(m_settings.m_messageColumns)) {
        ui->messages->sizeColumns();
    }

    ui->messages->setAudioDevice(m_settings.m_audioDeviceName);
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setValue(m_settings.m_udpPort);
    ui->udpFormat->setCurrentIndex(m_settings.m_udpFormat);
    ui->logEnable->setChecked(m_settings.m_logEnabled);
    ui->logFilename->setToolTip(tr(".csv log file: %1").arg(m_settings.m_logFilename));
    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);
    enableSpeechIfNeeded();
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RadioPagingDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RadioPagingDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RadioPagingDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_radioPagingDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0)
    {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
        updateStatus();
    }

    m_tickCount++;
}

QString RadioPagingDemodGUI::groupSlotToolTip(const RadioPagingFlexDecoder::GroupSlot& slot, int pol,
    const RadioPagingDemodSink::Status& status)
{
    static const int shownMembers = 20;
    QStringList members;

    for (size_t i = 0; (i < slot.m_members.size()) && (i < (size_t) shownMembers); i++) {
        members.append(QString::fromStdString(RadioPagingFlex::addressText(slot.m_members[i])));
    }
    if (slot.m_members.size() > (size_t) shownMembers) {
        members.append(tr("and %1 more").arg(slot.m_members.size() - shownMembers));
    }

    QString text = tr("Group slot %1 (temporary address %2), %3 network\nMembers: %4")
        .arg(slot.m_slot).arg(2029568 + slot.m_slot).arg(pol == 1 ? tr("inverted") : tr("normal"))
        .arg(slot.m_members.size());

    if (!members.isEmpty()) {
        text += "\n" + members.join("\n");
    }

    text += tr("\nGroup message in frame %1").arg(slot.m_targetFrame, 3, 10, QChar('0'));
    text += tr("\nAssigned in cycle %1 frame %2").arg(slot.m_setupCycle, 2, 10, QChar('0')).arg(slot.m_setupFrame, 3, 10, QChar('0'));

    const RadioPagingFlexDecoder::FrameReport& frame = status.m_flexFramePol[pol];

    if (frame.m_fiwValid)
    {
        int framesToGo = ((int) slot.m_targetFrame - (int) frame.m_frame + 128) % 128;
        text += tr("\nLast frame received: %1, the group message is %2 frames on").arg(frame.m_frame, 3, 10, QChar('0')).arg(framesToGo);
    }

    return text;
}

// The colours of the status. Grey is what is not active: a reception that ended, a slot that is free
namespace {

const char *colourInactive = "#a8a8a8";     // Values of a reception that ended: readable on the dark background
const char *colourOff = "rgb(79,79,79)";    // An indicator off, as in the other channels (NFM, BFM)
const char *colourAssumed = "rgb(76,84,76)"; // FLEX time of an assumed kind: the grey of off with a green hue

// The boxes of the status (bit error rate, phases, polarity): light tints with dark text, for contrast
enum Chip { ChipGood, ChipCorrected, ChipBad, ChipEnded, ChipNormal, ChipInverted };

const char *chipBackground(Chip chip)
{
    static const char *colours[] = {"#a8e0a8", "#f0dc8c", "#f4aca4", "#b4b4b4", "#a8c8ec", "#f4d070"};
    return colours[chip];
}

const char *chipText = "#141414";
const char lowTrafficSeparator = '`';       // FLEX phase box: A`0.00% when the low traffic flag of the phase is set

// The quality of a bit error rate window as a box: errors the code could not correct, errors corrected, none
Chip qualityChip(const RadioPagingBerWindow& ber, bool live)
{
    return !live ? ChipEnded : (ber.m_uncorrectable > 0) ? ChipBad : (ber.m_errorBits > 0) ? ChipCorrected : ChipGood;
}

QString chipStyle(Chip chip)
{
    return QString("QLabel { background-color : %1; color : %2; border-radius : 3px; }").arg(chipBackground(chip)).arg(chipText);
}

QString chipHtml(Chip chip, const QString& text)
{
    return QString("<span style=\"background-color:%1; color:%2;\">&nbsp;%3&nbsp;</span>")
        .arg(chipBackground(chip)).arg(chipText).arg(text.toHtmlEscaped());
}

// The FLEX network date and the time of the last frame as received; empty when the network sends none
QString flexDateTimeText(const RadioPagingFlexDecoder::NetworkTime& t)
{
    QStringList when;

    if (t.m_dateValid) {
        when.append(QString("%1-%2-%3").arg(t.m_year).arg(t.m_month, 2, 10, QChar('0')).arg(t.m_day, 2, 10, QChar('0')));
    }
    if (t.m_timeValid)
    {
        when.append(QString("%1:%2:%3").arg(t.m_currentHour, 2, 10, QChar('0')).arg(t.m_currentMinute, 2, 10, QChar('0'))
            .arg((int) t.m_currentSecond, 2, 10, QChar('0')));
    }

    return when.join(" ");
}

// The FLEX time zone word (STD-43A 3.7.2): the offset from UTC, DST in daylight saving time (L0 = 0), the extended
// second (S5-S3, steps of 1/64 minute); empty when the network sends none
QString flexZoneText(const RadioPagingFlexDecoder::NetworkTime& t)
{
    if (!t.m_zoneValid) {
        return QString();
    }

    int minutes = t.m_zoneMinutes;
    QString zone = t.m_zoneDefined
        ? QString("UTC%1%2:%3").arg(minutes < 0 ? "-" : "+").arg(std::abs(minutes) / 60, 2, 10, QChar('0'))
            .arg(std::abs(minutes) % 60, 2, 10, QChar('0'))
        : QString("zone %1 (not defined)").arg(t.m_zone);

    if (t.m_dst == 0) {
        zone += " DST";
    }
    zone += QString(" ext +%1 s").arg(t.m_extendedSeconds * 0.9375, 0, 'f', 4);

    return zone;
}

// The FLEX network time as received: date, time and zone
QString flexTimeText(const RadioPagingFlexDecoder::NetworkTime& t)
{
    QStringList parts;

    for (const QString& part : {flexDateTimeText(t), flexZoneText(t)})
    {
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }

    return parts.join(" ");
}

// The colours of the group slots: their state, a white digit on a muted background (free: grey digit)
struct SlotStyle
{
    const char *m_background;
    const char *m_text;
};

SlotStyle slotStyle(RadioPagingFlexDecoder::GroupState state)
{
    switch (state)
    {
    case RadioPagingFlexDecoder::GroupAssigning: return {"#2f6fa8", "#ffffff"};
    case RadioPagingFlexDecoder::GroupWaiting: return {"#7a6522", "#ffffff"};
    case RadioPagingFlexDecoder::GroupDelivering: return {"#1f736e", "#ffffff"};
    case RadioPagingFlexDecoder::GroupDelivered: return {"#2e6e33", "#ffffff"};
    case RadioPagingFlexDecoder::GroupMissed: return {"#87392d", "#ffffff"};
    case RadioPagingFlexDecoder::GroupDiscarded: return {"#5a4785", "#ffffff"};
    case RadioPagingFlexDecoder::GroupTimeout: return {"#8f5523", "#ffffff"};
    default: return {nullptr, "#a8a8a8"};     // Free: readable on the dark background
    }
}

} // namespace

namespace {

// A row of the details of a protocol: its name, what it is, and whether it is a value of the protocol (across both
// network columns) rather than of each network
struct DetailRow
{
    const char *m_name;
    const char *m_toolTip;
    bool m_single;
};

// The bit error rate and the carrier offset are in the status row of each protocol (of the network being received)
enum PocsagDetail { PocsagSpeeds, PocsagBatches, PocsagRemembered, PocsagMessages, PocsagReset };
enum FlexDetail { FlexMode, FlexFrame, FlexFlags, FlexRoaming, FlexTime, FlexZone, FlexSystem, FlexCountry, FlexPhasesAB,
    FlexPhasesCD, FlexRemembered, FlexReassembling, FlexErs, FlexMessages, FlexReset };
enum GscDetail { GscState, GscPage, GscRemembered, GscMessages, GscReset };

const DetailRow berRow = {"Bit error rate",
    "How clean the signal is: the share of the received bits that were wrong.\n"
    "Measured by the error correction: each codeword it corrects tells how many bits were wrong; a codeword\n"
    "too damaged to correct counts as 3 wrong bits. Over the last 64 codewords, halved each time the window\n"
    "fills, so that the recent codewords count most.\n"
    "The code corrects 2 wrong bits in a codeword of 32 (POCSAG, FLEX: BCH 31,21) or 3 in 23 (GSC: Golay),\n"
    "so a small rate still decodes every message. For errors spread at random, at 1% about 1 codeword in 250\n"
    "cannot be corrected, at 3% about 1 in 14: below 1% is a good signal, above 3% messages get damaged.\n"
    "Green: no bit errors. Yellow: errors, all corrected. Red: codewords too damaged to correct (their\n"
    "characters may be wrong or lost). Grey once the reception ended.", false};
const DetailRow offsetRow = {"Carrier offset",
    "How far the transmitter is from the frequency of the channel, measured on the known patterns of the protocol.\n"
    "Positive: the signal is above the channel. The receiver follows it; a large offset is worth correcting\n"
    "with the frequency of the channel (Settings).", false};
const DetailRow rememberedRow = {"Dedup memory",
    "Deduplication memory: the messages the receivers keep to recognise one received again (a duplicate:\n"
    "networks repeat pages). A duplicate is marked in the messages, not taken as a new message.\n"
    "A message stays in this memory for 60 s in POCSAG and GSC, and for 30 minutes in FLEX (with its fragments,\n"
    "groups and message numbers): 0 a minute after the last POCSAG or GSC message is normal.", false};
const DetailRow messagesRow = {"Messages", "Messages received since the channel started.", true};
const DetailRow resetRow = {"Last reset",
    "When the receivers last forgot what they remember: the duplicates, and for FLEX the fragments, groups\n"
    "and message numbers. Done with the reset button in Protocols, and when the device starts.", true};

const QList<DetailRow>& detailRows(int protocol)
{
    static const QList<DetailRow> pocsag = {
        {"Speeds",
            "Speeds in sync (bit/s: 512, 1200, 2400). A network sends at one speed; several at once means\n"
            "several transmitters or a mixed network.", false},
        {"Batches",
            "Batches of the transmission being received, or of the last one. A batch is a sync word and 16\n"
            "codewords (8 frames of 2): about 0.45 s at 1200 bit/s. A long transmission carries many pages.", false},
        rememberedRow, messagesRow, resetRow
    };
    static const QList<DetailRow> flex = {
        {"Mode",
            "Speed and levels of the last frame: bit/s / number of frequency levels.\n"
            "A frame carries 1, 2 or 4 phases: independent 1600 bit/s channels, each with its own addresses\n"
            "and messages, sent together. More phases carry more pages in the same time.\n"
            "  1600/2: 1 phase (A); 2 levels (+-4.8 kHz), 1600 symbols/s of 1 bit.\n"
            "  3200/2: 2 phases (A, C); 2 levels, 3200 symbols/s of 1 bit.\n"
            "  3200/4: 2 phases (A, C); 4 levels (+-4.8, +-1.6 kHz), 1600 symbols/s of 2 bits, one of each phase.\n"
            "  6400/4: 4 phases (A to D); 4 levels, 3200 symbols/s of 2 bits.\n"
            "4 levels are closer together (3.2 kHz apart instead of 9.6): they need a stronger signal.\n"
            "A pager listens to one phase, or looks for its address in all of them (its phase mode).", false},
        {"Cycle / frame",
            "Where the network is in its hour: 15 cycles of 128 frames of 1.875 s (a cycle is 4 minutes).\n"
            "Cycle 0, frame 0 starts at the top of the hour; a pager listens to its own frames only.", false},
        {"Collapse, repeats",
            "Collapse m: a pager wakes up for one frame in 2^m (its frame n, n + 2^m, ...) and sleeps otherwise.\n"
            "In brackets: the longest a page waits for the next frame of its pager. A smaller collapse pages\n"
            "faster, a larger one saves the battery of the pagers.\n"
            "Repeats: - the frame is sent once (the usual). xN: it is sent N times for pagers in poor coverage;\n"
            "TD: the collapse of the repeats.", false},
        {"Roaming",
            "Whether the network lets pagers roam to it (the roaming bit of every frame). A roaming network also\n"
            "sends its system ID (below), that pagers compare with theirs.", false},
        {"Network time",
            "The date and the time the network sends, as of the last frame: pagers set their clock from it.\n"
            "Networks must send it at least once an hour (cycle 0, frame 0); many send it in more frames.\n"
            "The time goes on with the frames between.\n"
            "Green: this cycle. Plain: this hour. Grey: older. Grey-green: kind assumed (often noise).", false},
        {"Time zone",
            "The time zone the network sends with the time: offset from UTC, DST when the time is daylight saving time,\n"
            "ext the extended second (0 to 7 steps of 0.9375 s) that corrects the time of frames not aligned to the\n"
            "second.\nGreen: this cycle. Plain: this hour. Grey: older.", false},
        {"Local ID, zone",
            "Who is transmitting: the local channel ID and the coverage zone of a roaming network, in every frame.\n"
            "A roaming pager looks for its own ID and zone to know it is at home or in a partner network.\n"
            "Green: this cycle. Plain: this hour. Grey: older.", false},
        {"Country, TMF",
            "The country code (ITU-T E.212) of the network, and its traffic management flags T3 T2 T1 T0:\n"
            "the traffic groups this channel carries (a pager of a group set to 0 looks for another channel).\n"
            "Sent in frames 0 to 3 of each cycle.\nGreen: this cycle. Plain: this hour. Grey: older.", false},
        {"Phases A, B",
            "Bit error rate of phases A and B of the last frame: the independent 1600 bit/s channels of the frame\n"
            "(A at 1600; A and C at 3200; A to D at 6400, where a symbol carries A and B, the next one C and D).\n"
            "A phase in error damages only the pages it carries.", false},
        {"Phases C, D",
            "Bit error rate of phases C and D of the last frame (C at 3200, C and D at 6400).", false},
        rememberedRow,
        {"Reassembling",
            "Long messages being put together from their fragments. A message too long for one frame is sent\n"
            "in fragments in the following frames; it is shown once all of them arrived.", false},
        {"Re-sync, count",
            "Emergency re-synchronization (ERS): a special signal a network sends when its time reference failed,\n"
            "for 4 minutes so that every pager receives it once and finds the frames again.\n"
            "The length of the last one, and how many were received since the start.", true},
        messagesRow, resetRow
    };
    static const QList<DetailRow> gsc = {
        {"State",
            "SYNC while a transmission is received, HUNT between them, VOICE while a voice page is played.", false},
        {"Preamble, batch, slot",
            "The last page. Preamble 0 to 9: pagers sleep until the preamble of their group, which saves their\n"
            "battery; NBS: sent without it (non battery saver), to every pager. batch: it was sent in a batch with\n"
            "other pages of the same preamble, - alone. Slot: its place in the batch.", false},
        rememberedRow, messagesRow, resetRow
    };

    return (protocol == 0) ? pocsag : (protocol == 1) ? flex : gsc;
}

} // namespace

// The protocols, in three places:
// - Settings: a row for each protocol, always shown: its switch, coloured by the state of its reception, what is
//   received (POCSAG the speeds, FLEX the mode) and its polarity; FLEX also the cycle and frame and the network time
//   indicator, GSC the voice button (the mute of the voice, green while it is played)
// - Protocols (rolled up at first): the settings of each protocol, set once
// - Details (rolled up at first): a table for each protocol, the normal and inverted networks in columns, and the
//   FLEX group slots
void RadioPagingDemodGUI::layoutProtocols()
{
    QFont small = font();

    if (small.pointSizeF() > 1) {
        small.setPointSizeF(small.pointSizeF() - 1.0);
    }

    // The widgets leave the grid of the form
    QGridLayout *grid = ui->protocolLayout;

    while (QLayoutItem *item = grid->takeAt(0))
    {
        if (item->layout()) {
            item->layout()->setParent(nullptr);
        } else {
            delete item;
        }
    }

    const char *names[3] = {"POCSAG", "FLEX", "GSC"};
    QWidget *enabled[3] = {ui->pocsagEnabled, ui->flexEnabled, ui->gscEnabled};
    QWidget *deviationLabel[3] = {ui->pocsagDeviationLabel, ui->flexDeviationLabel, ui->gscDeviationLabel};
    QWidget *deviation[3] = {ui->pocsagDeviation, ui->flexDeviation, ui->gscDeviation};
    QWidget *dc[3] = {ui->pocsagDcCorrection, ui->flexDcCorrection, ui->gscDcCorrection};
    QComboBox *charset[3] = {ui->pocsagCharset, ui->flexCharset, ui->gscCharset};
    ui->flexMaxMessageNumber->hide();     // 63, as the networks are; the WebAPI sets another

    // Settings: the status row of each protocol
    static const char *receivingTips[3] = {
        "POCSAG speeds in sync (bit/s: 512, 1200, 2400). A network sends at one speed; several at once\n"
        "means several transmitters or a mixed network. Grey once the reception ended.",
        "FLEX mode of the frame being received: bit/s / number of frequency levels.\n"
        "  1600/2: 1 phase (A); 2 levels (+-4.8 kHz), 1600 symbols/s of 1 bit.\n"
        "  3200/2: 2 phases (A, C); 2 levels, 3200 symbols/s of 1 bit.\n"
        "  3200/4: 2 phases (A, C); 4 levels (+-4.8, +-1.6 kHz), 1600 symbols/s of 2 bits.\n"
        "  6400/4: 4 phases (A to D); 4 levels, 3200 symbols/s of 2 bits.\n"
        "A phase is an independent 1600 bit/s channel: more phases carry more pages. 4 levels need a\n"
        "stronger signal (3.2 kHz apart instead of 9.6).\n"
        "ERS during an emergency re-synchronization, with its length so far. Grey once the reception ended.",
        "GSC: VOICE while a voice page is played (the speaker button on the right mutes it)."
    };

    for (int p = 0; p < 3; p++)
    {
        enabled[p]->setMinimumWidth(0);
        enabled[p]->setMaximumWidth(QWIDGETSIZE_MAX);
        enabled[p]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        enabled[p]->setToolTip(tr("%1 decoding on or off.\n"
            "Its colour is the state of the reception: grey off; the colour of a switch on while it looks for a\n"
            "signal (hunting); green while it is in sync with a transmission%2.").arg(names[p])
            .arg(p == 1 ? tr(";\namber during an emergency re-synchronization of the network")
                : p == 2 ? tr(";\namber while a voice page is received") : QString()));
        grid->addWidget(enabled[p], p, 0);

        m_receivingLabel[p] = new QLabel(ui->settingsContainer);
        m_receivingLabel[p]->setToolTip(tr(receivingTips[p]));
        grid->addWidget(m_receivingLabel[p], p, 1);

        m_polarityLabel[p] = new QLabel(ui->settingsContainer);
        m_polarityLabel[p]->setAlignment(Qt::AlignCenter);
        m_polarityLabel[p]->setFixedWidth(fontMetrics().horizontalAdvance("N") + 10);
        m_polarityLabel[p]->setToolTip(tr("Polarity of the %1 network being received: N normal, I inverted.\n"
            "Which way the transmitter shifts its frequency for a 1 and a 0. A network uses one polarity, and\n"
            "its pagers are made for it: the same address on the other polarity is another pager.\n"
            "Both are decoded at once; the Details have a column for each. Grey once the reception ended.").arg(names[p]));
        grid->addWidget(m_polarityLabel[p], p, 2);

        // The bit error rate: a box in the colour of the quality with the rate in it
        m_berLabel[p] = new QLabel(ui->settingsContainer);
        m_berLabel[p]->setAlignment(Qt::AlignCenter);
        m_berLabel[p]->setFixedWidth(fontMetrics().horizontalAdvance("00.00%") + 10);
        grid->addWidget(m_berLabel[p], p, 3);

        // The carrier offset, after the bit error rate
        m_offsetLabel[p] = new QLabel(ui->settingsContainer);
        m_offsetLabel[p]->setFixedWidth(fontMetrics().horizontalAdvance(QString("%1f -9999 Hz").arg(QChar(0x394))) + 4);
        grid->addWidget(m_offsetLabel[p], p, 4);
    }

    m_flexFrameLabel = new QLabel(ui->settingsContainer);
    m_flexFrameLabel->setToolTip(tr("FLEX cycle and frame of the last frame received.\n"
        "The network runs on a fixed hourly grid: 15 cycles of 128 frames of 1.875 s (a cycle is 4 minutes),\n"
        "cycle 0 frame 0 at the top of the hour. A pager listens to its own frames only, so the frame of a\n"
        "page tells which pagers can receive it. Grey once the reception ended."));
    grid->addWidget(m_flexFrameLabel, 1, 5);

    // The FLEX network time: lit while the network sends it, its tooltip gives the date, time and zone
    m_flexClockLabel = new QLabel(ui->settingsContainer);
    m_flexClockLabel->setPixmap(QIcon(":/clockcurrent.png").pixmap(16, 16));
    m_flexClockLabel->setAlignment(Qt::AlignCenter);
    m_flexClockLabel->setFixedSize(22, 22);
    grid->addWidget(m_flexClockLabel, 1, 7);

    // The voice of GSC voice pages: its mute, green while it is played
    ui->gscVoiceLayout->removeWidget(ui->gscAudio);
    grid->addWidget(ui->gscAudio, 2, 7);

    grid->setColumnMinimumWidth(1, fontMetrics().horizontalAdvance("512 1200 2400") + 8);
    grid->setColumnStretch(6, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);

    // Protocols: the settings of each protocol in aligned columns (name, deviation, DC correction, codepage and its
    // editor, reset); POCSAG strip and GSC voice on a line under their protocol
    QWidget *settingsGrid = new QWidget(ui->protocolsContainer);
    QGridLayout *columns = new QGridLayout(settingsGrid);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setHorizontalSpacing(6);
    columns->setVerticalSpacing(2);
    int row = 0;

    for (int p = 0; p < 3; p++, row++)
    {
        QLabel *name = new QLabel(names[p], settingsGrid);
        QFont bold = name->font();
        bold.setBold(true);
        name->setFont(bold);
        columns->addWidget(name, row, 0);
        columns->addWidget(deviationLabel[p], row, 1);
        columns->addWidget(deviation[p], row, 2);
        columns->addWidget(dc[p], row, 3);

        m_codepageLabel[p] = new QLabel(tr("Codepage"), settingsGrid);
        columns->addWidget(m_codepageLabel[p], row, 4);
        // Not wider than the window: a long codepage name is cut in the box, whole in its list
        charset[p]->setMaximumWidth(QWIDGETSIZE_MAX);
        charset[p]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        charset[p]->setMinimumContentsLength(12);
        charset[p]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        columns->addWidget(charset[p], row, 5);

        // The custom codepage of the protocol: its editor (the character set button of the Pager demodulator)
        QToolButton *custom = new QToolButton(settingsGrid);
        custom->setIcon(QIcon(":/keyboard.png"));
        custom->setToolTip(tr("Edit the custom codepage of %1: the character of each code and the direction of the text.\n"
            "Choose Custom in the codepages to decode with it").arg(names[p]));
        connect(custom, &QToolButton::clicked, this, [this, p]() { editCustomCodepage(p); });
        columns->addWidget(custom, row, 6);

        QToolButton *reset = new QToolButton(settingsGrid);
        reset->setIcon(QIcon(":/recycle.png"));
        reset->setToolTip(tr("Forget what the %1 receivers remember: the messages for duplicate detection%2.\n"
            "Done when the device starts too, so that a file played again is not taken as duplicates")
            .arg(names[p]).arg(p == 1 ? tr(", fragments, groups, message numbers and network time") : QString()));
        connect(reset, &QToolButton::clicked, this, [this, p]() { m_radioPagingDemod->resetProtocol(p); });
        columns->addWidget(reset, row, 7);

        // The line under the protocol: POCSAG the fill stripped, GSC the voice
        if ((p == 0) || (p == 2))
        {
            row++;
            QHBoxLayout *extra = new QHBoxLayout();
            extra->setSpacing(4);
            extra->addWidget(new QLabel((p == 0) ? tr("Strip") : tr("Voice"), settingsGrid));

            if (p == 0) {
                extra->addWidget(ui->pocsagFill);
            } else {
                extra->addLayout(ui->gscVoiceLayout);
            }

            extra->addStretch(1);
            columns->addLayout(extra, row, 1, 1, 7);
        }
    }

    columns->setColumnStretch(5, 1);
    ui->protocolsLayout->addWidget(settingsGrid);

    // Details: a tab for each protocol, its table and (FLEX) the group slots of each network
    QTabWidget *tabs = new QTabWidget(ui->detailsContainer);
    ui->detailsLayout->addWidget(tabs);
    const int rowHeight = fontMetrics().height() + 4;

    for (int p = 0; p < 3; p++)
    {
        const QList<DetailRow>& rows = detailRows(p);
        QWidget *page = new QWidget(tabs);
        QVBoxLayout *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(2, 2, 2, 2);
        pageLayout->setSpacing(2);

        QTableWidget *table = new QTableWidget(rows.size(), 3, page);
        table->setHorizontalHeaderLabels({QString(), tr("Normal"), tr("Inverted")});
        table->verticalHeader()->setVisible(false);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setDefaultSectionSize(rowHeight);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setAlternatingRowColors(true);
        table->setShowGrid(false);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        int nameWidth = 0;

        for (int r = 0; r < rows.size(); r++)
        {
            QTableWidgetItem *name = new QTableWidgetItem(tr(rows[r].m_name));
            name->setToolTip(tr(rows[r].m_toolTip));
            table->setItem(r, 0, name);

            // The values explain themselves too
            for (int column = 1; column <= 2; column++)
            {
                QTableWidgetItem *value = new QTableWidgetItem("-");
                value->setToolTip(name->toolTip());
                table->setItem(r, column, value);
            }
            nameWidth = std::max(nameWidth, fontMetrics().horizontalAdvance(name->text()));

            // A value of the protocol, not of one network: across both columns
            if (rows[r].m_single) {
                table->setSpan(r, 1, 1, 2);
            }
        }

        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->setColumnWidth(0, nameWidth + 16);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->setFixedHeight(table->horizontalHeader()->sizeHint().height() + rows.size() * rowHeight + 2 * table->frameWidth());
        m_detailsTable[p] = table;
        pageLayout->addWidget(table);

        // FLEX: the bit error rate of each phase in a box of the colour of its quality, A and B on a row, C and D on the
        // next (as the symbols of 6400/4 pair them)
        for (int pol = 0; (p == 1) && (pol < 2); pol++)
        {
            for (int half = 0; half < 2; half++)
            {
                QLabel *label = new QLabel(table);
                label->setTextFormat(Qt::RichText);
                label->setContentsMargins(3, 0, 0, 0);
                label->setText("-");
                table->setCellWidget(half == 0 ? FlexPhasesAB : FlexPhasesCD, 1 + pol, label);
                m_flexPhaseLabel[pol][half] = label;
            }

            // The network words: a chip around the value while it is fresh
            const int wordRows[WordCount] = {FlexTime, FlexZone, FlexSystem, FlexCountry};

            for (int word = 0; word < WordCount; word++)
            {
                QLabel *label = new QLabel(table);
                label->setTextFormat(Qt::RichText);
                label->setContentsMargins(3, 0, 0, 0);
                label->setText("-");
                label->setToolTip(tr(rows[wordRows[word]].m_toolTip));
                table->setCellWidget(wordRows[word], 1 + pol, label);
                m_flexWordLabel[pol][word] = label;
            }
        }

        // FLEX: the 16 group slots (temporary addresses) of each network
        for (int pol = 0; (p == 1) && (pol < 2); pol++)
        {
            m_flexGroupBar[pol] = new QWidget(page);
            QHBoxLayout *bar = new QHBoxLayout(m_flexGroupBar[pol]);
            bar->setContentsMargins(0, 0, 0, 0);
            bar->setSpacing(1);
            QLabel *title = new QLabel(pol == 1 ? tr("Groups, inverted") : tr("Groups, normal"), m_flexGroupBar[pol]);
            title->setFixedWidth(nameWidth + 16);
            title->setToolTip(tr("Group slots of the %1 network.\n"
                "To send one message to several pagers, the network first tells each of them (a short instruction)\n"
                "to also listen to a temporary address, 0 to F here (2029568 to 2029583), in a given frame; the\n"
                "message is then sent once to that address. Hover a slot for its members and frame.\n"
                "Blue: members being assigned. Amber: waiting for the group message. Teal: the group message\n"
                "being received. Green: delivered. Red: its frame went by without it. Violet: assigned again to\n"
                "another frame before it. Orange: the group message did not end. Grey: free.")
                .arg(pol == 1 ? tr("inverted") : tr("normal")));
            bar->addWidget(title);

            for (int s = 0; s < 16; s++)
            {
                QLabel *cell = new QLabel(QString::number(s, 16).toUpper(), m_flexGroupBar[pol]);
                cell->setFont(small);
                cell->setAlignment(Qt::AlignCenter);
                cell->setFixedWidth(cell->fontMetrics().horizontalAdvance("00") + 4);
                m_flexGroupCell[pol][s] = cell;
                bar->addWidget(cell);
            }

            bar->addStretch(1);
            pageLayout->addWidget(m_flexGroupBar[pol]);
        }

        pageLayout->addStretch(1);
        tabs->addTab(page, names[p]);
    }
}

// The network being received by a protocol: the polarity in sync, else the one received last
int RadioPagingDemodGUI::currentPolarity(const RadioPagingDemodSink::Status& status, int protocol)
{
    const bool sync[2] = {status.m_pol[protocol][0].m_sync, status.m_pol[protocol][1].m_sync};

    if (sync[0] != sync[1]) {
        m_currentPol[protocol] = sync[1] ? 1 : 0;
    } else if (!sync[0] && (status.m_pol[protocol][0].m_berLive != status.m_pol[protocol][1].m_berLive)) {
        m_currentPol[protocol] = status.m_pol[protocol][1].m_berLive ? 1 : 0;
    } else if (!status.m_pol[protocol][m_currentPol[protocol]].m_seen && status.m_pol[protocol][1 - m_currentPol[protocol]].m_seen) {
        m_currentPol[protocol] = 1 - m_currentPol[protocol];
    }

    return m_currentPol[protocol];
}

// The status row of each protocol in the settings (its switch coloured by the state of its reception, what is
// received, the polarity; FLEX the cycle and frame and the network time) and the details
void RadioPagingDemodGUI::updateStatus()
{
    RadioPagingDemodSink::Status status = m_radioPagingDemod->getStatus();
    // A protocol is off by its switch (the receivers report nothing before the device runs)
    status.m_enabled[0] = m_settings.m_pocsagEnabled;
    status.m_enabled[1] = m_settings.m_flexEnabled;
    status.m_enabled[2] = m_settings.m_gscEnabled;

    // FLEX: between the frames the decoder hunts for the next sync 1; SYNC stays shown until
    // that sync is due, then HUNT (the decoder state is not changed)
    qint64 now = m_statusClock.elapsed();

    if (status.m_flexSync && !m_flexWasSync) {
        m_flexFrameStart = now;
    } else if (status.m_flexSync && (now - m_flexFrameStart >= m_flexFrameMs)) {
        m_flexFrameStart += m_flexFrameMs;  // Frames back to back: the next one started
    }
    if (!status.m_enabled[1]) {
        m_flexFrameStart = -1;
    }
    m_flexWasSync = status.m_flexSync;

    bool flexHeld = !status.m_flexSync && (m_flexFrameStart >= 0)
        && (now - m_flexFrameStart < m_flexFrameMs + m_statusPeriodMs);
    // The polarity of the frame being received, or of the last one
    int flexPol = status.m_flexInverted ? 1 : 0;
    m_currentPol[1] = flexPol;
    const int pol[3] = {currentPolarity(status, 0), flexPol, currentPolarity(status, 2)};
    ButtonSwitch *switches[3] = {ui->pocsagEnabled, ui->flexEnabled, ui->gscEnabled};
    const char *names[3] = {"POCSAG", "FLEX", "GSC"};

    for (int p = 0; p < 3; p++)
    {
        const RadioPagingDemodSink::Status::Polarity& s = status.m_pol[p][pol[p]];
        const bool sync = s.m_sync || ((p == 1) && flexHeld);
        const bool live = sync || s.m_berLive;
        const bool ers = (p == 1) && status.m_flexErs && !status.m_flexSync;
        const bool voice = (p == 2) && status.m_gscVoicePol[pol[p]];

        // The switch: its own colours off and while hunting, green in sync, amber in a voice page or a re-synchronization
        const int state = !status.m_enabled[p] ? 0 : (ers || voice) ? 2 : sync ? 1 : 0;

        if (state != m_switchState[p])
        {
            m_switchState[p] = state;

            if (state == 0) {
                switches[p]->resetColor();
            } else {
                switches[p]->setColor(state == 1 ? QColor(Qt::darkGreen) : QColor(0xb0, 0x70, 0x10));
            }
        }

        // What is received: POCSAG the speeds in sync, FLEX the mode of the frame, GSC a voice page
        QString receiving;

        if (!status.m_enabled[p])
        {
            receiving = tr("off");
        }
        else if (p == 0)
        {
            QStringList speeds;

            for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
            {
                if (status.m_pocsagSpeedSync[pol[p]][i]) {
                    speeds.append(QString::number(RadioPagingDemodSettings::m_pocsagSpeeds[i]));
                }
            }

            receiving = speeds.join(" ");
        }
        else if (p == 1)
        {
            receiving = ers ? tr("ERS %1 s").arg(status.m_flexErsSeconds, 0, 'f', 1) : status.m_flexModePol[pol[p]];
        }
        else if (voice)
        {
            receiving = tr("VOICE");
        }

        m_receivingLabel[p]->setText(receiving.isEmpty() ? QString("-") : receiving);
        m_receivingLabel[p]->setStyleSheet((live && status.m_enabled[p]) ? QString() : QString("QLabel { color : %1; }").arg(colourInactive));

        // The polarity, in the colours of the Pol column of the messages
        if (status.m_enabled[p] && s.m_seen)
        {
            const bool inverted = pol[p] == 1;
            m_polarityLabel[p]->setText(inverted ? "I" : "N");
            // Live: the colours of the Pol column of the messages; ended: the grey box
            m_polarityLabel[p]->setStyleSheet(!live ? chipStyle(ChipEnded)
                : QString("QLabel { background-color : %1; color : %2; border-radius : 3px; }")
                    .arg(inverted ? "#ffcc44" : "#88aacc").arg(inverted ? "#2a1f00" : "#0d1a2a"));
        }
        else
        {
            m_polarityLabel[p]->setText(QString());
            m_polarityLabel[p]->setStyleSheet(QString());
        }

        // The bit error rate in a box of the colour of the quality: green no errors, yellow errors corrected, red
        // codewords that could not be corrected, grey once the reception ended
        const RadioPagingBerWindow& ber = s.m_ber;

        if (status.m_enabled[p] && ber.valid())
        {
            const double rate = 100.0 * ber.ber();
            m_berLabel[p]->setText(QString("%1%").arg(rate, 0, 'f', rate < 10.0 ? 2 : 1));
            m_berLabel[p]->setStyleSheet(chipStyle(qualityChip(ber, live)));
            m_berLabel[p]->setToolTip(tr("%1 bit error rate.\n%2\n\nNow%3: %4 wrong bits of %5 over the last %6 codewords, %7 codewords not corrected.")
                .arg(names[p]).arg(tr(berRow.m_toolTip)).arg(pol[p] == 1 ? tr(" (inverted network)") : QString())
                .arg(ber.m_errorBits).arg(ber.m_bits).arg(ber.m_codewords).arg(ber.m_uncorrectable));
        }
        else
        {
            m_berLabel[p]->setText(QString());
            m_berLabel[p]->setStyleSheet(QString());
            m_berLabel[p]->setToolTip(tr("%1 bit error rate.\n%2\n\nNone measured yet.").arg(names[p]).arg(tr(berRow.m_toolTip)));
        }

        // The carrier offset of the network, grey once the reception ended
        if (status.m_enabled[p] && s.m_offsetValid)
        {
            m_offsetLabel[p]->setText(QString("%1f %2%3 Hz").arg(QChar(0x394)).arg(s.m_offset < 0.0f ? "-" : "+")
                .arg((int) std::round(std::fabs(s.m_offset))));
            m_offsetLabel[p]->setStyleSheet(live ? QString() : QString("QLabel { color : %1; }").arg(colourInactive));
        }
        else
        {
            m_offsetLabel[p]->setText(QString());
        }

        m_offsetLabel[p]->setToolTip(tr("%1 carrier offset%2.\n%3").arg(names[p])
            .arg(pol[p] == 1 ? tr(" (inverted network)") : QString()).arg(tr(offsetRow.m_toolTip)));
    }

    // FLEX: the network words of the frames received, the cycle and frame of the last frame, the network time indicator
    stampFlexWords(status);
    const RadioPagingFlexDecoder::FrameReport& frame = status.m_flexFramePol[flexPol];
    const bool flexLive = status.m_pol[1][flexPol].m_sync || flexHeld || status.m_pol[1][flexPol].m_berLive;
    m_flexFrameLabel->setText((status.m_enabled[1] && frame.m_fiwValid)
        ? QString("C%1/F%2").arg(frame.m_cycle, 2, 10, QChar('0')).arg(frame.m_frame, 3, 10, QChar('0')) : QString());
    m_flexFrameLabel->setStyleSheet(flexLive ? QString() : QString("QLabel { color : %1; }").arg(colourInactive));

    const QString time = status.m_enabled[1] ? flexTimeText(status.m_flexTimePol[flexPol]) : QString();
    // By the age of the newest time word (date, time or zone), as the details, in the colours of the indicators of the
    // other channels (NFM, BFM): green received in this cycle, the icon alone in this hour, grey and dimmed older or
    // never; the kind of the time assumed: grey with a green hue, dimmed (it is often from noise: it does not stand out)
    const FlexWordAge dateAge = flexWordAge(flexPol, WordDateTime, status.m_signalMs);
    const FlexWordAge zoneAge = flexWordAge(flexPol, WordZone, status.m_signalMs);
    const FlexWordAge timeAge = (dateAge == AgeNever) ? zoneAge : (zoneAge == AgeNever) ? dateAge : std::min(dateAge, zoneAge);
    const bool timeShown = !time.isEmpty() && (timeAge != AgeNever) && (timeAge != AgeOld);
    const RadioPagingFlexDecoder::NetworkTime& flexTime = status.m_flexTimePol[flexPol];
    const bool assumed = timeShown && flexTime.m_timeValid && !flexTime.m_modeConfirmed;
    // A disabled label does not dim its pixmap
    m_flexClockLabel->setPixmap(QIcon(":/clockcurrent.png").pixmap(16, 16, (timeShown && !assumed) ? QIcon::Normal : QIcon::Disabled));
    m_flexClockLabel->setStyleSheet(assumed ? QString("QLabel { background:%1; }").arg(colourAssumed)
        : ((timeAge == AgeFrame) || (timeAge == AgeCycle)) ? QString("QLabel { background-color : green; }")
        : timeShown ? QString() : QString("QLabel { background:%1; }").arg(colourOff));
    const QString legend = tr("Green: this cycle. Plain: this hour. Grey: older. Grey-green: kind assumed (often noise).");
    const QString kind = flexTimeKind(flexTime, status.m_flexFramePol[flexPol]);
    m_flexClockLabel->setToolTip(time.isEmpty()
        ? tr("FLEX network time: none received\n%1").arg(legend)
        : tr("FLEX network time: %1\n%2%3").arg(time).arg(kind.isEmpty() ? QString() : kind + "\n").arg(legend));

    // Green while the voice is played
    bool playing = status.m_enabled[2] && status.m_gscAudio;
    ui->gscAudio->setStyleSheet(playing ? "QToolButton { background-color : green; }" : "");

    updateDetails(status, flexPol, flexHeld);
    updateGroupBars(status);
}

// FLEX: the frame each kind of network word of each polarity was last received in. The GUI sees every frame (it is
// updated more often than the 1.875 s of a frame): the words of a frame are taken once, when it is new
void RadioPagingDemodGUI::stampFlexWords(const RadioPagingDemodSink::Status& status)
{
    for (int pol = 0; pol < 2; pol++)
    {
        const RadioPagingFlexDecoder::FrameReport& frame = status.m_flexFramePol[pol];

        if (!frame.m_fiwValid) {
            continue;
        }

        const int index = (int) (frame.m_cycle * 128 + frame.m_frame);

        if (index == m_flexLastFrame[pol]) {
            continue;
        }

        const qint64 now = status.m_signalMs;
        m_flexLastFrame[pol] = index;
        m_flexLastFrameMs[pol] = now;
        auto stamp = [&](FlexWord word) {
            m_flexWordMs[pol][word] = now;
            m_flexWordFrame[pol][word] = index;
        };

        for (const auto& block : frame.m_blocks)
        {
            for (const RadioPagingFlexDecoder::ExtraBiw& e : block.m_extra)
            {
                switch (e.m_kind)
                {
                case RadioPagingFlexDecoder::ExtraBiw::Date:
                case RadioPagingFlexDecoder::ExtraBiw::Time:
                    stamp(WordDateTime);
                    break;
                case RadioPagingFlexDecoder::ExtraBiw::SystemInfo:
                    // A = 4 or 5: the time zone word
                    if ((e.m_infoType == 4) || (e.m_infoType == 5)) {
                        stamp(WordZone);
                    }
                    break;
                case RadioPagingFlexDecoder::ExtraBiw::Ssid1:
                    stamp(WordSsid1);
                    break;
                case RadioPagingFlexDecoder::ExtraBiw::Ssid2:
                    stamp(WordSsid2);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

// The kind of the network time (standard: the time of frame 0 of its cycle with the frame offset added, direct: the time
// of the frame), whether the time words of a cycle told it or it is assumed, its precision and when it was received
QString RadioPagingDemodGUI::flexTimeKind(const RadioPagingFlexDecoder::NetworkTime& t, const RadioPagingFlexDecoder::FrameReport& frame)
{
    if (!t.m_timeValid) {
        return QString();
    }

    QString kind = (t.m_mode == 1) ? tr("Direct (the time of the frame)") : tr("Standard (the time of frame 0, the frame offset added)");
    kind += t.m_modeConfirmed ? tr(", confirmed") : tr(", assumed");

    // When the time word came and when its state changes, on the grid of the network (cycle and frame) and in its
    // time: right on a recording played back too. The time of an event is the time of the last frame moved by the
    // frames between, about (the network time has steps of 7.5 s or 0.9375 s)
    if (frame.m_fiwValid && (t.m_timeAge >= 0))
    {
        const int now = (int) (frame.m_cycle * 128 + frame.m_frame);
        const double nowSeconds = t.m_currentHour * 3600.0 + t.m_currentMinute * 60.0 + t.m_currentSecond;
        auto at = [&](int framesFromNow) {
            const int index = ((now + framesFromNow) % 1920 + 1920) % 1920;
            const int seconds = ((int) std::lround(nowSeconds + framesFromNow * 1.875) % 86400 + 86400) % 86400;
            return QString("C%1/F%2 (~%3:%4:%5)").arg(index / 128, 2, 10, QChar('0')).arg(index % 128, 3, 10, QChar('0'))
                .arg(seconds / 3600, 2, 10, QChar('0')).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
        };
        const int received = -t.m_timeAge;
        kind += tr("\nLast received: %1, %2 frames ago").arg(at(received)).arg(t.m_timeAge);

        if (t.m_timeAge < 128) {
            kind += tr("\nGreen until %1, kept until %2").arg(at(received + 128)).arg(at(received + 1920));
        } else {
            kind += tr("\nKept until %1").arg(at(received + 1920));
        }
    }

    kind += t.m_zoneValid ? tr("\nPrecision 0.9375 s") : tr("\nPrecision 7.5 s (no zone word)");
    return kind;
}

// The age of a network word, in time of signal (its samples, signal or noise: a file played faster too), received or
// not since: in the last frame of its network (while that frame is the latest one, at most two frames ago), in this
// cycle (128 frames, 4 minutes), in this hour (15 cycles: the least a network sends the time), older
RadioPagingDemodGUI::FlexWordAge RadioPagingDemodGUI::flexWordAge(int pol, FlexWord word, qint64 signalMs) const
{
    const qint64 received = m_flexWordMs[pol][word];

    if ((received < 0) || (received > signalMs)) {
        return AgeNever;
    }

    const qint64 age = signalMs - received;

    if ((m_flexWordFrame[pol][word] == m_flexLastFrame[pol]) && (age < 2 * m_flexFrameMs)) {
        return AgeFrame;
    }

    return (age < 128 * m_flexFrameMs) ? AgeCycle : (age < 15 * 128 * m_flexFrameMs) ? AgeHour : AgeOld;
}

void RadioPagingDemodGUI::setNetworkDetail(FlexWord word, int pol, const QString& text, FlexWordAge age, bool assumed)
{
    QLabel *label = m_flexWordLabel[pol][word];

    if (text.isEmpty())
    {
        label->setText(QString("<span style=\"color:%1\">-</span>").arg(colourInactive));
    }
    else if (assumed && (age != AgeOld))
    {
        // The time mode not confirmed yet (the frame offset may be wrong, often from noise): grey with a green hue
        label->setText(QString("<span style=\"background-color:%1; color:#a4aca4;\">&nbsp;%2&nbsp;</span>")
            .arg(colourAssumed).arg(text.toHtmlEscaped()));
    }
    else if ((age == AgeFrame) || (age == AgeCycle))
    {
        // Received in this cycle: green, as the indicators of the other channels
        label->setText(QString("<span style=\"background-color:green; color:#ffffff;\">&nbsp;%1&nbsp;</span>").arg(text.toHtmlEscaped()));
    }
    else if (age == AgeOld)
    {
        // Older than an hour: grey
        label->setText(QString("<span style=\"background-color:%1; color:#9a9a9a;\">&nbsp;%2&nbsp;</span>")
            .arg(colourOff).arg(text.toHtmlEscaped()));
    }
    else
    {
        // In this hour: the text colour
        label->setText(QString("<span style=\"color:%1\">%2</span>").arg(palette().text().color().name()).arg(text.toHtmlEscaped()));
    }
}

// A value of the details: grey once its reception ended, else in its colour (the text colour if none)
void RadioPagingDemodGUI::setDetail(int protocol, int row, int pol, const QString& text, bool live, const QColor& colour)
{
    QTableWidgetItem *item = m_detailsTable[protocol]->item(row, 1 + pol);
    item->setText(text.isEmpty() ? QString("-") : text);
    item->setForeground(!live ? QBrush(QColor(colourInactive)) : colour.isValid() ? QBrush(colour) : palette().text());
}

// The details of each protocol: the values of each network (polarity) in its column
void RadioPagingDemodGUI::updateDetails(const RadioPagingDemodSink::Status& status, int flexPol, bool flexHeld)
{
    for (int p = 0; p < 3; p++)
    {
        const int rememberedRows[3] = {PocsagRemembered, FlexRemembered, GscRemembered};
        const int messagesRows[3] = {PocsagMessages, FlexMessages, GscMessages};
        const int resetRows[3] = {PocsagReset, FlexReset, GscReset};
        const int rememberedRowIndex = rememberedRows[p];
        const int messagesRowIndex = messagesRows[p];
        const int resetRowIndex = resetRows[p];
        const int current = (p == 1) ? flexPol : m_currentPol[p];

        for (int pol = 0; pol < 2; pol++)
        {
            const RadioPagingDemodSink::Status::Polarity& s = status.m_pol[p][pol];
            // FLEX: live between the frames of a transmission too, as its status row
            const bool held = (p == 1) && (pol == flexPol) && flexHeld;
            const bool live = status.m_enabled[p] && (s.m_sync || s.m_berLive || held);
            setDetail(p, rememberedRowIndex, pol, s.m_seen || s.m_remembered ? QString::number(s.m_remembered) : QString(), live);

            if (p == 0)
            {
                QStringList speeds;

                for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
                {
                    if (status.m_pocsagSpeedSync[pol][i]) {
                        speeds.append(QString::number(RadioPagingDemodSettings::m_pocsagSpeeds[i]));
                    }
                }

                setDetail(p, PocsagSpeeds, pol, speeds.join(" "), live);
                setDetail(p, PocsagBatches, pol, ((pol == current) && (status.m_pocsagBatches > 0))
                    ? QString::number(status.m_pocsagBatches) : QString(), live);
            }
            else if (p == 1)
            {
                const RadioPagingFlexDecoder::FrameReport& frame = status.m_flexFramePol[pol];
                const RadioPagingFlexDecoder::NetworkTime& t = status.m_flexTimePol[pol];
                QString collapse;

                if (frame.m_fiwValid && !frame.m_blocks.empty())
                {
                    // The collapse and the longest wait of a page for the frame of its pager: 2^m frames of 1.875 s
                    const unsigned m = frame.m_blocks.front().m_collapse;
                    const double wait = (1 << m) * 1.875;
                    collapse = QString("%1 (%2)").arg(m)
                        .arg(wait < 60.0 ? QString("%1 s").arg(wait, 0, 'f', 1) : QString("%1 min").arg(wait / 60.0));
                    // Sent several times: the count and the TD collapse that replaces the system collapse (3.6)
                    collapse += frame.m_repeat ? QString(", x%1%2").arg(frame.m_transmissions)
                        .arg(frame.m_tdCollapse >= 0 ? QString(" TD %1").arg(frame.m_tdCollapse) : QString()) : QString(", -");
                }

                // Every phase of the last frame (A at 1600, A and C at 3200, A to D at 6400), a box each in the colour
                // of its quality, grey once the reception ended. The separator after the phase is its low traffic flag
                // (STD-43A 3.6, t0-t3 for phases a-d when the frame is not repeated: its address field ends in block 0):
                // ':' not set, '`' set. A and B on a row, C and D on the next. The tooltip has the block information
                // of each phase
                QStringList phaseTips;
                QStringList halves[2];

                for (const auto& block : frame.m_blocks)
                {
                    const int index = (block.m_phase - 'A') & 3;
                    const RadioPagingBerWindow& phaseBer = status.m_flexPhaseBerPol[pol][index];
                    const bool lowTraffic = !frame.m_repeat && ((frame.m_traffic >> index) & 1);

                    if (phaseBer.valid())
                    {
                        halves[index / 2].append(chipHtml(qualityChip(phaseBer, live), QString("%1%2%3%").arg(QChar(block.m_phase))
                            .arg(lowTraffic ? lowTrafficSeparator : ':').arg(100.0 * phaseBer.ber(), 0, 'f', 2)));
                        phaseTips.append(tr("%1: bit error rate %2% (%3 bits corrected of %4, %5 codewords not corrected)%6\n"
                            "   collapse %7, carry-on %8 frames, %9 priority address words, %10 block information words")
                            .arg(QChar(block.m_phase)).arg(100.0 * phaseBer.ber(), 0, 'f', 2).arg(phaseBer.m_errorBits)
                            .arg(phaseBer.m_bits).arg(phaseBer.m_uncorrectable)
                            .arg(lowTraffic ? tr(", low traffic (the addresses end in block 0)") : QString())
                            .arg(block.m_collapse).arg(block.m_carryOn).arg(block.m_priorityWords).arg(1 + block.m_extra.size()));
                    }
                }

                const QString phaseTip = tr("Bit error rate of each phase of the last frame (%1 network): the independent\n"
                    "1600 bit/s channels of the frame, A at 1600, A and C at 3200, A to D at 6400.\n"
                    "A%3: the low traffic flag of the phase is set: its addresses end in the first block, so its pagers\n"
                    "can stop listening early and save their battery.\n"
                    "Green: no bit errors. Yellow: errors corrected. Red: errors the code could not correct. Grey: the\n"
                    "reception ended.%2")
                    .arg(pol == 1 ? tr("inverted") : tr("normal"))
                    .arg(phaseTips.isEmpty() ? QString() : "\n\n" + phaseTips.join("\n")).arg(lowTrafficSeparator);

                for (int half = 0; half < 2; half++)
                {
                    m_flexPhaseLabel[pol][half]->setToolTip(phaseTip);
                    m_flexPhaseLabel[pol][half]->setText(halves[half].isEmpty() ? QString("-") : halves[half].join("&nbsp;"));
                }

                // The mode of the frame of the column (its frame number, phases and flags are of the same frame)
                setDetail(p, FlexMode, pol, frame.m_fiwValid ? RadioPagingFlex::modeName(frame.m_mode) : QString(), live);
                setDetail(p, FlexFrame, pol, frame.m_fiwValid
                    ? QString("C%1/F%2").arg(frame.m_cycle, 2, 10, QChar('0')).arg(frame.m_frame, 3, 10, QChar('0')) : QString(), live);
                setDetail(p, FlexFlags, pol, collapse, live);
                setDetail(p, FlexRoaming, pol, frame.m_fiwValid ? (frame.m_roaming ? tr("allowed") : tr("not allowed")) : QString(), live);
                // The network words by their age: received in the last frame, in this cycle, in this hour, older
                const qint64 at = status.m_signalMs;
                setNetworkDetail(WordDateTime, pol, flexDateTimeText(t), flexWordAge(pol, WordDateTime, at),
                    t.m_timeValid && !t.m_modeConfirmed);
                {
                    const DetailRow& row = detailRows(1)[FlexTime];
                    const QString kind = flexTimeKind(t, frame);
                    m_flexWordLabel[pol][WordDateTime]->setToolTip(tr(row.m_toolTip) + (kind.isEmpty() ? QString() : "\n\n" + kind));
                }
                setNetworkDetail(WordZone, pol, flexZoneText(t), flexWordAge(pol, WordZone, at));
                setNetworkDetail(WordSsid1, pol, t.m_ssid1Valid
                    ? QString("%1, %2").arg(t.m_localId).arg(t.m_coverageZone) : QString(), flexWordAge(pol, WordSsid1, at));
                // The traffic management flags as their bits T3 T2 T1 T0
                setNetworkDetail(WordSsid2, pol, t.m_ssid2Valid
                    ? QString("%1, %2").arg(t.m_country).arg(t.m_tmf, 4, 2, QChar('0')) : QString(), flexWordAge(pol, WordSsid2, at));
                setDetail(p, FlexTime, pol, QString(" "), true);
                setDetail(p, FlexZone, pol, QString(" "), true);
                setDetail(p, FlexSystem, pol, QString(" "), true);
                setDetail(p, FlexCountry, pol, QString(" "), true);
                setDetail(p, FlexPhasesAB, pol, QString(" "), live);
                setDetail(p, FlexPhasesCD, pol, QString(" "), live);
                setDetail(p, FlexReassembling, pol, s.m_seen ? QString::number(status.m_flexReassembliesPol[pol]) : QString(), live);
            }
            else
            {
                QString page;

                if ((pol == current) && (status.m_messages[2] > 0))
                {
                    page = QString("%1, %2, %3").arg(status.m_gscNbs ? QString("NBS") : QString::number(status.m_gscPreamble))
                        .arg(status.m_gscBatch ? tr("batch") : QString("-")).arg(status.m_gscSlot);
                }

                setDetail(p, GscState, pol, !status.m_enabled[p] ? QString() : status.m_gscVoicePol[pol] ? tr("VOICE")
                    : s.m_sync ? tr("SYNC") : tr("HUNT"), live);
                setDetail(p, GscPage, pol, page, live);
            }
        }

        // The values of the protocol, across both columns
        setDetail(p, messagesRowIndex, 0, QString::number(status.m_messages[p]), true);
        setDetail(p, resetRowIndex, 0, (status.m_resetTime[p] > 0)
            ? QDateTime::fromMSecsSinceEpoch(status.m_resetTime[p]).toString("HH:mm:ss") : QString(), true);

        if (p == 1) {
            setDetail(p, FlexErs, 0, (status.m_flexErsCount > 0)
                ? QString("%1 s, %2").arg(status.m_flexErsSeconds, 0, 'f', 2).arg(status.m_flexErsCount) : QString(),
                status.m_flexErs);
        }
    }
}

// FLEX: the group slots of each polarity received, while a slot is in use or has an outcome to show. The colour of a
// slot is its state; its tooltip gives the state, the members and the frame of the group message
void RadioPagingDemodGUI::updateGroupBars(const RadioPagingDemodSink::Status& status)
{
    for (int pol = 0; pol < 2; pol++)
    {
        // Both networks always shown (the details keep their size): the slots free are grey
        const std::vector<RadioPagingFlexDecoder::GroupSlotState>& states = status.m_flexGroupStatesPol[pol];
        const RadioPagingFlexDecoder::GroupSlot *inUse[16] = {};

        for (const RadioPagingFlexDecoder::GroupSlot& slot : status.m_flexGroupsPol[pol])
        {
            if ((slot.m_slot >= 0) && (slot.m_slot < 16)) {
                inUse[slot.m_slot] = &slot;
            }
        }

        for (int s = 0; (s < 16) && (s < (int) states.size()); s++)
        {
            QLabel *cell = m_flexGroupCell[pol][s];
            const RadioPagingFlexDecoder::GroupSlotState& state = states[s];
            SlotStyle style = slotStyle(state.m_state);
            cell->setStyleSheet(style.m_background
                ? QString("QLabel { background-color : %1; color : %2; }").arg(style.m_background, style.m_text)
                : QString("QLabel { color : %1; }").arg(style.m_text));

            QString tip = tr("Group slot %1 (temporary address %2): %3").arg(s).arg(2029568 + s)
                .arg(RadioPagingFlexDecoder::groupStateName(state.m_state));

            if (state.m_state != RadioPagingFlexDecoder::GroupFree)
            {
                tip += tr(", %1 members, group message in frame %2, %3 frames ago").arg(state.m_members)
                    .arg(state.m_targetFrame, 3, 10, QChar('0')).arg(state.m_framesAgo);
            }
            if (inUse[s]) {
                tip += "\n\n" + groupSlotToolTip(*inUse[s], pol, status);
            }

            cell->setToolTip(tip);
        }
    }
}

void RadioPagingDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RadioPagingDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &RadioPagingDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->pocsagDeviation, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_pocsagDeviation_valueChanged);
    QObject::connect(ui->flexDeviation, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_flexDeviation_valueChanged);
    QObject::connect(ui->flexMaxMessageNumber, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_flexMaxMessageNumber_valueChanged);
    QObject::connect(ui->pocsagFill, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_settings.m_pocsagFill = index;
        applySettings(QStringList("pocsagFill"));
    });
    QObject::connect(ui->pocsagCharset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_settings.m_pocsagCharset = PagingComposer::codepageKey(ui->pocsagCharset);
        applySettings(QStringList("pocsagCharset"));
    });
    QObject::connect(ui->flexCharset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_settings.m_flexCharset = PagingComposer::codepageKey(ui->flexCharset);
        applySettings(QStringList("flexCharset"));
    });
    QObject::connect(ui->gscCharset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_settings.m_gscCharset = PagingComposer::codepageKey(ui->gscCharset);
        applySettings(QStringList("gscCharset"));
    });
    QObject::connect(ui->pocsagEnabled, &QAbstractButton::clicked, this, &RadioPagingDemodGUI::on_pocsagEnabled_clicked);
    QObject::connect(ui->flexEnabled, &QAbstractButton::clicked, this, &RadioPagingDemodGUI::on_flexEnabled_clicked);
    QObject::connect(ui->gscEnabled, &QAbstractButton::clicked, this, &RadioPagingDemodGUI::on_gscEnabled_clicked);
    QObject::connect(ui->pocsagDcCorrection, &QAbstractButton::clicked, this, [this](bool checked) {
        m_settings.m_pocsagDcCorrection = checked;
        applySettings(QStringList("pocsagDcCorrection"));
    });
    QObject::connect(ui->flexDcCorrection, &QAbstractButton::clicked, this, [this](bool checked) {
        m_settings.m_flexDcCorrection = checked;
        applySettings(QStringList("flexDcCorrection"));
    });
    QObject::connect(ui->gscDcCorrection, &QAbstractButton::clicked, this, [this](bool checked) {
        m_settings.m_gscDcCorrection = checked;
        applySettings(QStringList("gscDcCorrection"));
    });
    QObject::connect(ui->gscVoiceTimeout, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_gscVoiceTimeout_valueChanged);
    QObject::connect(ui->gscDeviation, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_gscDeviation_valueChanged);
    QObject::connect(ui->gscAudio, &QToolButton::toggled, this, &RadioPagingDemodGUI::on_gscAudio_toggled);
    QObject::connect(ui->gscVoiceRecord, &QToolButton::toggled, this, &RadioPagingDemodGUI::on_gscVoiceRecord_toggled);
    QObject::connect(ui->gscVoiceFiles, &QToolButton::clicked, this, &RadioPagingDemodGUI::on_gscVoiceFiles_clicked);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &RadioPagingDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->clearTable, &QToolButton::clicked, this, &RadioPagingDemodGUI::on_clearTable_clicked);
    PagingMessageTable::setupMessageCenterButton(ui->openMessageCenter);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &RadioPagingDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &RadioPagingDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingDemodGUI::on_udpPort_valueChanged);
    QObject::connect(ui->udpFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingDemodGUI::on_udpFormat_currentIndexChanged);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &RadioPagingDemodGUI::on_notifications_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &RadioPagingDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &RadioPagingDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &RadioPagingDemodGUI::on_logOpen_clicked);
    // The rows follow the filter as it is typed
    QObject::connect(ui->filterAddress, &QLineEdit::textChanged, this, &RadioPagingDemodGUI::on_filterAddress_editingFinished);
    // A member of a group: its messages (the group messages it is a member of too)
    QObject::connect(ui->messages, &PagingMessageTable::addressFilterRequested, this, [this](const QString& address) {
        ui->filterAddress->setText(QRegularExpression::escape(address));
    });
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingDemodGUI::on_channel2_currentIndexChanged);
}

void RadioPagingDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
