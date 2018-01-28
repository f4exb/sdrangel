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

#include <QDebug>
#include <QColor>
#include <algorithm>
#include <utility>
#include <math.h>

#include <stdint.h>
#include <vector>
#include <boost/circular_buffer.hpp>
#include "dsp/dsptypes.h"
#include "dsp/basebandsamplesink.h"
#include "util/export.h"
#include "util/message.h"
#include "util/doublebuffer.h"

#undef M_PI
#define M_PI		3.14159265358979323846

class GLScopeNG;

class SDRANGEL_API ScopeVisNG : public BasebandSampleSink {

public:
    enum ProjectionType
    {
        ProjectionReal = 0, //!< Extract real part
        ProjectionImag,     //!< Extract imaginary part
        ProjectionMagLin,   //!< Calculate linear magnitude or modulus
        ProjectionMagDB,    //!< Calculate logarithmic (dB) of squared magnitude
        ProjectionPhase,    //!< Calculate phase
        ProjectionDPhase,   //!< Calculate phase derivative i.e. instantaneous frequency scaled to sample rate
		nbProjectionTypes   //!< Gives the number of projections in the enum
    };

    struct TraceData
    {
        ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trace is associated with
        float m_amp;                     //!< Amplification factor
        uint32_t m_ampIndex;             //!< Index in list of amplification factors
        float m_ofs;                     //!< Offset factor
        int m_ofsCoarse;                 //!< Coarse offset slider value
        int m_ofsFine;                   //!< Fine offset slider value
        int m_traceDelay;                //!< Trace delay in number of samples
        int m_traceDelayCoarse;          //!< Coarse delay slider value
        int m_traceDelayFine;            //!< Fine delay slider value
        float m_triggerDisplayLevel;     //!< Displayable trigger display level in -1:+1 scale. Off scale if not displayable.
        QColor m_traceColor;             //!< Trace display color
        float m_traceColorR;             //!< Trace display color - red shortcut
        float m_traceColorG;             //!< Trace display color - green shortcut
        float m_traceColorB;             //!< Trace display color - blue shortcut
        bool m_hasTextOverlay;           //!< True if a text overlay has to be displayed
        QString m_textOverlay;           //!< Text overlay to display
        bool m_viewTrace;                //!< Trace visibility

        TraceData() :
            m_projectionType(ProjectionReal),
            m_inputIndex(0),
            m_amp(1.0f),
            m_ampIndex(0),
            m_ofs(0.0f),
            m_ofsCoarse(0),
            m_ofsFine(0),
            m_traceDelay(0),
            m_traceDelayCoarse(0),
            m_traceDelayFine(0),
			m_triggerDisplayLevel(2.0),  // OVer scale by default (2.0)
			m_traceColor(255,255,64),
			m_hasTextOverlay(false),
			m_viewTrace(true)
        {
            setColor(m_traceColor);
        }

        void setColor(QColor color)
        {
            m_traceColor = color;
            qreal r,g,b,a;
            m_traceColor.getRgbF(&r, &g, &b, &a);
            m_traceColorR = r;
            m_traceColorG = g;
            m_traceColorB = b;
        }
    };


    struct TriggerData
    {
        ProjectionType m_projectionType; //!< Complex to real projection type
        uint32_t m_inputIndex;           //!< Input or feed index this trigger is associated with
        Real m_triggerLevel;             //!< Level in real units
        int  m_triggerLevelCoarse;
        int  m_triggerLevelFine;
        bool m_triggerPositiveEdge;      //!< Trigger on the positive edge (else negative)
        bool m_triggerBothEdges;         //!< Trigger on both edges (else only one)
        uint32_t m_triggerDelay;         //!< Delay before the trigger is kicked off in number of samples (trigger delay)
        double m_triggerDelayMult;       //!< Trigger delay as a multiplier of trace length
        int m_triggerDelayCoarse;
        int m_triggerDelayFine;
        uint32_t m_triggerRepeat;        //!< Number of trigger conditions before the final decisive trigger
        QColor m_triggerColor;           //!< Trigger line display color
        float m_triggerColorR;           //!< Trigger line display color - red shortcut
        float m_triggerColorG;           //!< Trigger line display color - green shortcut
        float m_triggerColorB;           //!< Trigger line display color - blue shortcut

