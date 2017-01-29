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

	struct TraceData
	{
        ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trace is associated with
        float m_amp;                     //!< Amplification factor
        float m_ofs;                     //!< Offset factor
        int m_traceDelay;                //!< Trace delay in number of samples

        TraceData() :
            m_projectionType(ProjectionReal),
            m_inputIndex(0),
            m_amp(1.0f),
            m_ofs(0.0f),
            m_traceDelay(0)
        {}
	};

	struct DisplayTrace
	{
	    TraceData m_traceData;           //!< Trace data
	    float *m_trace;                  //!< Displayable trace (interleaved x,y of GLfloat)
	};

	struct TriggerData
	{
        ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trigger is associated with
        Real m_triggerLevel;             //!< Level in real units
        bool m_triggerPositiveEdge;      //!< Trigger on the positive edge (else negative)
        bool m_triggerBothEdges;         //!< Trigger on both edges (else only one)
        uint32_t m_triggerDelay;         //!< Delay before the trigger is kicked off in number of samples
        uint32_t m_triggerCounts;        //!< Number of trigger conditions before the final decisive trigger

        TriggerData() :
            m_projectionType(ProjectionReal),
            m_inputIndex(0),
            m_triggerLevel(0.0f),
            m_triggerPositiveEdge(true),
            m_triggerBothEdges(false),
            m_triggerDelay(0),
            m_triggerCounts(0)
        {}
	};

    typedef std::vector<DisplayTrace> DisplayTraces;

    static const uint m_traceChunkSize;
    static const uint m_nbTriggers = 10;

    ScopeVisNG(GLScopeNG* glScope = 0);
    virtual ~ScopeVisNG();

    void setSampleRate(int sampleRate);
    void configure(uint32_t traceSize);
    void addTrace(const TraceData& traceData);
    void changeTrace(const TraceData& traceData, uint32_t traceIndex);
    void removeTrace(uint32_t traceIndex);
    void addTrigger(const TriggerData& triggerData);
    void changeTrigger(const TriggerData& triggerData, uint32_t traceIndex);
    void removeTrigger(uint32_t triggerIndex);

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& message);
    SampleVector::const_iterator getTriggerPoint() const { return m_triggerPoint; }

private:
    typedef DoubleBufferSimple<Sample> TraceBuffer;

    // === messages ===
    // ---------------------------------------------
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

    // ---------------------------------------------
    class MsgScopeVisNGAddTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGAddTrigger* create(
                const TriggerData& triggerData)
        {
            return new MsgScopeVisNGAddTrigger(triggerData);
        }

    private:
        TriggerData m_triggerData;

        MsgScopeVisNGAddTrigger(const TriggerData& triggerData) :
            m_triggerData(triggerData)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGChangeTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGChangeTrigger* create(
                const TriggerData& triggerData, uint32_t triggerIndex)
        {
            return new MsgScopeVisNGChangeTrigger(triggerData, triggerIndex);
        }

    private:
        TriggerData m_triggerData;
        uint32_t m_triggerIndex;

        MsgScopeVisNGChangeTrigger(const TriggerData& triggerData, uint32_t triggerIndex) :
            m_triggerData(triggerData),
            m_triggerIndex(triggerIndex)
        {}
    };

    // ---------------------------------------------
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

    // ---------------------------------------------
    class MsgScopeVisNGAddTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGAddTrace* create(
                const TraceData& traceData)
        {
            return new MsgScopeVisNGAddTrace(traceData);
        }

    private:
        TraceData m_traceData;

        MsgScopeVisNGAddTrace(const TraceData& traceData) :
            m_traceData(traceData)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGChangeTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGChangeTrace* create(
                const TraceData& traceData, uint32_t traceIndex)
        {
            return new MsgScopeVisNGChangeTrace(traceData, traceIndex);
        }

    private:
        TraceData m_traceData;
        uint32_t m_traceIndex;

        MsgScopeVisNGChangeTrace(TraceData traceData, uint32_t traceIndex) :
            m_traceData(traceData),
            m_traceIndex(traceIndex)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGRemoveTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGRemoveTrace* create(
                uint32_t traceIndex)
        {
            return new MsgScopeVisNGRemoveTrace(traceIndex);
        }

    private:
        uint32_t m_traceIndex;

        MsgScopeVisNGRemoveTrace(uint32_t traceIndex) :
            m_traceIndex(traceIndex)
        {}
    };

    // === projectors ===
    // ---------------------------------------------
    class Projector
    {
    public:
        Projector(ProjectionType projectionType) : m_projectionType(projectionType) {}

        ProjectionType getProjectionType() const { return m_projectionType; }
        virtual Real run(const Sample& s) = 0;
    private:
        ProjectionType m_projectionType;
    };

    // ---------------------------------------------
    class ProjectorReal : public Projector
    {
    public:
        ProjectorReal() : Projector(ProjectionReal) {}
        virtual Real run(const Sample& s) { return s.m_real / 32768.0f; }
    };

    // ---------------------------------------------
    class ProjectorImag : public Projector
    {
    public:
        ProjectorImag() : Projector(ProjectionImag) {}
        virtual Real run(const Sample& s) { return s.m_imag / 32768.0f; }
    };

    // ---------------------------------------------
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

    // ---------------------------------------------
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

    // ---------------------------------------------
    class ProjectorPhase : public Projector
    {
    public:
        ProjectorPhase() : Projector(ProjectionPhase) {}
        virtual Real run(const Sample& s) { return std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;  }
    };

    // ---------------------------------------------
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
    	TriggerData m_triggerData;    //!< Trigger data
        bool m_prevCondition;         //!< Condition (above threshold) at previous sample
        uint32_t m_triggerDelayCount; //!< Counter of samples for delay
        uint32_t m_triggerCounter;    //!< Counter of trigger occurences

        TriggerCondition(Projector *projector) :
        	m_projector(projector),
			m_prevCondition(false),
            m_triggerDelayCount(0),
            m_triggerCounter(0)
        {}
    };

    struct Trace : public DisplayTrace
    {
    	Projector *m_projector; //!< Projector transform from complex trace to real (displayable) trace
    	int m_traceCount;       //!< Count of samples processed

    	Trace(Projector *projector, Real *displayTraceBuffer) :
    		m_projector(projector),
			m_traceCount(0)
    	{
    	    m_traceData.m_projectionType = m_projector->getProjectionType();
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
    int m_sampleRate;

    void processPrevTrace(SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, std::vector<Trace>::iterator& trace);
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
