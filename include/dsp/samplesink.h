#ifndef INCLUDE_SAMPLESINK_H
#define INCLUDE_SAMPLESINK_H

#include <QObject>
#include "dsptypes.h"
#include "util/export.h"

class Message;

class SDRANGELOVE_API SampleSink : public QObject {
public:
	SampleSink();
	virtual ~SampleSink();

	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual bool handleMessage(Message* cmd) = 0; //!< Handle message immediately or submit it to a queue
	virtual bool executeMessage(Message* cmd);    //!< Handle message immediately FIXME: shall we keep it or fix behaviour of ThreadedSampleSink?
};

#endif // INCLUDE_SAMPLESINK_H
