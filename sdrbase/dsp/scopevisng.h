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

        DisplayTrace(const TraceData& traceData) :
            m_traceData(traceData),
            m_trace(0)
        {}
    };

    struct TriggerData
    {
        ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trigger is associated with
        Real m_triggerLevel;             //!< Level in real units
        bool m_triggerPositiveEdge;      //!< Trigger on the positive edge (else negative)
        bool m_triggerBothEdges;         //!< Trigger on both edges (else only one)
        uint32_t m_triggerDelay;         //!< Delay before the trigger is kicked off in number of samples
        uint32_t m_triggerRepeat;        //!< Number of trigger conditions before the final decisive trigger

        TriggerData() :
            m_projectionType(ProjectionReal),
            m_inputIndex(0),
            m_triggerLevel(0.0f),
            m_triggerPositiveEdge(true),
            m_triggerBothEdges(false),
            m_triggerDelay(0),
			m_triggerRepeat(0)
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

        uint32_t getTraceSize() const { return m_traceSize; }

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
        virtual Real run(const Sample& s) {}
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


    /**
     * Trigger stuff
     */
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

        TriggerCondition(const TriggerData& triggerData) :
            m_triggerData(triggerData),
            m_prevCondition(false),
            m_triggerDelayCount(0),
            m_triggerCounter(0)
        {
            m_projector = new Projector(m_triggerData.m_projectionType);
        }

        ~TriggerCondition() { delete m_projector; }

        void setData(const TriggerData& triggerData)
        {
            m_triggerData = triggerData;

            if (m_projector->getProjectionType() != m_triggerData.m_projectionType)
            {
                delete m_projector;
                m_projector = new Projector(m_triggerData.m_projectionType);
            }

            m_prevCondition = false;
            m_triggerDelayCount = 0;
            m_triggerCounter = 0;
        }
    };

    /**
     * Complex trace stuff
     */
    typedef DoubleBufferSimple<Sample> TraceBuffer;

    struct TraceBackBuffer
    {
    	TraceBuffer m_traceBuffer;
    	SampleVector::iterator m_endPoint;

    	TraceBackBuffer()
    	{
    		m_startPoint = m_traceBuffer.getCurrent();
    		m_endPoint = m_traceBuffer.getCurrent();
    	}

    	void resize(uint32_t size)
    	{
    		m_traceBuffer.resize(size);
    	}

    	void write(const SampleVector::const_iterator begin, const SampleVector::const_iterator end)
    	{
    		m_traceBuffer.write(begin, end);
    	}

    	unsigned int absoluteFill() const {
    		return m_traceBuffer.absoluteFill();
    	}

    	SampleVector::iterator current() { return m_traceBuffer.getCurrent(); }
    };

    struct TraceBackDiscreteMemory
    {
    	std::vector<TraceBackBuffer> m_traceBackBuffers;
    	uint32_t m_memSize;
    	uint32_t m_currentMemIndex;

    	/**
    	 * Give memory size in number of traces
    	 */
    	TraceBackDiscreteMemory(uint32_t size) : m_memSize(size), m_currentMemIndex(0)
    	{
    		m_traceBackBuffers.resize(m_memSize);
    	}

    	/**
    	 * Resize all trace buffers in memory
    	 */
    	void resize(uint32_t size)
    	{
    		for (std::vector<TraceBackBuffer>::iterator it = m_traceBackBuffers.begin(); it != m_traceBackBuffers.end(); ++it)
    		{
    			it->resize(size);
    		}
    	}

    	/**
    	 * Move index forward by one position and return reference to the trace at this position
    	 */
    	TraceBackBuffer &store()
    	{
    		m_currentMemIndex = m_currentMemIndex < m_memSize ? m_currentMemIndex+1 : 0;
    		m_traceBackBuffers[m_currentMemIndex].reset();
    		return m_traceBackBuffers[m_currentMemIndex]; // new trace
    	}

    	/**
    	 * Recalls trace at shift positions back. Therefore 0 is current. Wraps around memory size.
    	 */
    	TraceBackBuffer& recall(uint32_t shift)
    	{
    		int index = (m_currentMemIndex + (m_memSize - (shift % m_memSize))) % m_memSize;
    		return m_traceBackBuffers[index];
    	}

    	/**
    	 * Return trace at current memory position
    	 */
    	TraceBackBuffer& current()
    	{
    		return m_traceBackBuffers[m_currentMemIndex];
    	}
    };

    /**
     * Displayable trace stuff
     */
    struct Trace : public DisplayTrace
    {
        Projector *m_projector; //!< Projector transform from complex trace to real (displayable) trace
        int m_traceSize;        //!< Size of the trace in buffer
        int m_maxTraceSize;
        int m_traceCount;       //!< Count of samples processed

        Trace(const TraceData& traceData, int traceSize) :
            DisplayTrace(traceData),
            m_traceSize(traceSize),
            m_maxTraceSize(traceSize),
            m_traceCount(0)
        {
            m_projector = new Projector(m_traceData.m_projectionType);
            m_trace = new float[2*traceSize];
        }

        ~Trace()
        {
            delete m_projector;
            delete[] m_trace;
        }

        void setData(const TraceData& traceData)
        {
            m_traceData = traceData;

            if (m_projector->getProjectionType() != m_traceData.m_projectionType)
            {
                delete m_projector;
                m_projector = new Projector(m_traceData.m_projectionType);
            }
        }

        void resize(int traceSize)
        {
            m_traceSize = traceSize;
            m_traceCount = 0;

            if (m_traceSize > m_maxTraceSize)
            {
                delete[] m_trace;
                m_trace = new float[2*m_traceSize];
                m_maxTraceSize = m_traceSize;
            }
        }
    };

    GLScopeNG* m_glScope;
    int m_preTriggerDelay;                         //!< Pre-trigger delay in number of samples
    std::vector<TriggerCondition> m_triggerConditions; //!< Chain of triggers
    int m_currentTriggerIndex;                     //!< Index of current index in the chain
    TriggerState m_triggerState;                   //!< Current trigger state
    std::vector<Trace> m_traces;                   //!< One trace control object per display trace allocated to X, Y[n] or Z
    int m_traceSize;                               //!< Size of traces in number of samples
    int m_memTraceSize;                            //!< Trace size in memory in number of samples up to trace size
    int m_timeOfsProMill;                          //!< Start trace shift in 1/1000 trace size
    bool m_traceStart;                             //!< Trace is at start point
    int m_traceFill;                               //!< Count of samples accumulated into trace
    int m_zTraceIndex;                             //!< Index of the trace used for Z input (luminance or false colors)
    int m_traceCompleteCount;                      //!< Count of completed traces
    SampleVector::const_iterator m_triggerPoint;   //!< Trigger start location in the samples vector
    int m_sampleRate;
    TraceBackDiscreteMemory m_traceDiscreteMemory; //!< Complex trace memory for triggered states TODO: vectorize when more than on input is allowed

    bool nextTrigger();
    void processPrevTraces(int beginPoint, int endPoint, TraceBackBuffer& traceBuffer);
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
