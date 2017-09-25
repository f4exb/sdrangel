#include <plugin/plugininstancegui.h>
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "plugin/plugininterface.h"


void PluginInterface::deleteSampleSourcePluginInstanceGUI(PluginInstanceGUI *ui)
{
    ui->destroy();
}

void PluginInterface::deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source)
{
    source->destroy();
}

void PluginInterface::deleteSampleSinkPluginInstanceGUI(PluginInstanceGUI *ui)
{
    ui->destroy();
}

void PluginInterface::deleteSampleSinkPluginInstanceOutput(DeviceSampleSink *sink)
{
    sink->destroy();
}
