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

    static const uint m_traceChunkSize;
    static const uint m_nbTriggers = 10;

    ScopeVisNG(GLScopeNG* glScope = 0);
    virtual ~ScopeVisNG();

    void setSampleRate(int sampleRate);
    void configure(uint32_t traceSize, uint32_t timeOfsProMill, uint32_t triggerPre, bool freeRun);
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
            uint32_t traceSize,
            uint32_t timeOfsProMill,
			uint32_t triggerPre,
            bool freeRun)
        {
            return new MsgConfigureScopeVisNG(traceSize, timeOfsProMill, triggerPre, freeRun);
        }

        uint32_t getTraceSize() const { return m_traceSize; }
        uint32_t getTimeOfsProMill() const { return m_timeOfsProMill; }
        uint32_t getTriggerPre() const { return m_triggerPre; }
        bool getFreeRun() const { return m_freeRun; }

    private:
        uint32_t m_traceSize;
        uint32_t m_timeOfsProMill;
        uint32_t m_triggerPre;
        bool m_freeRun;

        MsgConfigureScopeVisNG(uint32_t traceSize,
                uint32_t timeOfsProMill,
				uint32_t triggerPre,
                bool freeRun) :
            m_traceSize(traceSize),
            m_timeOfsProMill(timeOfsProMill),
			m_triggerPre(triggerPre),
            m_freeRun(freeRun)
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

        const TriggerData& getTriggerData() const { return m_triggerData; }

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

        const TriggerData& getTriggerData() const { return m_triggerData; }
        uint32_t getTriggerIndex() const { return m_triggerIndex; }

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

        uint32_t getTriggerIndex() const { return m_triggerIndex; }

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

        const TraceData& getTraceData() const { return m_traceData; }

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

        const TraceData& getTraceData() const { return m_traceData; }
        uint32_t getTraceIndex() const { return m_traceIndex; }

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

        uint32_t getTraceIndex() const { return m_traceIndex; }

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
        virtual ~Projector() {}

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
        virtual ~ProjectorReal() {}
        virtual Real run(const Sample& s) { return s.m_real / 32768.0f; }
    };

    // ---------------------------------------------
    class ProjectorImag : public Projector
    {
    public:
        ProjectorImag() : Projector(ProjectionImag) {}
        virtual ~ProjectorImag() {}
        virtual Real run(const Sample& s) { return s.m_imag / 32768.0f; }
    };

    // ---------------------------------------------
    class ProjectorMagLin : public Projector
    {
    public:
        ProjectorMagLin() : Projector(ProjectionMagLin) {}
        virtual ~ProjectorMagLin() {}
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
        virtual ~ProjectorMagDB() {}
        virtual Real run(const Sample& s)
        {
            uint32_t magsq = s.m_real*s.m_real + s.m_imag*s.m_imag;
            //return mult * log2f(magsq/1073741824.0f);
            return log10f(magsq/1073741824.0f) * 10.0f;
        }
    private:
        static const Real mult;
    };

    // ---------------------------------------------
    class ProjectorPhase : public Projector
    {
    public:
        ProjectorPhase() : Projector(ProjectionPhase) {}
        virtual ~ProjectorPhase() {}
        virtual Real run(const Sample& s) { return std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;  }
    };

    // ---------------------------------------------
    class ProjectorDPhase : public Projector
    {
    public:
        ProjectorDPhase() : Projector(ProjectionDPhase), m_prevArg(0.0f) {}
        virtual ~ProjectorDPhase() {}
        virtual Real run(const Sample& s)
        {
            Real curArg = std::atan2((float) s.m_imag, (float) s.m_real);
            Real dPhi = (curArg - m_prevArg) / M_PI;
            m_prevArg = curArg;

            if (dPhi < -1.0f) {
                dPhi += 2.0f;
            } else if (dPhi > 1.0f) {
                dPhi -= 2.0f;
            }

            return dPhi;

//            Real dPhi = curArg - m_prevArg;
//            m_prevArg = curArg;
//
//            if (dPhi < -M_PI) {
//                dPhi += 2.0 * M_PI;
//            } else if (dPhi > M_PI) {
//                dPhi -= 2.0 * M_PI;
//            }
//
//            return dPhi/M_PI;
        }

    private:
        Real m_prevArg;
    };

    static Projector *createProjector(ProjectionType projectionType)
    {
        //qDebug("ScopeVisNG::createProjector: projectionType: %d", projectionType);
        switch (projectionType)
        {
        case ProjectionImag:
            return new ProjectorImag();
        case ProjectionMagLin:
            return new ProjectorMagLin();
        case ProjectionMagDB:
            return new ProjectorMagDB();
        case ProjectionPhase:
            return new ProjectorPhase();
        case ProjectionDPhase:
            return new ProjectorDPhase();
        case ProjectionReal:
        default:
            return new ProjectorReal();
        }
    }

    /**
     * Trigger stuff
     */
    enum TriggerState
    {
        TriggerUntriggered, //!< Trigger is not kicked off yet (or trigger list is empty)
        TriggerTriggered,   //!< Trigger has been kicked off
        TriggerWait,        //!< In one shot mode trigger waits for manual re-enabling
        TriggerDelay,       //!< Trigger conditions have been kicked off but it is waiting for delay before final kick off
        TriggerNewConfig,   //!< Special condition when a new configuration has been received
    };

    struct TriggerCondition
    {
    public:
        Projector *m_projector;
        TriggerData m_triggerData;    //!< Trigger data
        bool m_prevCondition;         //!< Condition (above threshold) at previous sample
        uint32_t m_triggerDelayCount; //!< Counter of samples for delay
        uint32_t m_triggerCounter;    //!< Counter of trigger occurences

        TriggerCondition(const TriggerData& triggerData) :
            m_projector(0),
            m_triggerData(triggerData),
            m_prevCondition(false),
            m_triggerDelayCount(0),
            m_triggerCounter(0)
        {
        }

        ~TriggerCondition()
        {
            if (m_projector) delete m_projector;
        }

        void init()
        {
            m_projector = createProjector(m_triggerData.m_projectionType);
        }

        void setData(const TriggerData& triggerData)
        {
            m_triggerData = triggerData;

            if (m_projector->getProjectionType() != m_triggerData.m_projectionType)
            {
                delete m_projector;
                m_projector = createProjector(m_triggerData.m_projectionType);
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
    		m_endPoint = m_traceBuffer.getCurrent();
    	}

    	void resize(uint32_t size)
    	{
    		m_traceBuffer.resize(size);
    	}

    	void reset()
    	{
    	    m_traceBuffer.reset();
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
    	uint32_t m_traceSize;

    	/**
    	 * Give memory size in number of traces
    	 */
    	TraceBackDiscreteMemory(uint32_t size) : m_memSize(size), m_currentMemIndex(0), m_traceSize(0)
    	{
    		m_traceBackBuffers.resize(m_memSize);
    	}

    	/**
    	 * Resize all trace buffers in memory
    	 */
    	void resize(uint32_t size)
    	{
    	    m_traceSize = size;

    		for (std::vector<TraceBackBuffer>::iterator it = m_traceBackBuffers.begin(); it != m_traceBackBuffers.end(); ++it)
    		{
    			it->resize(4*m_traceSize);
    		}
    	}

    	/**
    	 * Move index forward by one position and return reference to the trace at this position
    	 * Copy a trace length of samples into the new memory slot
    	 */
    	TraceBackBuffer &store()
    	{
    	    uint32_t nextMemIndex = m_currentMemIndex < (m_memSize-1) ? m_currentMemIndex+1 : 0;
            m_traceBackBuffers[nextMemIndex].reset();
            m_traceBackBuffers[nextMemIndex].write(m_traceBackBuffers[m_currentMemIndex].m_endPoint - m_traceSize,
                    m_traceBackBuffers[m_currentMemIndex].m_endPoint);
    	    m_currentMemIndex = nextMemIndex;
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

    	/**
    	 * Return current memory index
    	 */
    	uint32_t currentIndex() const { return m_currentMemIndex; }
    };

    /**
     * Displayable trace stuff
     */
    struct TraceControl
    {
        Projector *m_projector; //!< Projector transform from complex trace to real (displayable) trace
        int m_traceCount[2];    //!< Count of samples processed (double buffered)

        TraceControl() : m_projector(0)
        {
            reset();
        }

        ~TraceControl()
        {
            if (m_projector) delete m_projector;
        }

        void init(ProjectionType projectionType)
        {
            if (m_projector) delete m_projector;
            m_projector = createProjector(projectionType);
        }

        void reset()
        {
            m_traceCount[0] = 0;
            m_traceCount[1] = 0;
        }
    };

    struct Traces
    {
        std::vector<TraceControl> m_tracesControl;    //!< Corresponding traces control data
        std::vector<TraceData> m_tracesData;          //!< Corresponding traces data
        std::vector<float *> m_traces[2];             //!< Double buffer of traces processed by glScope
        int m_traceSize;                              //!< Current size of a trace in buffer
        int m_maxTraceSize;                           //!< Maximum Size of a trace in buffer
        bool evenOddIndex;                            //!< Even (true) or odd (false) index

        Traces() : evenOddIndex(true), m_traceSize(0), m_maxTraceSize(0) {}

        ~Traces()
        {
            std::vector<float *>::iterator it0 = m_traces[0].begin();
            std::vector<float *>::iterator it1 = m_traces[1].begin();

            for (; it0 != m_traces[0].end(); ++it0, ++it1)
            {
                delete[] (*it0);
                delete[] (*it1);
            }
        }

        void addTrace(const TraceData& traceData, int traceSize)
        {
            resize(traceSize);

            m_tracesData.push_back(traceData);
            m_tracesControl.push_back(TraceControl());
            m_tracesControl.back().init(traceData.m_projectionType);
            float *x0 = new float[2*m_traceSize];
            float *x1 = new float[2*m_traceSize];
            m_traces[0].push_back(x0);
            m_traces[1].push_back(x1);
        }

        void changeTrace(const TraceData& traceData, uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size()) {
                m_tracesControl[traceIndex].init(traceData.m_projectionType);
                m_tracesData[traceIndex] = traceData;
            }
        }

        void removeTrace(uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size())
            {
                m_tracesControl.erase(m_tracesControl.begin() + traceIndex);
                m_tracesData.erase(m_tracesData.begin() + traceIndex);
                delete[] (m_traces[0])[traceIndex];
                delete[] (m_traces[1])[traceIndex];
                m_traces[0].erase(m_traces[0].begin() + traceIndex);
                m_traces[1].erase(m_traces[1].begin() + traceIndex);
            }
        }

        void resize(int traceSize)
        {
            m_traceSize = traceSize;

            if (m_traceSize > m_maxTraceSize)
            {
                std::vector<float *>::iterator it0 = m_traces[0].begin();
                std::vector<float *>::iterator it1 = m_traces[1].begin();

                for (; it0 != m_traces[0].end(); ++it0, ++it1)
                {
                    delete[] (*it0);
                    delete[] (*it1);
                    *it0 = new float[2*m_traceSize];
                    *it1 = new float[2*m_traceSize];
                }

                m_maxTraceSize = m_traceSize;
            }
        }

        uint32_t currentBufferIndex() const { return evenOddIndex? 0 : 1; }
        uint32_t size() const { return m_tracesControl.size(); }

        void switchBuffer()
        {
            evenOddIndex = !evenOddIndex;

            for (std::vector<TraceControl>::iterator it = m_tracesControl.begin(); it != m_tracesControl.end(); ++it)
            {
                it->m_traceCount[currentBufferIndex()] = 0;
            }
        }
    };

    class TriggerComparator
    {
    public:
        TriggerComparator() : m_level(0), m_reset(true)
        {
            computeLevels();
        }

        bool triggered(const Sample& s, TriggerCondition& triggerCondition)
        {
            if (triggerCondition.m_triggerData.m_triggerLevel != m_level)
            {
                m_level = triggerCondition.m_triggerData.m_triggerLevel;
                computeLevels();
            }

            bool condition, trigger;

            if (triggerCondition.m_projector->getProjectionType() == ProjectionMagDB) {
                condition = triggerCondition.m_projector->run(s) > m_levelPwoerDB;
            } else if (triggerCondition.m_projector->getProjectionType() == ProjectionMagLin) {
                condition = triggerCondition.m_projector->run(s) > m_levelPowerLin;
            } else {
                condition = triggerCondition.m_projector->run(s) > m_level;
            }

            if (m_reset)
            {
                triggerCondition.m_prevCondition = condition;
                m_reset = false;
                return false;
            }

            if (triggerCondition.m_triggerData.m_triggerBothEdges) {
                trigger = triggerCondition.m_prevCondition ? !condition : condition; // This is a XOR between bools
            } else if (triggerCondition.m_triggerData.m_triggerPositiveEdge) {
                trigger = !triggerCondition.m_prevCondition && condition;
            } else {
                trigger = triggerCondition.m_prevCondition && !condition;
            }

//            if (trigger) {
//                qDebug("ScopeVisNG::triggered: %s/%s %f/%f",
//                        triggerCondition.m_prevCondition ? "T" : "F",
//                        condition ? "T" : "F",
//                        triggerCondition.m_projector->run(s),
//                        triggerCondition.m_triggerData.m_triggerLevel);
//            }

            triggerCondition.m_prevCondition = condition;
            return trigger;
        }

        void reset()
        {
            m_reset = true;
        }

    private:
        void computeLevels()
        {
            m_levelPowerLin = m_level + 1.0f;
            m_levelPwoerDB = (100.0f * (m_level - 1.0f));
        }

        Real m_level;
        Real m_levelPwoerDB;
        Real m_levelPowerLin;
        bool m_reset;
    };

    GLScopeNG* m_glScope;
    uint32_t m_preTriggerDelay;                    //!< Pre-trigger delay in number of samples
    std::vector<TriggerCondition> m_triggerConditions; //!< Chain of triggers
    int m_currentTriggerIndex;                     //!< Index of current index in the chain
    TriggerState m_triggerState;                   //!< Current trigger state
    Traces m_traces;                               //!< Displayable traces
    int m_traceSize;                               //!< Size of traces in number of samples
    int m_nbSamples;                               //!< Number of samples yet to process in one complex trace
    int m_timeOfsProMill;                          //!< Start trace shift in 1/1000 trace size
    bool m_traceStart;                             //!< Trace is at start point
    int m_traceFill;                               //!< Count of samples accumulated into trace
    int m_zTraceIndex;                             //!< Index of the trace used for Z input (luminance or false colors)
    SampleVector::const_iterator m_triggerPoint;   //!< Trigger start location in the samples vector
    int m_sampleRate;
    TraceBackDiscreteMemory m_traceDiscreteMemory; //!< Complex trace memory for triggered states TODO: vectorize when more than on input is allowed
    bool m_freeRun;                                //!< True if free running (trigger globally disabled)
    int m_maxTraceDelay;                           //!< Maximum trace delay
    TriggerComparator m_triggerComparator;         //!< Compares sample level to trigger level

    /**
     * Moves on to the next trigger if any or increments trigger count if in repeat mode
     * - If not final it returns true
     * - If final i.e. signal is actually triggerd it returns false
     */
    bool nextTrigger(); //!< Returns true if not final

    /**
     * Process a sample trace which length is at most the trace length (m_traceSize)
     */
    void processTrace(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, int& triggerPoint);

    /**
     * Process traces from complex trace memory buffer.
     * - if finished it returns the number of unprocessed samples left in the buffer
     * - if not finished it returns -1
     */
    int processTraces(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool traceBack = false);

    /**
     * Get maximum trace delay
     */
    void updateMaxTraceDelay();

    /**
     * Initialize trace buffers
     */
    void initTraceBuffers();
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
