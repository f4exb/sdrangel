///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <map>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "dtv/dtvapps.h"
#include "dtv/dtvappsdialog.h"

namespace {

constexpr int m_pidRole = Qt::UserRole;         //!< Of a carousel's items
constexpr int m_pathRole = Qt::UserRole + 1;    //!< Of a file or directory's, "" for the carousel's

QString sizeText(qint64 bytes)
{
    if (bytes < 0) {
        return QString();
    } else if (bytes < 10000) {
        return QString("%1 B").arg(bytes);
    } else {
        return QString("%1 KB").arg((bytes + 512) / 1024);
    }
}

QString expandedKey(int pid, const QString& path)
{
    return QString("%1:%2").arg(pid).arg(path);
}

} // namespace

DTVAppsDialog::DTVAppsDialog(DTVApps *apps, QWidget *parent) :
    QDialog(parent),
    m_apps(apps),
    m_serviceId(0),
    m_revision(-1)
{
    setWindowTitle("Interactive Applications");
    resize(900, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);
    m_service = new QLabel();
    layout->addWidget(m_service);

    QSplitter *splitter = new QSplitter(Qt::Vertical);

    m_appsTree = new QTreeWidget();
    m_appsTree->setHeaderLabels({"Application", "Id", "Type", "Control", "Starts from"});
    m_appsTree->setRootIsDecorated(false);
    m_appsTree->setToolTip("The service's applications, from its application information table (AIT). AUTOSTART applications start with the service; "
        "PRESENT ones are started by the user (as from the red button). Right click to copy where one starts from");
    m_appsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_appsTree, &QTreeWidget::customContextMenuRequested, this, &DTVAppsDialog::appsMenu);
    splitter->addWidget(m_appsTree);

    m_carouselsTree = new QTreeWidget();
    m_carouselsTree->setHeaderLabels({"Carousel", "Kind", "Size", "Received"});
    m_carouselsTree->setToolTip("The service's object carousels: the files of its MHEG-5 and HbbTV applications, sent round and round. "
        "Expand one to see its files, as far as they've been received");
    connect(m_carouselsTree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        const int pid = item->data(0, m_pidRole).toInt();
        const QString path = item->data(0, m_pathRole).toString();
        m_expanded.insert(expandedKey(pid, path));

        if (!item->parent() && (item->childCount() == 0)) {
            fillFiles(item);
        }
    });
    connect(m_carouselsTree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
        m_expanded.erase(expandedKey(item->data(0, m_pidRole).toInt(), item->data(0, m_pathRole).toString()));
    });
    connect(m_carouselsTree, &QTreeWidget::itemSelectionChanged, this, [this]() { m_save->setEnabled(selectedCarousel() >= 0); });
    splitter->addWidget(m_carouselsTree);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    m_save = buttons->addButton("Save Files...", QDialogButtonBox::ActionRole);
    m_save->setToolTip("Save the files of the carousel selected that have been received, to a directory");
    m_save->setEnabled(false);
    connect(m_save, &QPushButton::clicked, this, &DTVAppsDialog::saveFiles);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(&m_timer, &QTimer::timeout, this, &DTVAppsDialog::refresh);
    m_timer.start(1000);
    refresh();
}

void DTVAppsDialog::setService(int serviceId, const QString& name)
{
    if ((serviceId != m_serviceId) || (name != m_serviceName))
    {
        if (serviceId != m_serviceId) {
            m_expanded.clear();
        }

        m_serviceId = serviceId;
        m_serviceName = name;
        m_revision = -1;
        refresh();
    }
}

void DTVAppsDialog::refresh()
{
    const int revision = m_apps->getRevision();

    if (revision == m_revision) {
        return;
    }

    m_revision = revision;

    if (m_serviceId == 0) {
        m_service->setText("No service is being played");
    } else {
        m_service->setText(QString("%1 (service %2)").arg(m_serviceName.isEmpty() ? QString("Service") : m_serviceName).arg(m_serviceId));
    }

    refreshApps();
    refreshCarousels();
}

void DTVAppsDialog::refreshApps()
{
    const int transportStreamId = m_apps->getTransportStreamId();
    const int networkId = m_apps->getOriginalNetworkId();
    const int scroll = m_appsTree->verticalScrollBar()->value();
    m_appsTree->clear();

    if (m_serviceId == 0) {
        return;
    }

    for (const DTVApps::App& app : m_apps->getApps(m_serviceId))
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_appsTree);
        item->setText(0, app.m_name);
        item->setText(1, app.idText());
        item->setText(2, app.typeName());
        item->setText(3, app.controlCodeName());
        item->setText(4, app.url(networkId, transportStreamId));
        item->setToolTip(4, item->text(4));
    }

    for (int column = 0; column < 4; column++) {
        m_appsTree->resizeColumnToContents(column);
    }

    m_appsTree->verticalScrollBar()->setValue(scroll);
}

