#ifndef INCLUDE_TETRAPLUGIN_H
#define INCLUDE_TETRAPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class TetraPlugin : public QObject, PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID "de.maintech.sdrangelove.demod.tetra")

public:
	explicit TetraPlugin(QObject* parent = NULL);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	PluginGUI* createDemod(const QString& demodName);

private:
	static const PluginDescriptor m_pluginDescriptor;

	PluginAPI* m_pluginAPI;

private slots:
	void createInstanceTetra();
};

#endif // INCLUDE_TETRAPLUGIN_H
