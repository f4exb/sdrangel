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

#ifndef INCLUDE_FEATURE_MESSAGECENTERGUI_H_
#define INCLUDE_FEATURE_MESSAGECENTERGUI_H_

#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHash>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>
#ifdef QT_TEXTTOSPEECH_FOUND
#include <QTextToSpeech>
#endif

#include "feature/featuregui.h"
#include "gui/pagingmessagetable.h"
#include "gui/pagingcomposer.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "messagecenter.h"
#include "messagecentersettings.h"

class PluginAPI;
class FeatureUISet;
class QDateTimeEdit;
class QMenu;
class QLabel;
class QTimer;
class QToolButton;

// A field of the records offered to the conditions: its key, a readable name, its protocol (empty: common to all, or
// one seen in the log only), and the values of a list
struct MessageCenterFieldInfo
{
    QString m_key;
    QString m_name;
    QString m_protocol;
    QStringList m_values;
};

// The fields of a filter of the message center: the view of the log and the routes use it
class MessageCenterFilterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageCenterFilterWidget(QWidget *parent = nullptr);
    MessageCenterFilter filter() const;
    void setFilter(const MessageCenterFilter& filter);
    //!< The source of the messages can be chosen too (routes): channels, api and these sources
    void showSources(const QStringList& sources);
    //!< The contacts of the address book to choose from (with their icons); hidden when there is none
    void setContacts(const QList<MessageCenterContact>& contacts);
    //!< The fields offered to the conditions: the common ones, those the channels describe, those of the log
    void setFields(const QList<MessageCenterFieldInfo>& fields) { m_fieldInfos = fields; }

signals:
    void changed();
    //!< Conditions... was pressed: the owner sets the fields of the log before the dialog opens
    void fieldsWanted();

private:
    //! The message types of a protocol (lower case, empty for any) in the type list, the current one kept if it has it
    void fillTypes(const QString& protocol);
    //! The conditions on any field in a dialog: a row each (field, operator, value), all of them met
    void editConditions();
    void showConditions();

    QComboBox *m_contact;
    QComboBox *m_protocol;
    QComboBox *m_direction;
    QComboBox *m_polarity;
    QComboBox *m_type;
    QLineEdit *m_channel;
    QLineEdit *m_address;
    QLineEdit *m_text;
    QCheckBox *m_hideDuplicates;
    QCheckBox *m_voiceOnly;
    QComboBox *m_quality;               //!< Any, not damaged, clean, corrected, damaged
    QToolButton *m_conditionsButton;    //!< Conditions on any field: their number
    QList<MessageCenterContact::Condition> m_conditions;
    QList<MessageCenterFieldInfo> m_fieldInfos;
    QComboBox *m_source;
    bool m_sourcesShown = false;
    //! The filter set: what the widgets do not show of it (more than one contact, protocol, channel or source, the
    //! kind, speeds, frequencies and times) is kept unless the widget showing its first entry is changed
    MessageCenterFilter m_base;
};

class QTabWidget;
class QDialog;

class MessageCenterGUI : public FeatureGUI
{
    Q_OBJECT
public:
    static MessageCenterGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    void destroy() override;