        TriggerData() :
            m_projectionType(ProjectionReal),
            m_inputIndex(0),
            m_triggerLevel(0.0f),
            m_triggerLevelCoarse(0),
            m_triggerLevelFine(0),
            m_triggerPositiveEdge(true),
            m_triggerBothEdges(false),
            m_triggerDelay(0),
            m_triggerDelayMult(0.0),
            m_triggerDelayCoarse(0),
            m_triggerDelayFine(0),
			m_triggerRepeat(0),
			m_triggerColor(0,255,0)
        {
            setColor(m_triggerColor);
        }

        void setColor(QColor color)
        {
            m_triggerColor = color;
            qreal r,g,b,a;
            m_triggerColor.getRgbF(&r, &g, &b, &a);
            m_triggerColorR = r;
            m_triggerColorG = g;
            m_triggerColorB = b;
        }
    };

    static const uint32_t m_traceChunkSize;
    static const uint32_t m_maxNbTriggers = 10;
    static const uint32_t m_maxNbTraces = 10;
    static const uint32_t m_nbTraceMemories = 16;

    ScopeVisNG(GLScopeNG* glScope = 0);
    virtual ~ScopeVisNG();

    void setSampleRate(int sampleRate);
    void configure(uint32_t traceSize, uint32_t timeBase, uint32_t timeOfsProMill, uint32_t triggerPre, bool freeRun);
    void addTrace(const TraceData& traceData);
    void changeTrace(const TraceData& traceData, uint32_t traceIndex);
    void removeTrace(uint32_t traceIndex);
    void moveTrace(uint32_t traceIndex, bool upElseDown);
    void focusOnTrace(uint32_t traceIndex);
    void addTrigger(const TriggerData& triggerData);
    void changeTrigger(const TriggerData& triggerData, uint32_t triggerIndex);
    void removeTrigger(uint32_t triggerIndex);
    void moveTrigger(uint32_t triggerIndex, bool upElseDown);
    void focusOnTrigger(uint32_t triggerIndex);
    void setOneShot(bool oneShot);
    void setMemoryIndex(uint32_t memoryIndex);

    void getTriggerData(TriggerData& triggerData, uint32_t triggerIndex)
    {
        if (triggerIndex < m_triggerConditions.size())
        {
            triggerData = m_triggerConditions[triggerIndex].m_triggerData;
        }
    }

    void getTraceData(TraceData& traceData, uint32_t traceIndex)
    {
        if (traceIndex < m_traces.m_tracesData.size())
        {
            traceData = m_traces.m_tracesData[traceIndex];
        }
    }

