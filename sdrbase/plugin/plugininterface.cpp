#include "dsp/devicesamplesource.h"
#include "plugin/plugininterface.h"
#include "plugininstanceui.h"


void PluginInterface::deleteSampleSourcePluginInstanceGUI(PluginInstanceUI *ui)
{
    ui->destroy();
}

void PluginInterface::deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source)
{
    source->destroy();
}

void PluginInterface::deleteSampleSinkPluginInstanceGUI(PluginInstanceUI *ui)
{
    ui->destroy();
}