void DTVAppsDialog::refreshCarousels()
{
    const int selected = selectedCarousel();
    const int scroll = m_carouselsTree->verticalScrollBar()->value();
    m_carouselsTree->clear();

    if (m_serviceId == 0) {
        return;
    }

    for (const DTVApps::Carousel& carousel : m_apps->getCarousels(m_serviceId))
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_carouselsTree);
        item->setText(0, carousel.m_componentTag >= 0 ? QString("PID 0x%1 (component %2)").arg(carousel.m_pid, 0, 16).arg(carousel.m_componentTag)
                                                     : QString("PID 0x%1").arg(carousel.m_pid, 0, 16));
        item->setText(1, carousel.kindName());
        item->setData(0, m_pidRole, carousel.m_pid);
        item->setData(0, m_pathRole, QString());

        if (!carousel.m_received)
        {
            item->setText(3, "Not yet");
        }
        else
        {
            const DTVCarousel::Progress& progress = carousel.m_progress;
            item->setText(2, sizeText(progress.m_bytes));
            item->setText(3, carousel.m_complete ? QString("Complete: %1 files").arg(carousel.m_files)
                : QString("%1 of %2 modules, %3%: %4 files").arg(progress.m_modulesComplete).arg(progress.m_modules)
                    .arg(progress.m_bytes > 0 ? (int) (100 * progress.m_bytesComplete / progress.m_bytes) : 0).arg(carousel.m_files));
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        if (carousel.m_pid == selected) {
            item->setSelected(true);
        }

        if (carousel.m_received && (m_expanded.count(expandedKey(carousel.m_pid, QString())) != 0))
        {
            fillFiles(item);
            item->setExpanded(true);
        }
    }

    for (int column = 0; column < 4; column++) {
        m_carouselsTree->resizeColumnToContents(column);
    }

    m_carouselsTree->verticalScrollBar()->setValue(scroll);
}

// A carousel's tree, as far as it's been received: what's been listed in directories received, files received or not
void DTVAppsDialog::fillFiles(QTreeWidgetItem *carouselItem)
{
    const int pid = carouselItem->data(0, m_pidRole).toInt();
    std::map<QString, QTreeWidgetItem*> directories;
    directories[QString()] = carouselItem;

    for (const DTVCarousel::Entry& entry : m_apps->getCarouselEntries(pid))
    {
        // Entries are in path order, so a directory's before what's in it
        const int slash = entry.m_path.lastIndexOf('/');
        auto parent = directories.find(entry.m_path.left(slash));
        QTreeWidgetItem *item = new QTreeWidgetItem(parent != directories.end() ? parent->second : carouselItem);
        item->setText(0, entry.m_path.mid(slash + 1));
        item->setText(1, DTVCarousel::kindName(entry.m_kind));
        item->setText(2, sizeText(entry.m_size));
        item->setText(3, entry.m_received ? "Yes" : "No");
        item->setData(0, m_pidRole, pid);
        item->setData(0, m_pathRole, entry.m_path);

        if (entry.m_kind == DTVCarousel::Kind::Directory)
        {
            directories[entry.m_path] = item;

            if (m_expanded.count(expandedKey(pid, entry.m_path)) != 0) {
                item->setExpanded(true);
            }
        }
    }
}

int DTVAppsDialog::selectedCarousel() const
{
    const QList<QTreeWidgetItem*> items = m_carouselsTree->selectedItems();

    if (items.isEmpty()) {
        return -1;
    }

    return items[0]->data(0, m_pidRole).toInt();
}

void DTVAppsDialog::appsMenu(const QPoint& position)
{
    QTreeWidgetItem *item = m_appsTree->itemAt(position);

    if (!item) {
        return;
    }

    QMenu menu(this);
    QAction *copy = menu.addAction("Copy URL");

    if (menu.exec(m_appsTree->viewport()->mapToGlobal(position)) == copy) {
        QApplication::clipboard()->setText(item->text(4));
    }
}

void DTVAppsDialog::saveFiles()
{
    const int pid = selectedCarousel();

    if (pid < 0) {
        return;
    }

    const QString directory = QFileDialog::getExistingDirectory(this, "Save the carousel's files to");

    if (directory.isEmpty()) {
        return;
    }

    int saved = 0;
    QStringList failed;

    for (const auto& file : m_apps->getCarouselFiles(pid))
    {
        const QString path = QDir(directory).filePath(DTVCarousel::localPath(file.first));
        QDir().mkpath(QFileInfo(path).path());
        QFile out(path);

        if (out.open(QIODevice::WriteOnly) && (out.write(file.second) == file.second.size())) {
            saved++;
        } else {
            failed.append(file.first);
        }
    }

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, "Save Files", QString("%1 files saved. Couldn't save %2 files, as %3").arg(saved).arg(failed.size()).arg(failed[0]));
    } else {
        QMessageBox::information(this, "Save Files", QString("%1 files saved to %2").arg(saved).arg(QDir::toNativeSeparators(directory)));
    }
}
