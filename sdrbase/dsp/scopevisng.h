///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_SCOPEVISNG_H_
#define SDRBASE_DSP_SCOPEVISNG_H_

#include <stdint.h>
#include <vector>
#include <boost/circular_buffer.hpp>
#include "dsp/dsptypes.h"
#include "dsp/basebandsamplesink.h"
#include "util/export.h"
#include "util/message.h"
#include "util/doublebuffer.h"

class GLScopeNG;

class SDRANGEL_API ScopeVisNG : public BasebandSampleSink {

public:
	enum ProjectionType
	{
		ProjectionReal,    //!< Extract real part
		ProjectionImag,    //!< Extract imaginary part
		ProjectionMagLin,  //!< Calculate linear magnitude or modulus
		ProjectionMagDB,   //!< Calculate logarithmic (dB) of squared magnitude
		ProjectionPhase,   //!< Calculate phase
		ProjectionDPhase   //!< Calculate phase derivative i.e. instantaneous frequency scaled to sample rate
	};

	struct DisplayTrace
	{
	    float *m_trace;                  //!< Displayable trace (interleaved x,y of GLfloat)
	    ProjectionType m_projectionType; //!< Complex to real projection type
	    float m_amp;                     //!< Amplification factor
	    float m_ofs;                     //!< Offset factor
	};

    typedef std::vector<DisplayTrace> DisplayTraces;

    static const uint m_traceChunkSize;
    static const uint m_nbTriggers = 10;

    ScopeVisNG(GLScopeNG* glScope = 0);
    virtual ~ScopeVisNG();

    void configure(MessageQueue* msgQueue,
            uint32_t traceSize);

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& message);
    SampleVector::const_iterator getTriggerPoint() const { return m_triggerPoint; }

