#ifndef INCLUDE_PLUGININSTANCEUI_H
#define INCLUDE_PLUGININSTANCEUI_H

#include <QtGlobal>
#include <QString>
#include <QByteArray>

#include "export.h"

class Message;
class MessageQueue;

class SDRBASE_API PluginInstanceGUI {
public:
	PluginInstanceGUI() { };
	virtual ~PluginInstanceGUI() { };
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

	virtual bool handleMessage(const Message& message) = 0;
};

#endif // INCLUDE_PLUGININSTANCEUI_H