    void resetToDefaults() override;
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    MessageQueue *getInputMessageQueue() override { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) override;
    int getWorkspaceIndex() const override { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) override { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const override { return m_settings.m_geometryBytes; }

    //! The route dialog filled from a message (its JSON record): its protocol, polarity, type and address, to the log,
    //! decoded as the codepage (charsetData; empty for none) and the type (empty for none). Called by the tables of
    //! paging messages (PagingMessageTable::createRoute)
    Q_INVOKABLE void createRouteFromRecord(const QString& recordJson, const QVariantMap& charset, const QString& type);

private:
    MessageCenter *m_center;
    MessageCenterSettings m_settings;
    RollupState m_rollupState;
    MessageQueue m_inputMessageQueue;
    bool m_doApplySettings;
    int m_shownLogged;                  //!< Records of the log when the table was made
    static const int m_maxLogRows = 100;        //!< Live: the newest messages
    static const int m_pageRows = 100;          //!< Query: a page

    // The log: live (the newest messages, as they come) or a query (a time range, a page of the messages it selects,
    // newest first, over the history)
    bool m_queryMode = false;
    QToolButton *m_liveButton;
    QToolButton *m_queryButton;
    QWidget *m_queryBar;
    QComboBox *m_range;
    QDateTimeEdit *m_from;
    QDateTimeEdit *m_to;
    QComboBox *m_refresh;
    QLabel *m_queryStatus;
    QCheckBox *m_showFragments;         //!< The fragments of FLEX messages as rows of their own
    int m_liveMatching = 0;             //!< Live: the messages of the log the filters select
    int m_countedLogged = -1;           //!< The log (logged) when they were counted
    QElapsedTimer m_countTimer;         //!< Since they were counted
    QToolButton *m_filtersButton;       //!< Shows or hides the filters of the log
    QToolButton *m_newer;
    QToolButton *m_older;
    QTimer *m_refreshTimer;
    qint64 m_queryFrom = -1;            //!< The range of the query run, fixed when it is run (the pages keep it)
    qint64 m_queryTo = -1;
    int m_queryOffset = 0;
    int m_queryTotal = 0;
    int m_queryRunning = 0;             //!< Number of the last query started; a result of an older one is dropped
    bool m_queryBusy = false;

    void buildQueryBar(QWidget *container);
    void setLogMode(bool query);
    //!< resolve: the range from now (Run, auto-refresh); else the range of the last run (pages)
    void runQuery(bool resolve);
    void showQueryResult(int number, const MessageCenterArchiveResult& result);
    static const int m_builtInSinks = 2;      //!< log and queue, first in the table of the sinks
    static const int m_builtInSources = 2;    //!< channels and api, first in the table of the sources

    // The log
    MessageCenterFilterWidget *m_logFilter;
    PagingMessageTable *m_logTable;
    // The endpoints and the queue
    QTableWidget *m_endpointsTable;
    QTableWidget *m_queueTable;
    // The page to compose
    PagingComposer *m_composer;
    QComboBox *m_toContact = nullptr;   //!< The page to the address of the composer, or to a contact
    QLabel *m_toWhere;                  //!< Where the page to the contact goes
    bool m_fillingContact = false;      //!< The composer filled with the send to of the contact
    QToolButton *m_addContact = nullptr; //!< Add the destination of the composer to the contacts
    QLabel *m_transmitterNote = nullptr; //!< What the transmitter cannot send (one-shot: no copies)
    QLabel *m_sendResult = nullptr;     //!< The last page posted: waiting, sent, or why not
    bool m_shownNetwork = true;         //!< What the composer was set to offer: copies, FLEX copies
    bool m_shownNumbering = true;
    QString m_lastPostId;
    QDateTime m_lastPostTime;
    QString m_lastPostTo;               //!< The contact, or the protocol and address
    QString m_lastPostTransmitter;
    // The routes and the files
    QTableWidget *m_routesTable;
    QTableWidget *m_contactsTable;
    QTabWidget *m_tabs;                 //!< The sections of the window
    QDialog *m_optionsDialog;           //!< The options (log size, history, claim time, offline time)
    QSpinBox *m_logSize;
    QCheckBox *m_historyEnabled;
    QSpinBox *m_historyMaxMB;
    QSpinBox *m_historyDays;
    QSpinBox *m_claimTimeout;
    QSpinBox *m_silentTime;
    QTableWidget *m_sinksTable;
    QTableWidget *m_sourcesTable;
    QPlainTextEdit *m_errors;
#ifdef QT_TEXTTOSPEECH_FOUND
    QTextToSpeech *m_speech = nullptr;
#endif

    explicit MessageCenterGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    ~MessageCenterGUI() override;

    QWidget *makeRollup(const QString& title, bool expanding);
    void buildLog(QWidget *container);
    void buildEndpoints(QWidget *container);
    void buildQueue(QWidget *container);
    void buildCompose(QWidget *container);
    void buildRoutes(QWidget *container);
    void buildContacts(QWidget *container);
    void buildFiles(QWidget *container);
    void buildOptions(QWidget *container);

    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void refresh();
    void refreshLog();
    MessageCenterFilter logFilter() const;
    void showLiveCount();
    //! The actions of the menu of a message of the log: page it again, add its sender as a contact, only its pager,
    //! requeue or cancel a page
    void messageMenu(QMenu& menu, const MessageRecord& record);
    void refreshEndpoints();
    void refreshQueue();
    void refreshComposeTransmitters();
    void applyCapabilities();
    void showSendResult();
    void showSendError(const QString& error);
    void refreshRoutes();
    QString ruleSentence(const MessageCenterEngine::Route& route) const;
    //! The messages of the log a filter selects, in a window: newest first, a page at a time
    void showMatches(const MessageCenterFilter& filter, const QString& title, QWidget *parent = nullptr);
    void refreshContacts();
    //!< -1: a new one, preset (if given) to start with
    void editContact(int index, const MessageCenterContact *preset = nullptr);
    QStringList logFieldNames(std::vector<QJsonObject> *sample = nullptr) const;
    //! The fields for the conditions: common, described by the channels (by protocol), then the others of the log
    QList<MessageCenterFieldInfo> fieldInfos() const;
    //!< A new contact of the page composed: a rule for the messages of its address, a send to of its transmitter
    MessageCenterContact contactOfPage(const RadioPagingPage& page) const;
    //!< The first contact one of whose send to is the destination of the page composed; empty if none
    QString contactOfDestination(const RadioPagingPage& page) const;
    void removeContact();
    void exportContacts();
    void importContacts();
    void refreshFiles();
    QString selectedId(QTableWidget *table) const;
    //! A route: index -1 for a new one, filled from preset when it is given
    void editRoute(int index, const MessageCenterEngine::Route *preset = nullptr);
    //! A sink or a source: index -1 for a new one
    //! The name of the sink or source saved, empty if cancelled. forRule: a new sink for the rule being edited (no
    //! rule of its own offered)
    QString editFile(bool sink, int index, bool forRule = false);
    void speak(const QString& text);
    void importFile();
    //! The messages the log filters select to a CSV file (columns of MessageRecord), as a CSV sink writes them
    void exportLog();

private slots:
    void handleInputMessages();
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void post();
    bool contactPage(RadioPagingPage& page, QString& error) const;
    void showContactPage();
    void destinationEdited();
    void updateAddContact();
    void pageAction(int action);
    void addRoute();
    void removeRoute();
    void editSelectedRoute();
    void addSink();
    void removeSink();
    void addSource();
    void removeSource();
    void editSelectedSink();
    void editSelectedSource();
    void moveRoute(int step);
    void exportRouting();
    void importRouting();
    void transmitterChanged();
};

#endif // INCLUDE_FEATURE_MESSAGECENTERGUI_H_