private:
    typedef DoubleBufferSimple<Sample> TraceBuffer;

    class MsgConfigureScopeVisNG : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgConfigureScopeVisNG* create(
            uint32_t traceSize)
        {
            return new MsgConfigureScopeVisNG(traceSize);
        }

    private:
        uint32_t m_traceSize;

        MsgConfigureScopeVisNG(uint32_t traceSize) :
            m_traceSize(traceSize)
        {}
    };

    class MsgScopeVisNGAddTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGAddTrigger* create(
        		ProjectionType projectionType)
        {
            return new MsgScopeVisNGAddTrigger(projectionType);
        }

    private:
        ProjectionType m_projectionType;

        MsgScopeVisNGAddTrigger(ProjectionType projectionType) :
        	m_projectionType(projectionType)
        {}
    };

    class MsgScopeVisNGRemoveTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGRemoveTrigger* create(
                uint32_t triggerIndex)
        {
            return new MsgScopeVisNGRemoveTrigger(triggerIndex);
        }

    private:
        uint32_t m_triggerIndex;

        MsgScopeVisNGRemoveTrigger(uint32_t triggerIndex) :
        	m_triggerIndex(triggerIndex)
        {}
    };

    class Projector
    {
    public:
        Projector(ProjectionType projectionType) : m_projectionType(projectionType) {}

        ProjectionType getProjectionType() const { return m_projectionType; }
        virtual Real run(const Sample& s) = 0;
    private:
        ProjectionType m_projectionType;
    };

    class ProjectorReal : public Projector
    {
    public:
        ProjectorReal() : Projector(ProjectionReal) {}
        virtual Real run(const Sample& s) { return s.m_real / 32768.0f; }
    };

    class ProjectorImag : public Projector
    {
    public:
        ProjectorImag() : Projector(ProjectionImag) {}
        virtual Real run(const Sample& s) { return s.m_imag / 32768.0f; }
    };

    class ProjectorMagLin : public Projector
    {
    public:
        ProjectorMagLin() : Projector(ProjectionMagLin) {}
        virtual Real run(const Sample& s)
        {
            uint32_t magsq = s.m_real*s.m_real + s.m_imag*s.m_imag;
            return std::sqrt(magsq/1073741824.0f);
        }
    };

    class ProjectorMagDB : public Projector
    {
    public:
        ProjectorMagDB() : Projector(ProjectionMagDB) {}
        virtual Real run(const Sample& s)
        {
            uint32_t magsq = s.m_real*s.m_real + s.m_imag*s.m_imag;
            return mult * log2f(magsq/1073741824.0f);
        }
    private:
        static const Real mult;
    };

    class ProjectorPhase : public Projector
    {
    public:
        ProjectorPhase() : Projector(ProjectionPhase) {}
        virtual Real run(const Sample& s) { return std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;  }
    };

    class ProjectorDPhase : public Projector
    {
    public:
        ProjectorDPhase() : Projector(ProjectionDPhase), m_prevArg(0.0f) {}
        virtual Real run(const Sample& s)
        {
            Real curArg = std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;
            Real dPhi = curArg - m_prevArg;
            m_prevArg = curArg;

            if (dPhi < -M_PI) {
                dPhi += 2.0 * M_PI;
            } else if (dPhi > M_PI) {
                dPhi -= 2.0 * M_PI;
            }

            return dPhi;
        }

    private:
        Real m_prevArg;
    };

    enum TriggerState
	{
        TriggerFreeRun,     //!< Trigger is disabled
    	TriggerUntriggered, //!< Trigger is not kicked off yet (or trigger list is empty)
		TriggerTriggered,   //!< Trigger has been kicked off
		TriggerWait,        //!< In one shot mode trigger waits for manual re-enabling
		TriggerDelay,       //!< Trigger conditions have been kicked off but it is waiting for delay before final kick off
		TriggerNewConfig,   //!< Special condition when a new configuration has been received
	};

    struct TriggerCondition
    {
    public:
    	Projector *m_projector;       //!< Projector transform from complex trace to reaL trace usable for triggering
    	uint32_t m_inputIndex;        //!< Input or feed index this trigger is associated with
        Real m_triggerLevel;          //!< Level in real units
        bool m_triggerPositiveEdge;   //!< Trigger on the positive edge (else negative)
        bool m_triggerBothEdges;      //!< Trigger on both edges (else only one)
        bool m_prevCondition;         //!< Condition (above threshold) at previous sample
        uint32_t m_triggerDelay;      //!< Delay before the trigger is kicked off in number of samples
        uint32_t m_triggerDelayCount; //!< Counter of samples for delay
        uint32_t m_triggerCounts;     //!< Number of trigger conditions before the final decisive trigger

        TriggerCondition(Projector *projector) :
        	m_projector(projector),
			m_inputIndex(0),
            m_triggerLevel(0.0f),
            m_triggerPositiveEdge(true),
            m_triggerBothEdges(false),
			m_prevCondition(false),
            m_triggerDelay(0),
            m_triggerDelayCount(0),
            m_triggerCounts(0)
        {}
    };

    struct Trace : public DisplayTrace
    {
    	Projector *m_projector; //!< Projector transform from complex trace to real (displayable) trace
    	uint32_t m_inputIndex;  //!< Input or feed index this trace is associated with
    	int m_traceDelay;       //!< Trace delay in number of samples
    	int m_traceCount;       //!< Count of samples processed
    	float m_amp;            //!< Linear trace amplifier factor
    	float m_shift;          //!< Linear trace shift

    	Trace(Projector *projector, Real *displayTraceBuffer) :
    		m_projector(projector),
    		m_inputIndex(0),
			m_traceDelay(0),
			m_traceCount(0),
			m_amp(1.0f),
			m_shift(0.0f)
    	{
    	    m_projectionType = m_projector->getProjectionType();
    	    m_trace = displayTraceBuffer;
    	}
    };

    GLScopeNG* m_glScope;
    std::vector<TraceBuffer> m_tracebackBuffers; //!< One complex (Sample type) trace buffer per input source or feed
    DoubleBufferSimple<Sample> m_traceback;      //!< FIFO to handle delayed processes
    int m_preTriggerDelay;                       //!< Pre-trigger delay in number of samples
    std::vector<TriggerCondition> m_triggerConditions; //!< Chain of triggers
    int m_currentTriggerIndex;                   //!< Index of current index in the chain
    TriggerState m_triggerState;                 //!< Current trigger state
    std::vector<Trace> m_traces;                 //!< One trace control object per display trace allocated to X, Y[n] or Z
    int m_traceSize;                             //!< Size of traces in number of samples
    int m_timeOfsProMill;                        //!< Start trace shift in 1/1000 trace size
    bool m_traceStart;                           //!< Trace is at start point
    int m_traceFill;                             //!< Count of samples accumulated into trace
    int m_zTraceIndex;                           //!< Index of the trace used for Z input (luminance or false colors)
    int m_traceCompleteCount;                    //!< Count of completed traces
    SampleVector::const_iterator m_triggerPoint; //!< Trigger start location in the samples vector

    void processPrevTrace(SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, std::vector<Trace>::iterator& trace);
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
