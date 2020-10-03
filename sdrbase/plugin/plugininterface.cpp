#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "plugin/plugininterface.h"


void PluginInterface::deleteSampleSourcePluginInstanceInput(DeviceSampleSource *source)
{
    if (source) { source->destroy(); }
}

void PluginInterface::deleteSampleSinkPluginInstanceOutput(DeviceSampleSink *sink)
{
    if (sink) { sink->destroy(); }
}

void PluginInterface::deleteSampleMIMOPluginInstanceMIMO(DeviceSampleMIMO *mimo)
{
    if (mimo) { mimo->destroy(); }
}
