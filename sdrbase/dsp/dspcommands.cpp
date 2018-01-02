///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(DSPExit, Message)
MESSAGE_CLASS_DEFINITION(DSPAcquisitionInit, Message)
MESSAGE_CLASS_DEFINITION(DSPAcquisitionStart, Message)
MESSAGE_CLASS_DEFINITION(DSPAcquisitionStop, Message)
MESSAGE_CLASS_DEFINITION(DSPGenerationInit, Message)
MESSAGE_CLASS_DEFINITION(DSPGenerationStart, Message)
MESSAGE_CLASS_DEFINITION(DSPGenerationStop, Message)
MESSAGE_CLASS_DEFINITION(DSPGetSourceDeviceDescription, Message)
MESSAGE_CLASS_DEFINITION(DSPGetSinkDeviceDescription, Message)
MESSAGE_CLASS_DEFINITION(DSPGetErrorMessage, Message)
MESSAGE_CLASS_DEFINITION(DSPSetSource, Message)
MESSAGE_CLASS_DEFINITION(DSPSetSink, Message)
MESSAGE_CLASS_DEFINITION(DSPAddBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPAddSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPAddBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPAddThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPAddThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPAddAudioSink, Message)
MESSAGE_CLASS_DEFINITION(DSPRemoveAudioSink, Message)
//MESSAGE_CLASS_DEFINITION(DSPConfigureSpectrumVis, Message)
MESSAGE_CLASS_DEFINITION(DSPConfigureCorrection, Message)
MESSAGE_CLASS_DEFINITION(DSPEngineReport, Message)
MESSAGE_CLASS_DEFINITION(DSPConfigureScopeVis, Message)
MESSAGE_CLASS_DEFINITION(DSPSignalNotification, Message)
MESSAGE_CLASS_DEFINITION(DSPConfigureChannelizer, Message)
