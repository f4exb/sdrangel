#ifndef INCLUDE_PLUGININSTANCEUI_H
#define INCLUDE_PLUGININSTANCEUI_H

#include <QtGlobal>
#include <QString>
#include <QByteArray>

#include "util/export.h"

class Message;
class MessageQueue;

class SDRANGEL_API PluginInstanceGUI {
public:
	PluginInstanceGUI() { };
	virtual ~PluginInstanceGUI() { };

	virtual void destroy() = 0;

	virtual void setName(const QString& name) = 0;
	virtual QString getName() const = 0;

	virtual void resetToDefaults() = 0;

	virtual qint64 getCenterFrequency() const = 0;
	virtual void setCenterFrequency(qint64 centerFrequency) = 0;

	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

	virtual bool handleMessage(const Message& message) = 0;
};

#endif // INCLUDE_PLUGININSTANCEUI_H