    const TriggerData& getTriggerData(uint32_t triggerIndex) const { return m_triggerConditions[triggerIndex].m_triggerData; }
    const std::vector<TraceData>& getTracesData() const { return m_traces.m_tracesData; }
    uint32_t getNbTriggers() const { return m_triggerConditions.size(); }

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
            uint32_t timeBase,
            uint32_t timeOfsProMill,
			uint32_t triggerPre,
            bool freeRun)
        {
            return new MsgConfigureScopeVisNG(traceSize, timeBase, timeOfsProMill, triggerPre, freeRun);
        }

        uint32_t getTraceSize() const { return m_traceSize; }
        uint32_t getTimeBase() const { return m_timeBase; }
        uint32_t getTimeOfsProMill() const { return m_timeOfsProMill; }
        uint32_t getTriggerPre() const { return m_triggerPre; }
        bool getFreeRun() const { return m_freeRun; }

    private:
        uint32_t m_traceSize;
        uint32_t m_timeBase;
        uint32_t m_timeOfsProMill;
        uint32_t m_triggerPre;
        bool m_freeRun;

        MsgConfigureScopeVisNG(uint32_t traceSize,
                uint32_t timeBase,
                uint32_t timeOfsProMill,
				uint32_t triggerPre,
                bool freeRun) :
            m_traceSize(traceSize),
            m_timeBase(timeBase),
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
    class MsgScopeVisNGMoveTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGMoveTrigger* create(
                uint32_t triggerIndex,
                bool moveUpElseDown)
        {
            return new MsgScopeVisNGMoveTrigger(triggerIndex, moveUpElseDown);
        }

        uint32_t getTriggerIndex() const { return m_triggerIndex; }
        bool getMoveUp() const { return m_moveUpElseDown; }

    private:
        uint32_t m_triggerIndex;
        bool m_moveUpElseDown;

        MsgScopeVisNGMoveTrigger(uint32_t triggerIndex, bool moveUpElseDown) :
            m_triggerIndex(triggerIndex),
            m_moveUpElseDown(moveUpElseDown)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGFocusOnTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGFocusOnTrigger* create(
                uint32_t triggerIndex)
        {
            return new MsgScopeVisNGFocusOnTrigger(triggerIndex);
        }

        uint32_t getTriggerIndex() const { return m_triggerIndex; }

    private:
        uint32_t m_triggerIndex;

        MsgScopeVisNGFocusOnTrigger(uint32_t triggerIndex) :
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

    // ---------------------------------------------
    class MsgScopeVisNGMoveTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGMoveTrace* create(
                uint32_t traceIndex,
                bool moveUpElseDown)
        {
            return new MsgScopeVisNGMoveTrace(traceIndex, moveUpElseDown);
        }

        uint32_t getTraceIndex() const { return m_traceIndex; }
        bool getMoveUp() const { return m_moveUpElseDown; }

    private:
        uint32_t m_traceIndex;
        bool m_moveUpElseDown;

        MsgScopeVisNGMoveTrace(uint32_t traceIndex, bool moveUpElseDown) :
            m_traceIndex(traceIndex),
            m_moveUpElseDown(moveUpElseDown)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGFocusOnTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGFocusOnTrace* create(
                uint32_t traceIndex)
        {
            return new MsgScopeVisNGFocusOnTrace(traceIndex);
        }

        uint32_t getTraceIndex() const { return m_traceIndex; }

    private:
        uint32_t m_traceIndex;

        MsgScopeVisNGFocusOnTrace(uint32_t traceIndex) :
            m_traceIndex(traceIndex)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGOneShot : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGOneShot* create(
                bool oneShot)
        {
            return new MsgScopeVisNGOneShot(oneShot);
        }

        bool getOneShot() const { return m_oneShot; }

    private:
        bool m_oneShot;

        MsgScopeVisNGOneShot(bool oneShot) :
            m_oneShot(oneShot)
        {}
    };

    // ---------------------------------------------
    class MsgScopeVisNGMemoryTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisNGMemoryTrace* create(
                uint32_t memoryIndex)
        {
            return new MsgScopeVisNGMemoryTrace(memoryIndex);
        }

        uint32_t getMemoryIndex() const { return m_memoryIndex; }

    private:
        uint32_t m_memoryIndex;

        MsgScopeVisNGMemoryTrace(uint32_t memoryIndex) :
            m_memoryIndex(memoryIndex)
        {}
    };

    // ---------------------------------------------

    /**
     * Projection stuff
     */
    class Projector
    {
    public:
        Projector(ProjectionType projectionType) :
            m_projectionType(projectionType),
            m_prevArg(0.0f),
			m_cache(0),
			m_cacheMaster(true)
        {}

        ~Projector()
        {}

        ProjectionType getProjectionType() const { return m_projectionType; }
        void settProjectionType(ProjectionType projectionType) { m_projectionType = projectionType; }
        void setCache(Real *cache) { m_cache = cache; }
        void setCacheMaster(bool cacheMaster) { m_cacheMaster = cacheMaster; }

        Real run(const Sample& s)
        {
        	Real v;

        	if ((m_cache) && !m_cacheMaster) {
        		return m_cache[(int) m_projectionType];
        	}
        	else
        	{
                switch (m_projectionType)
                {
                case ProjectionImag:
                    v = s.m_imag / SDR_RX_SCALEF;
                    break;
                case ProjectionMagLin:
                {
                	Real re = s.m_real / SDR_RX_SCALEF;
                	Real im = s.m_imag / SDR_RX_SCALEF;
                	Real magsq = re*re + im*im;
                    v = std::sqrt(magsq);
                }
                    break;
                case ProjectionMagDB:
                {
                	Real re = s.m_real / SDR_RX_SCALEF;
                	Real im = s.m_imag / SDR_RX_SCALEF;
                	Real magsq = re*re + im*im;
                    v = log10f(magsq) * 10.0f;
                }
                    break;
                case ProjectionPhase:
                    v = std::atan2((float) s.m_imag, (float) s.m_real) / M_PI;
                    break;
                case ProjectionDPhase:
                {
                    Real curArg = std::atan2((float) s.m_imag, (float) s.m_real);
                    Real dPhi = (curArg - m_prevArg) / M_PI;
                    m_prevArg = curArg;

                    if (dPhi < -1.0f) {
                        dPhi += 2.0f;
                    } else if (dPhi > 1.0f) {
                        dPhi -= 2.0f;
                    }

                    v = dPhi;
                }
                    break;
                case ProjectionReal:
                default:
                    v = s.m_real / SDR_RX_SCALEF;
                    break;
                }

                if (m_cache) {
                	m_cache[(int) m_projectionType] = v;
                }

                return v;
        	}
        }

    private:
        ProjectionType m_projectionType;
        Real m_prevArg;
        Real *m_cache;
        bool m_cacheMaster;
    };

    /**
     * Trigger stuff
     */
    enum TriggerState
    {
        TriggerUntriggered, //!< Trigger is not kicked off yet (or trigger list is empty)
        TriggerTriggered,   //!< Trigger has been kicked off
        TriggerDelay,       //!< Trigger conditions have been kicked off but it is waiting for delay before final kick off
        TriggerNewConfig,   //!< Special condition when a new configuration has been received
    };

    struct TriggerCondition
    {
    public:
        Projector m_projector;
        TriggerData m_triggerData;    //!< Trigger data
        bool m_prevCondition;         //!< Condition (above threshold) at previous sample
        uint32_t m_triggerDelayCount; //!< Counter of samples for delay
        uint32_t m_triggerCounter;    //!< Counter of trigger occurences

        TriggerCondition(const TriggerData& triggerData) :
            m_projector(ProjectionReal),
            m_triggerData(triggerData),
            m_prevCondition(false),
            m_triggerDelayCount(0),
            m_triggerCounter(0)
        {
        }

        ~TriggerCondition()
        {
        }

        void initProjector()
        {
            m_projector.settProjectionType(m_triggerData.m_projectionType);
        }

        void releaseProjector()
        {
        }

        void setData(const TriggerData& triggerData)
        {
            m_triggerData = triggerData;

            if (m_projector.getProjectionType() != m_triggerData.m_projectionType)
            {
                m_projector.settProjectionType(m_triggerData.m_projectionType);
            }

            m_prevCondition = false;
            m_triggerDelayCount = 0;
            m_triggerCounter = 0;
        }

        void operator=(const TriggerCondition& other)
        {
            setData(other.m_triggerData);
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
         * Return trace at given memory position
         */
        TraceBackBuffer& at(int index)
        {
            return m_traceBackBuffers[index];
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
        Projector m_projector;    //!< Projector transform from complex trace to real (displayable) trace
        uint32_t m_traceCount[2]; //!< Count of samples processed (double buffered)
        Real m_maxPow;            //!< Maximum power over the current trace for MagDB overlay display
        Real m_sumPow;            //!< Cumulative power over the current trace for MagDB overlay display
        int m_nbPow;              //!< Number of power samples over the current trace for MagDB overlay display

        TraceControl() : m_projector(ProjectionReal)
        {
            reset();
        }

        ~TraceControl()
        {
        }

        void initProjector(ProjectionType projectionType)
        {
            m_projector.settProjectionType(projectionType);
        }

        void releaseProjector()
        {
        }

        void reset()
        {
            m_traceCount[0] = 0;
            m_traceCount[1] = 0;
            m_maxPow = 0.0f;
            m_sumPow = 0.0f;
            m_nbPow = 0;
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

        Traces() :
            m_traceSize(0),
            m_maxTraceSize(0),
            evenOddIndex(true),
            m_x0(0),
            m_x1(0)
        {
        }

        ~Traces()
        {
            if (m_x0) delete[] m_x0;
            if (m_x1) delete[] m_x1;
            m_maxTraceSize = 0;
        }

        bool isVerticalDisplayChange(const TraceData& traceData, uint32_t traceIndex)
        {
        	return (m_tracesData[traceIndex].m_projectionType != traceData.m_projectionType)
        			|| (m_tracesData[traceIndex].m_amp != traceData.m_amp)
					|| (m_tracesData[traceIndex].m_ofs != traceData.m_ofs
					|| (m_tracesData[traceIndex].m_traceColor != traceData.m_traceColor));
        }

        void addTrace(const TraceData& traceData, int traceSize)
        {
            if (m_traces[0].size() < m_maxNbTraces)
            {
                m_traces[0].push_back(0);
                m_traces[1].push_back(0);
                m_tracesData.push_back(traceData);
                m_tracesControl.push_back(TraceControl());
                m_tracesControl.back().initProjector(traceData.m_projectionType);

                resize(traceSize);
            }
        }

        void changeTrace(const TraceData& traceData, uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size()) {
                m_tracesControl[traceIndex].releaseProjector();
                m_tracesControl[traceIndex].initProjector(traceData.m_projectionType);
                m_tracesData[traceIndex] = traceData;
            }
        }

        void removeTrace(uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size())
            {
                m_traces[0].erase(m_traces[0].begin() + traceIndex);
                m_traces[1].erase(m_traces[1].begin() + traceIndex);
            	m_tracesControl[traceIndex].releaseProjector();
                m_tracesControl.erase(m_tracesControl.begin() + traceIndex);
                m_tracesData.erase(m_tracesData.begin() + traceIndex);

                resize(m_traceSize); // reallocate pointers
            }
        }

        void moveTrace(uint32_t traceIndex, bool upElseDown)
        {
            if ((!upElseDown) && (traceIndex == 0)) {
                return;
            }

            int nextControlIndex = (traceIndex + (upElseDown ? 1 : -1)) % (m_tracesControl.size());
            int nextDataIndex = (traceIndex + (upElseDown ? 1 : -1)) % (m_tracesData.size()); // should be the same

            m_tracesControl[traceIndex].releaseProjector();
            m_tracesControl[nextControlIndex].releaseProjector();

            TraceControl nextControl = m_tracesControl[nextControlIndex];
            m_tracesControl[nextControlIndex] = m_tracesControl[traceIndex];
            m_tracesControl[traceIndex] = nextControl;

            TraceData nextData = m_tracesData[nextDataIndex];
            m_tracesData[nextDataIndex] = m_tracesData[traceIndex];
            m_tracesData[traceIndex] = nextData;

            m_tracesControl[traceIndex].initProjector(m_tracesData[traceIndex].m_projectionType);
            m_tracesControl[nextControlIndex].initProjector(m_tracesData[nextDataIndex].m_projectionType);
        }

        void resize(int traceSize)
        {
            m_traceSize = traceSize;

            if (m_traceSize > m_maxTraceSize)
            {
                delete[] m_x0;
                delete[] m_x1;
                m_x0 = new float[2*m_traceSize*m_maxNbTraces];
                m_x1 = new float[2*m_traceSize*m_maxNbTraces];

                m_maxTraceSize = m_traceSize;
            }

            std::fill_n(m_x0, 2*m_traceSize*m_traces[0].size(), 0.0f);
            std::fill_n(m_x1, 2*m_traceSize*m_traces[0].size(), 0.0f);

            for (unsigned int i = 0; i < m_traces[0].size(); i++)
            {
                (m_traces[0])[i] = &m_x0[2*m_traceSize*i];
                (m_traces[1])[i] = &m_x1[2*m_traceSize*i];
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

    private:
        float *m_x0;
        float *m_x1;
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

            if (triggerCondition.m_projector.getProjectionType() == ProjectionMagDB) {
                condition = triggerCondition.m_projector.run(s) > m_levelPowerDB;
            } else if (triggerCondition.m_projector.getProjectionType() == ProjectionMagLin) {
                condition = triggerCondition.m_projector.run(s) > m_levelPowerLin;
            } else {
                condition = triggerCondition.m_projector.run(s) > m_level;
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
            m_levelPowerDB = (100.0f * (m_level - 1.0f));
        }

        Real m_level;
        Real m_levelPowerDB;
        Real m_levelPowerLin;
        bool m_reset;
    };

    GLScopeNG* m_glScope;
    uint32_t m_preTriggerDelay;                    //!< Pre-trigger delay in number of samples
    std::vector<TriggerCondition> m_triggerConditions; //!< Chain of triggers
    uint32_t m_currentTriggerIndex;                //!< Index of current index in the chain
    uint32_t m_focusedTriggerIndex;                //!< Index of the trigger that has focus
    TriggerState m_triggerState;                   //!< Current trigger state
    Traces m_traces;                               //!< Displayable traces
    int m_focusedTraceIndex;                       //!< Index of the trace that has focus
    uint32_t m_traceSize;                          //!< Size of traces in number of samples
    int m_nbSamples;                               //!< Number of samples yet to process in one complex trace
    uint32_t m_timeBase;                           //!< Trace display time divisor
    uint32_t m_timeOfsProMill;                     //!< Start trace shift in 1/1000 trace size
    bool m_traceStart;                             //!< Trace is at start point
    int m_traceFill;                               //!< Count of samples accumulated into trace
    int m_zTraceIndex;                             //!< Index of the trace used for Z input (luminance or false colors)
    SampleVector::const_iterator m_triggerPoint;   //!< Trigger start location in the samples vector
    int m_sampleRate;
    TraceBackDiscreteMemory m_traceDiscreteMemory; //!< Complex trace memory for triggered states TODO: vectorize when more than on input is allowed
    bool m_freeRun;                                //!< True if free running (trigger globally disabled)
    int m_maxTraceDelay;                           //!< Maximum trace delay
    TriggerComparator m_triggerComparator;         //!< Compares sample level to trigger level
    QMutex m_mutex;
    Real m_projectorCache[(int) nbProjectionTypes];
    bool m_triggerOneShot;                         //!< True when one shot mode is active
    bool m_triggerWaitForReset;                    //!< In one shot mode suspended until reset by UI
    uint32_t m_currentTraceMemoryIndex;            //!< The current index of trace in memory (0: current)

    /**
     * Moves on to the next trigger if any or increments trigger count if in repeat mode
     * - If not final it returns true
     * - If final i.e. signal is actually triggerd it returns false
     */
    bool nextTrigger(); //!< Returns true if not final

    /**
     * Process a sample trace which length is at most the trace length (m_traceSize)
     */
    void processTrace(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, int& triggerPointToEnd);

    /**
     * process a trace in memory at current trace index in memory
     */
    void processMemoryTrace();

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

    /**
     * Calculate trigger levels on display
     * - every time a trigger condition focus changes TBD
     * - every time the focused trigger condition changes its projection type or level
     * - every time a trace data changes: projection type, amp, offset
     * - every time a trace data is added or removed
     */
    void computeDisplayTriggerLevels();

    /**
     * Update glScope display
     * - Live trace: call glScipe update method
     * - Trace in memory: call process memory trace
     */
    void updateGLScopeDisplay();
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
