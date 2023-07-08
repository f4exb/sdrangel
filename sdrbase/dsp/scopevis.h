///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QByteArray>

#include <algorithm>
#include <utility>
#include <cmath>

#include <stdint.h>
#include <vector>
#include "dsp/dsptypes.h"
#include "dsp/projector.h"
#include "dsp/glscopesettings.h"
#include "export.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/doublebuffer.h"
#include "util/doublebuffermultiple.h"


class GLScopeInterface;
class SpectrumVis;

class SDRBASE_API ScopeVis : public QObject {
    Q_OBJECT
public:
    // === messages ===
    // ---------------------------------------------
    class SDRBASE_API MsgConfigureScopeVis : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const GLScopeSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureScopeVis* create(const GLScopeSettings& settings, bool force)
        {
            return new MsgConfigureScopeVis(settings, force);
        }

    private:
        GLScopeSettings m_settings;
        bool m_force;

        MsgConfigureScopeVis(const GLScopeSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisAddTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisAddTrigger* create(
                const GLScopeSettings::TriggerData& triggerData)
        {
            return new MsgScopeVisAddTrigger(triggerData);
        }

        const GLScopeSettings::TriggerData& getTriggerData() const { return m_triggerData; }

    private:
        GLScopeSettings::TriggerData m_triggerData;

        MsgScopeVisAddTrigger(const GLScopeSettings::TriggerData& triggerData) :
            m_triggerData(triggerData)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisChangeTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisChangeTrigger* create(
            const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex)
        {
            return new MsgScopeVisChangeTrigger(triggerData, triggerIndex);
        }

        const GLScopeSettings::TriggerData& getTriggerData() const { return m_triggerData; }
        uint32_t getTriggerIndex() const { return m_triggerIndex; }

    private:
        GLScopeSettings::TriggerData m_triggerData;
        uint32_t m_triggerIndex;

        MsgScopeVisChangeTrigger(const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex) :
            m_triggerData(triggerData),
            m_triggerIndex(triggerIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisRemoveTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisRemoveTrigger* create(
                uint32_t triggerIndex)
        {
            return new MsgScopeVisRemoveTrigger(triggerIndex);
        }

        uint32_t getTriggerIndex() const { return m_triggerIndex; }

    private:
        uint32_t m_triggerIndex;

        MsgScopeVisRemoveTrigger(uint32_t triggerIndex) :
            m_triggerIndex(triggerIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisMoveTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisMoveTrigger* create(
                uint32_t triggerIndex,
                bool moveUpElseDown)
        {
            return new MsgScopeVisMoveTrigger(triggerIndex, moveUpElseDown);
        }

        uint32_t getTriggerIndex() const { return m_triggerIndex; }
        bool getMoveUp() const { return m_moveUpElseDown; }

    private:
        uint32_t m_triggerIndex;
        bool m_moveUpElseDown;

        MsgScopeVisMoveTrigger(uint32_t triggerIndex, bool moveUpElseDown) :
            m_triggerIndex(triggerIndex),
            m_moveUpElseDown(moveUpElseDown)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisFocusOnTrigger : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisFocusOnTrigger* create(
                uint32_t triggerIndex)
        {
            return new MsgScopeVisFocusOnTrigger(triggerIndex);
        }

        uint32_t getTriggerIndex() const { return m_triggerIndex; }

    private:
        uint32_t m_triggerIndex;

        MsgScopeVisFocusOnTrigger(uint32_t triggerIndex) :
            m_triggerIndex(triggerIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisAddTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisAddTrace* create(
                const GLScopeSettings::TraceData& traceData)
        {
            return new MsgScopeVisAddTrace(traceData);
        }

        const GLScopeSettings::TraceData& getTraceData() const { return m_traceData; }

    private:
        GLScopeSettings::TraceData m_traceData;

        MsgScopeVisAddTrace(const GLScopeSettings::TraceData& traceData) :
            m_traceData(traceData)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisChangeTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisChangeTrace* create(
                const GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
        {
            return new MsgScopeVisChangeTrace(traceData, traceIndex);
        }

        const GLScopeSettings::TraceData& getTraceData() const { return m_traceData; }
        uint32_t getTraceIndex() const { return m_traceIndex; }

    private:
        GLScopeSettings::TraceData m_traceData;
        uint32_t m_traceIndex;

        MsgScopeVisChangeTrace(GLScopeSettings::TraceData traceData, uint32_t traceIndex) :
            m_traceData(traceData),
            m_traceIndex(traceIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisRemoveTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisRemoveTrace* create(
                uint32_t traceIndex)
        {
            return new MsgScopeVisRemoveTrace(traceIndex);
        }

        uint32_t getTraceIndex() const { return m_traceIndex; }

    private:
        uint32_t m_traceIndex;

        MsgScopeVisRemoveTrace(uint32_t traceIndex) :
            m_traceIndex(traceIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisMoveTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisMoveTrace* create(
                uint32_t traceIndex,
                bool moveUpElseDown)
        {
            return new MsgScopeVisMoveTrace(traceIndex, moveUpElseDown);
        }

        uint32_t getTraceIndex() const { return m_traceIndex; }
        bool getMoveUp() const { return m_moveUpElseDown; }

    private:
        uint32_t m_traceIndex;
        bool m_moveUpElseDown;

        MsgScopeVisMoveTrace(uint32_t traceIndex, bool moveUpElseDown) :
            m_traceIndex(traceIndex),
            m_moveUpElseDown(moveUpElseDown)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisFocusOnTrace : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgScopeVisFocusOnTrace* create(
                uint32_t traceIndex)
        {
            return new MsgScopeVisFocusOnTrace(traceIndex);
        }

        uint32_t getTraceIndex() const { return m_traceIndex; }

    private:
        uint32_t m_traceIndex;

        MsgScopeVisFocusOnTrace(uint32_t traceIndex) :
            m_traceIndex(traceIndex)
        {}
    };

    // ---------------------------------------------
    class SDRBASE_API MsgScopeVisNGOneShot : public Message {
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
    class SDRBASE_API MsgScopeVisNGMemoryTrace : public Message {
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

    ScopeVis();
    virtual ~ScopeVis();

    void setGLScope(GLScopeInterface* glScope);
    void setSpectrumVis(SpectrumVis *spectrumVis) { m_spectrumVis = spectrumVis; }
    void setSSBSpectrum(bool ssbSpectrum) { m_ssbSpectrum = ssbSpectrum; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

    void setLiveRate(int sampleRate);
    void setNbStreams(uint32_t nbStreams);
    void configure(
        uint32_t traceSize,
        uint32_t timeBase,
        uint32_t timeOfsProMill,
        uint32_t triggerPre,
        bool freeRun
    );
    void configure(
        GLScopeSettings::DisplayMode displayMode,
        uint32_t traceIntensity,
        uint32_t gridIntensity
    );
    void setOneShot(bool oneShot);
    void setMemoryIndex(uint32_t memoryIndex);
    void setTraceChunkSize(uint32_t chunkSize) { m_traceChunkSize = chunkSize; }
    uint32_t getTraceChunkSize() const { return m_traceChunkSize; }

    QByteArray serializeMemory() const
    {
        SimpleSerializer s(1);

        s.writeU32(1, m_traceSize);
        s.writeU32(2, m_preTriggerDelay);
        s.writeS32(3, m_sampleRate);
        QByteArray buffer = m_traceDiscreteMemory.serialize();
        s.writeBlob(4, buffer);

        return s.final();
    }

    bool deserializeMemory(const QByteArray& data)
    {
        SimpleDeserializer d(data);

        if(!d.isValid()) {
            return false;
        }

        if (d.getVersion() == 1)
        {
            uint32_t traceSize, preTriggerDelay;
            int sampleRate;
            QByteArray buf;
            bool traceDiscreteMemorySuccess;

            d.readU32(1, &traceSize, GLScopeSettings::m_traceChunkDefaultSize);
            d.readU32(2, &preTriggerDelay, 0);
            d.readS32(3, &sampleRate, 0);
            setSampleRate(sampleRate);
            setTraceSize(traceSize, true);
            setPreTriggerDelay(preTriggerDelay, true);
            d.readBlob(4, &buf);
            traceDiscreteMemorySuccess = m_traceDiscreteMemory.deserialize(buf);

            if (traceDiscreteMemorySuccess && (m_glScope) && (m_currentTraceMemoryIndex > 0)) {
                processMemoryTrace();
            }

            return traceDiscreteMemorySuccess;
        }
        else
        {
            return false;
        }
    }

    void getTriggerData(GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex)
    {
        if (triggerIndex < m_triggerConditions.size()) {
            triggerData = m_triggerConditions[triggerIndex]->m_triggerData;
        }
    }

    void getTraceData(GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
    {
        if (traceIndex < m_traces.m_tracesData.size()) {
            traceData = m_traces.m_tracesData[traceIndex];
        }
    }

    const GLScopeSettings::TriggerData& getTriggerData(uint32_t triggerIndex) const { return m_triggerConditions[triggerIndex]->m_triggerData; }
    const std::vector<GLScopeSettings::TraceData>& getTracesData() const { return m_traces.m_tracesData; }
    uint32_t getNbTriggers() const { return m_triggerConditions.size(); }
    uint32_t getNbTraces() const { return m_traces.size(); }

    void feed(const std::vector<SampleVector::const_iterator>& vbegin, int nbSamples);
    void feed(const std::vector<ComplexVector::const_iterator>& vbegin, int nbSamples);
    //virtual void start();
    //virtual void stop();
    bool handleMessage(const Message& message);
    int getTriggerLocation() const { return m_triggerLocation; }
    bool getFreeRun() const { return m_freeRun; }

private:
    /**
     * Trigger stuff
     */
    enum TriggerState
    {
        TriggerUntriggered, //!< Trigger is not kicked off yet (or trigger list is empty)
        TriggerTriggered,   //!< Trigger has been kicked off
        TriggerDelay,       //!< Trigger conditions have been kicked off but it is waiting for delay before final kick off
    };

    struct TriggerCondition
    {
    public:
        Projector m_projector;
        GLScopeSettings::TriggerData m_triggerData; //!< Trigger data
        bool m_prevCondition;         //!< Condition (above threshold) at previous sample
        uint32_t m_triggerDelayCount; //!< Counter of samples for delay
        uint32_t m_triggerCounter;    //!< Counter of trigger occurrences
        uint32_t m_trues;             //!< Count of true conditions for holdoff processing
        uint32_t m_falses;            //!< Count of false conditions for holdoff processing


        TriggerCondition(const GLScopeSettings::TriggerData& triggerData) :
            m_projector(Projector::ProjectionReal),
            m_triggerData(triggerData),
            m_prevCondition(false),
            m_triggerDelayCount(0),
            m_triggerCounter(0),
            m_trues(0),
            m_falses(0)
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

        void setData(const GLScopeSettings::TriggerData& triggerData)
        {
            m_triggerData = triggerData;

            if (m_projector.getProjectionType() != m_triggerData.m_projectionType)
            {
                m_projector.settProjectionType(m_triggerData.m_projectionType);
            }

            m_prevCondition = false;
            m_triggerDelayCount = 0;
            m_triggerCounter = 0;
            m_trues = 0;
            m_falses = 0;
        }

        void operator=(const TriggerCondition& other)
        {
            setData(other.m_triggerData);
        }
    };

    /**
     * Complex trace stuff
     */
    typedef DoubleBufferSimple<Complex> TraceBuffer;

    struct TraceBackBuffer
    {
    	TraceBuffer m_traceBuffer;

    	TraceBackBuffer() {
    		m_endPoint = m_traceBuffer.getCurrent();
    	}

    	void resize(uint32_t size) {
    		m_traceBuffer.resize(size);
    	}

    	void reset() {
    	    m_traceBuffer.reset();
    	}

    	void write(const ComplexVector::const_iterator begin, int nbSamples) {
    		m_traceBuffer.write(begin, nbSamples);
    	}

    	unsigned int absoluteFill() const {
    		return m_traceBuffer.absoluteFill();
    	}

        void current(ComplexVector::iterator& it) {
            m_traceBuffer.getCurrent(it);
        }

        QByteArray serialize() const
        {
            SimpleSerializer s(1);

            QByteArray buffer = m_traceBuffer.serialize();
            unsigned int endDelta = m_endPoint - m_traceBuffer.begin();
            s.writeU32(1, endDelta);
            s.writeBlob(2, buffer);

            return s.final();
        }

        bool deserialize(const QByteArray& data)
        {
            SimpleDeserializer d(data);

            if(!d.isValid()) {
                return false;
            }

            if (d.getVersion() == 1)
            {
                unsigned int tmpUInt;
                QByteArray buf;

                d.readU32(1, &tmpUInt, 0);
                d.readBlob(2, &buf);
                m_traceBuffer.deserialize(buf);
                m_endPoint = m_traceBuffer.begin() + tmpUInt;

                return true;
            }
            else
            {
                return false;
            }
        }

        void setEndPoint(const ComplexVector::const_iterator& endPoint) {
            m_endPoint = endPoint;
        }

        ComplexVector::const_iterator getEndPoint() {
            return m_endPoint;
        }

        void getEndPoint(ComplexVector::const_iterator& it) {
            it = m_endPoint;
        }

    private:
    	ComplexVector::const_iterator m_endPoint;
    };

    typedef std::vector<TraceBackBuffer> TraceBackBufferStream;

    struct ConvertBuffers
    {
        ConvertBuffers(uint32_t nbStreams = 1) :
            m_convertBuffers(nbStreams)
        {}

        void setNbStreams(uint32_t nbStreams)
        {
            m_convertBuffers.resize(nbStreams);
            resize(m_size);
        }

        void resize(unsigned int size)
        {
            for (unsigned int s = 0; s < m_convertBuffers.size(); s++) {
                m_convertBuffers[s].resize(size);
            }

            m_size = size;
        }

        unsigned int size() const {
            return m_size;
        }

        std::vector<ComplexVector>& getBuffers() {
            return m_convertBuffers;
        }

    private:
        unsigned int m_size;
        std::vector<ComplexVector> m_convertBuffers;
    };
    struct TraceBackDiscreteMemory
    {
    	/**
    	 * Give memory size in number of traces
    	 */
        TraceBackDiscreteMemory(uint32_t size, uint32_t nbStreams = 1) :
            m_traceBackBuffersStreams(nbStreams),
            m_memSize(size),
            m_currentMemIndex(0),
            m_maxMemIndex(0),
            m_traceSize(0)
    	{
            for (unsigned int s = 0; s < m_traceBackBuffersStreams.size(); s++) {
        		m_traceBackBuffersStreams[s].resize(m_memSize);
            }
    	}

        void setNbStreams(uint32_t nbStreams)
        {
            m_traceBackBuffersStreams.resize(nbStreams);

            for (unsigned int s = 0; s < m_traceBackBuffersStreams.size(); s++) {
                m_traceBackBuffersStreams[s].resize(m_memSize);
            }

            resize(m_traceSize);
        }

    	/**
    	 * Resize all trace buffers in memory
    	 */
    	void resize(uint32_t size)
    	{
    	    m_traceSize = size;

            for (unsigned int s = 0; s < m_traceBackBuffersStreams.size(); s++)
            {
                for (std::vector<TraceBackBuffer>::iterator it = m_traceBackBuffersStreams[s].begin(); it != m_traceBackBuffersStreams[s].end(); ++it) {
                    it->resize(2*m_traceSize); // was multiplied by 4
                }
            }
    	}

    	/**
    	 * Move index forward by one position and return reference to the trace at this position
    	 * Copy a trace length of samples into the new memory slot
         * samplesToReport are the number of samples to report on the next trace
    	 */
        void store(int samplesToReport)
    	{
    	    uint32_t nextMemIndex = m_currentMemIndex < (m_memSize-1) ? m_currentMemIndex+1 : 0;

            for (unsigned int s = 0; s < m_traceBackBuffersStreams.size(); s++)
            {
                m_traceBackBuffersStreams[s][nextMemIndex].reset();
                m_traceBackBuffersStreams[s][nextMemIndex].write(
                    m_traceBackBuffersStreams[s][m_currentMemIndex].getEndPoint() - samplesToReport,
                    samplesToReport
                );
            }

    	    m_currentMemIndex = nextMemIndex;

            if (m_currentMemIndex > m_maxMemIndex) {
                m_maxMemIndex = m_currentMemIndex;
            }
    	}

    	/**
    	 * Return current memory index
    	 */
    	uint32_t currentIndex() const { return m_currentMemIndex; }

    	/**
    	 * Return max memory index processed
    	 */
    	uint32_t maxIndex() const { return m_maxMemIndex; }

    	/**
    	 * Serializer
    	 */
        QByteArray serialize() const
        {
            SimpleSerializer s(1);

            s.writeU32(1, m_traceBackBuffersStreams.size());
            s.writeU32(2, m_memSize);
            s.writeU32(3, m_currentMemIndex);
            s.writeU32(4, m_traceSize);

            for (unsigned int is = 0; is < m_traceBackBuffersStreams.size(); is++)
            {
                SimpleSerializer ss(1);

                for (unsigned int i = 0; i < m_memSize; i++)
                {
                    QByteArray buffer = m_traceBackBuffersStreams[is][i].serialize();
                    ss.writeBlob(i, buffer);
                }

                s.writeBlob(5+is, ss.final());
            }

            return s.final();
        }

        /**
         * Deserializer
         */
        bool deserialize(const QByteArray& data)
        {
            SimpleDeserializer d(data);

            if(!d.isValid()) {
                return false;
            }

            if (d.getVersion() == 1)
            {
                unsigned int nbStreams;
                d.readU32(1, &nbStreams, 0);
                d.readU32(2, &m_memSize, 0);
                d.readU32(3, &m_currentMemIndex, 0);
                uint32_t traceSize;
                d.readU32(4, &traceSize, 0);

                for (unsigned int is = 0; is < nbStreams; is++)
                {
                    if (is >= m_traceBackBuffersStreams.size()) {
                        break;
                    }

                    m_traceBackBuffersStreams[is].resize(m_memSize);

                    if (traceSize != m_traceSize) {
                        resize(traceSize);
                    }

                    QByteArray streamData;
                    d.readBlob(5+is, &streamData);
                    SimpleDeserializer ds(streamData);

                    for (unsigned int i = 0; i < m_memSize; i++)
                    {
                        QByteArray buffer;
                        ds.readBlob(i, &buffer);
                        m_traceBackBuffersStreams[is][i].deserialize(buffer);
                    }
                }

                return true;
            }
            else
            {
                return false;
            }
        }

        /**
         * Get current point at current memory position (first stream)
         */
        void getCurrent(ComplexVector::iterator& it) {
            current().current(it);
        }

        /**
         * Get current points at current memory position
         */
        void getCurrent(std::vector<ComplexVector::const_iterator>& vit)
        {
            vit.clear();

            for (unsigned int is = 0; is < m_traceBackBuffersStreams.size(); is++)
            {
                ComplexVector::iterator it;
                current(is).current(it);
                vit.push_back(it);
            }
        }

        /**
         * Set end point at current memory position (first stream)
         */
        void setCurrentEndPoint(const ComplexVector::iterator& it) {
            current().setEndPoint(it);
        }

        /**
         * Set end points at current memory position
         */
        void setCurrentEndPoint(const std::vector<ComplexVector::const_iterator>& vit)
        {
            for (unsigned int is = 0; is < vit.size(); is++)
            {
                if (is >= m_traceBackBuffersStreams.size()) {
                    break;
                }

                current(is).setEndPoint(vit[is]);
            }
        }

        /**
         * Get end point at given memory position (first stream)
         */
        void getEndPointAt(int index, ComplexVector::const_iterator& mend) {
            at(index).getEndPoint(mend);
        }

        /**
         * Get end points at given memory position
         */
        void getEndPointAt(int index, std::vector<ComplexVector::const_iterator>& vend)
        {
            vend.clear();

            for (unsigned int is = 0; is < m_traceBackBuffersStreams.size(); is++)
            {
                ComplexVector::const_iterator mend;
                at(index, is).getEndPoint(mend);
                vend.push_back(mend);
            }
        }

        /**
         * Write trace at current memory position (first stream)
         */
        void writeCurrent(const ComplexVector::const_iterator& begin, int length) {
            current().write(begin, length);
        }

        /**
         * Write traces at current memory position
         */
        void writeCurrent(const std::vector<ComplexVector::const_iterator>& vbegin, int length)
        {
            for (unsigned int i = 0; i < vbegin.size(); i++) {
                current(i).write(vbegin[i], length);
            }
        }

        /**
         * Move buffer iterator by a certain amount (first stream)
         */
        static void moveIt(const ComplexVector::const_iterator& x, ComplexVector::const_iterator& y, int amount) {
            y = x + amount;
        }

        /**
         * Move buffers iterators by a certain amount
         */
        static void moveIt(const std::vector<ComplexVector::const_iterator>& vx, std::vector<ComplexVector::const_iterator>& vy, int amount)
        {
            for (unsigned int i = 0; i < vx.size(); i++)
            {
                if (i >= vy.size()) {
                    break;
                }

                vy[i] = vx[i] + amount;
            }
        }

    private:
        std::vector<TraceBackBufferStream> m_traceBackBuffersStreams;
        uint32_t m_memSize;
        uint32_t m_currentMemIndex;
        uint32_t m_maxMemIndex;
        uint32_t m_traceSize;

        TraceBackBuffer& current(uint32_t streamIndex = 0) { //!< Return trace at current memory position
            return m_traceBackBuffersStreams[streamIndex][m_currentMemIndex];
        }

        TraceBackBuffer& at(int index, uint32_t streamIndex = 0) { //!< Return trace at given memory position
            return m_traceBackBuffersStreams[streamIndex][index];
        }
    };

    /**
     * Displayable trace stuff
     */
    struct TraceControl
    {
        Projector m_projector;    //!< Projector transform from complex trace to real (displayable) trace
        uint32_t m_traceCount[2]; //!< Count of samples processed (double buffered)
        double m_maxPow;          //!< Maximum power over the current trace for MagDB overlay display
        double m_sumPow;          //!< Cumulative power over the current trace for MagDB overlay display
        int m_nbPow;              //!< Number of power samples over the current trace for MagDB overlay display

        TraceControl() : m_projector(Projector::ProjectionReal)
        {
            reset();
        }

        ~TraceControl()
        {
        }

        void initProjector(Projector::ProjectionType projectionType)
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
        std::vector<TraceControl*> m_tracesControl;   //!< Corresponding traces control data
        std::vector<GLScopeSettings::TraceData> m_tracesData; //!< Corresponding traces data
        std::vector<float *> m_traces[2];             //!< Double buffer of traces processed by glScope
        std::vector<Projector::ProjectionType> m_projectionTypes;
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
            for (std::vector<TraceControl*>::iterator it = m_tracesControl.begin(); it != m_tracesControl.end(); ++it) {
                delete *it;
            }

            if (m_x0) {
                delete[] m_x0;
            }

            if (m_x1) {
                delete[] m_x1;
            }

            m_maxTraceSize = 0;
        }

        bool isVerticalDisplayChange(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
        {
        	return (m_tracesData[traceIndex].m_projectionType != traceData.m_projectionType)
        			|| (m_tracesData[traceIndex].m_amp != traceData.m_amp)
					|| (m_tracesData[traceIndex].m_ofs != traceData.m_ofs
					|| (m_tracesData[traceIndex].m_traceColor != traceData.m_traceColor));
        }

        void addTrace(const GLScopeSettings::TraceData& traceData, int traceSize)
        {
            if (m_traces[0].size() < GLScopeSettings::m_maxNbTraces)
            {
                qDebug("ScopeVis::Traces::addTrace");
                m_traces[0].push_back(nullptr);
                m_traces[1].push_back(nullptr);
                m_tracesData.push_back(traceData);
                m_projectionTypes.push_back(traceData.m_projectionType);
                m_tracesControl.push_back(new TraceControl());
                TraceControl *traceControl = m_tracesControl.back();
                traceControl->initProjector(traceData.m_projectionType);

                resize(traceSize);
            }
        }

        void changeTrace(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size())
            {
                TraceControl *traceControl = m_tracesControl[traceIndex];
                traceControl->releaseProjector();
                traceControl->initProjector(traceData.m_projectionType);
                m_tracesData[traceIndex] = traceData;
                m_projectionTypes[traceIndex] = traceData.m_projectionType;
            }
        }

        void removeTrace(uint32_t traceIndex)
        {
            if (traceIndex < m_tracesControl.size())
            {
                qDebug("ScopeVis::Traces::removeTrace");
                m_traces[0].erase(m_traces[0].begin() + traceIndex);
                m_traces[1].erase(m_traces[1].begin() + traceIndex);
                m_projectionTypes.erase(m_projectionTypes.begin() + traceIndex);
                TraceControl *traceControl = m_tracesControl[traceIndex];
                traceControl->releaseProjector();
                m_tracesControl.erase(m_tracesControl.begin() + traceIndex);
                m_tracesData.erase(m_tracesData.begin() + traceIndex);
                delete traceControl;

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
            int nextProjectionTypeIndex = (traceIndex + (upElseDown ? 1 : -1)) % (m_projectionTypes.size()); // should be the same

            Projector::ProjectionType nextType = m_projectionTypes[traceIndex];
            m_projectionTypes[nextProjectionTypeIndex] = m_projectionTypes[traceIndex];
            m_projectionTypes[traceIndex] = nextType;

            TraceControl *traceControl = m_tracesControl[traceIndex];
            TraceControl *nextTraceControl = m_tracesControl[nextControlIndex];

            traceControl->releaseProjector();
            nextTraceControl->releaseProjector();

            m_tracesControl[nextControlIndex] = traceControl;
            m_tracesControl[traceIndex] = nextTraceControl;

            GLScopeSettings::TraceData nextData = m_tracesData[nextDataIndex];
            m_tracesData[nextDataIndex] = m_tracesData[traceIndex];
            m_tracesData[traceIndex] = nextData;

            traceControl = m_tracesControl[traceIndex];
            nextTraceControl = m_tracesControl[nextControlIndex];

            traceControl->initProjector(m_tracesData[traceIndex].m_projectionType);
            nextTraceControl->initProjector(m_tracesData[nextDataIndex].m_projectionType);
        }

        void resize(int traceSize)
        {
            m_traceSize = traceSize;

            if (m_traceSize > m_maxTraceSize)
            {
                delete[] m_x0;
                delete[] m_x1;
                m_x0 = new float[2*m_traceSize*GLScopeSettings::m_maxNbTraces];
                m_x1 = new float[2*m_traceSize*GLScopeSettings::m_maxNbTraces];

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

            for (std::vector<TraceControl*>::iterator it = m_tracesControl.begin(); it != m_tracesControl.end(); ++it) {
                (*it)->m_traceCount[currentBufferIndex()] = 0;
            }
        }

        void resetControls()
        {
            for (auto traceControl : m_tracesControl) {
                traceControl->reset();
            }
        }

    private:
        float *m_x0;
        float *m_x1;
    };

    class TriggerComparator
    {
    public:
        TriggerComparator() : m_level(0), m_reset(true) {
            computeLevels();
        }

        bool triggered(const Complex& s, TriggerCondition& triggerCondition)
        {
            if (triggerCondition.m_triggerData.m_triggerLevel != m_level)
            {
                m_level = triggerCondition.m_triggerData.m_triggerLevel;
                computeLevels();
            }

            bool condition, trigger;

            if (triggerCondition.m_projector.getProjectionType() == Projector::ProjectionMagDB) {
                condition = triggerCondition.m_projector.run(s) > m_levelPowerDB;
            } else if (triggerCondition.m_projector.getProjectionType() == Projector::ProjectionMagLin) {
                condition = triggerCondition.m_projector.run(s) > m_levelPowerLin;
            } else if (triggerCondition.m_projector.getProjectionType() == Projector::ProjectionMagSq) {
                condition = triggerCondition.m_projector.run(s) > m_levelPowerLin;
            } else {
                condition = triggerCondition.m_projector.run(s) > m_level;
            }

            if (condition)
            {
                if (triggerCondition.m_trues < triggerCondition.m_triggerData.m_triggerHoldoff) {
                    condition = false;
                    triggerCondition.m_trues++;
                }
                else
                {
                    triggerCondition.m_falses = 0;
                }
            }
            else
            {
                if (triggerCondition.m_falses < triggerCondition.m_triggerData.m_triggerHoldoff) {
                    condition = true;
                    triggerCondition.m_falses++;
                }
                else
                {
                    triggerCondition.m_trues = 0;
                }
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
//                qDebug("ScopeVis::triggered: %s/%s %f/%f",
//                        triggerCondition.m_prevCondition ? "T" : "F",
//                        condition ? "T" : "F",
//                        triggerCondition.m_projector->run(s),
//                        triggerCondition.m_triggerData.m_triggerLevel);
//            }

            triggerCondition.m_prevCondition = condition;
            return trigger;
        }

        void reset() {
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

    GLScopeInterface* m_glScope;
    SpectrumVis *m_spectrumVis;
    bool m_ssbSpectrum;
    GLScopeSettings m_settings;
    MessageQueue m_inputMessageQueue;
    uint32_t m_preTriggerDelay;                    //!< Pre-trigger delay in number of samples
    uint32_t m_livePreTriggerDelay;                //!< Pre-trigger delay in number of samples in live mode
    std::vector<TriggerCondition*> m_triggerConditions; //!< Chain of triggers
    uint32_t m_currentTriggerIndex;                //!< Index of current index in the chain
    uint32_t m_focusedTriggerIndex;                //!< Index of the trigger that has focus
    TriggerState m_triggerState;                   //!< Current trigger state
    Traces m_traces;                               //!< Displayable traces
    int m_focusedTraceIndex;                       //!< Index of the trace that has focus
    uint32_t m_nbStreams;
    uint32_t m_traceChunkSize;                     //!< Trace length unit size in number of samples
    uint32_t m_traceSize;                          //!< Size of traces in number of samples
    uint32_t m_liveTraceSize;                      //!< Size of traces in number of samples in live mode
    int m_nbSamples;                               //!< Number of samples yet to process in one complex trace
    uint32_t m_timeBase;                           //!< Trace display time divisor
    uint32_t m_timeOfsProMill;                     //!< Start trace shift in 1/1000 trace size
    bool m_traceStart;                             //!< Trace is at start point
    int m_triggerLocation;                         //!< Trigger location from end point
    int m_sampleRate;                              //!< Actual sample rate being used
    int m_liveSampleRate;                          //!< Sample rate in live mode
    TraceBackDiscreteMemory m_traceDiscreteMemory; //!< Complex trace memory
    ConvertBuffers m_convertBuffers;               //!< Sample to Complex conversions
    bool m_freeRun;                                //!< True if free running (trigger globally disabled)
    int m_maxTraceDelay;                           //!< Maximum trace delay
    TriggerComparator m_triggerComparator;         //!< Compares sample level to trigger level
    QRecursiveMutex m_mutex;
    Real m_projectorCache[(int) Projector::nbProjectionTypes];
    bool m_triggerOneShot;                         //!< True when one shot mode is active
    bool m_triggerWaitForReset;                    //!< In one shot mode suspended until reset by UI
    uint32_t m_currentTraceMemoryIndex;            //!< The current index of trace in memory (0: current)


    void applySettings(const GLScopeSettings& settings, bool force = false);
    void addTrace(const GLScopeSettings::TraceData& traceData);
    void changeTrace(const GLScopeSettings::TraceData& traceData, uint32_t traceIndex);
    void removeTrace(uint32_t traceIndex);
    void moveTrace(uint32_t traceIndex, bool upElseDown);
    void focusOnTrace(uint32_t traceIndex);
    void addTrigger(const GLScopeSettings::TriggerData& triggerData);
    void changeTrigger(const GLScopeSettings::TriggerData& triggerData, uint32_t triggerIndex);
    void removeTrigger(uint32_t triggerIndex);
    void moveTrigger(uint32_t triggerIndex, bool upElseDown);
    void focusOnTrigger(uint32_t triggerIndex);

    /**
     * Moves on to the next trigger if any or increments trigger count if in repeat mode
     * - If not final it returns true
     * - If final i.e. signal is actually triggerd it returns false
     */
    bool nextTrigger(); //!< Returns true if not final

    /**
     * Process a sample trace which length is at most the trace length (m_traceSize)
     */
    void processTrace(const std::vector<ComplexVector::const_iterator>& vbegin, int length, int& triggerPointToEnd);

    /**
     * process a trace in memory at current trace index in memory
     */
    void processMemoryTrace();

    /**
     * Process traces from complex trace memory buffer.
     * - if finished it returns the number of unprocessed samples left in the buffer
     * - if not finished it returns -1
     */
    int processTraces(const std::vector<ComplexVector::const_iterator>& vbegin, int length, bool traceBack = false);

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

    /**
     * Set the actual sample rate
     */
    void setSampleRate(int sampleRate);

    /**
     * Set the traces size
     */
    void setTraceSize(uint32_t traceSize, bool emitSignal = false);

    /**
     * Set the pre trigger delay
     */
    void setPreTriggerDelay(uint32_t preTriggerDelay, bool emitSignal = false);

private slots:
    void handleInputMessages();
};



#endif /* SDRBASE_DSP_SCOPEVISNG_H_ */
