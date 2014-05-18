#include "plugin/plugingui.h"

QByteArray PluginGUI::serializeGeneral() const
{
	return QByteArray();
}

bool PluginGUI::deserializeGeneral(const QByteArray& data)
{
	return false;
}

quint64 PluginGUI::getCenterFrequency() const
{
	return 0;
}
