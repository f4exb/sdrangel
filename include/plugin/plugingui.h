#ifndef INCLUDE_PLUGINGUI_H
#define INCLUDE_PLUGINGUI_H

#include <QWidget>
#include "util/export.h"

class Message;

class SDRANGELOVE_API PluginGUI {
public:
	PluginGUI() { };
	virtual void destroy() = 0;

	virtual void setName(const QString& name) = 0;

	virtual void resetToDefaults() = 0;

	virtual QByteArray serializeGeneral() const;
	virtual bool deserializeGeneral(const QByteArray& data);
	virtual quint64 getCenterFrequency() const;

	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;

	virtual bool handleMessage(Message* message) = 0;
};

#endif // INCLUDE_PLUGINGUI_H
