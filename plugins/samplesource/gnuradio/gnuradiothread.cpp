///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2013 by Dimitri Stolnikov <horiz0n@gmx.net>                     //
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

#include <stdio.h>
#include <errno.h>
#include "gnuradiothread.h"
#include "dsp/samplefifo.h"

#include <gnuradio/sync_block.h>
#include <gnuradio/io_signature.h>

////////////////////////////////////////////////////////////////////////////////
class gr_adaptor;

typedef boost::shared_ptr< gr_adaptor > gr_adaptor_sptr;

gr_adaptor_sptr make_gr_adaptor (SampleFifo* sampleFifo);

class gr_adaptor : public gr::sync_block
{
public:
	gr_adaptor (SampleFifo* sampleFifo);
	~gr_adaptor ();

	int work (int noutput_items,
		  gr_vector_const_void_star &input_items,
		  gr_vector_void_star &output_items);

private:
	SampleFifo *m_sampleFifo;
};

gr_adaptor_sptr
make_gr_adaptor (SampleFifo *sampleFifo)
{
	return gr_adaptor_sptr (new gr_adaptor (sampleFifo));
}

gr_adaptor::gr_adaptor (SampleFifo *sampleFifo)
	: gr::sync_block("gr_adaptor",
			 gr::io_signature::make(1, 1, sizeof (gr_complex)),
			 gr::io_signature::make(0, 0, 0)),
	  m_sampleFifo(sampleFifo)
{
}

gr_adaptor::~gr_adaptor ()
{
}

int
gr_adaptor::work (int noutput_items,
		  gr_vector_const_void_star &input_items,
		  gr_vector_void_star &output_items)
{
	const gr_complex *in = (const gr_complex *) input_items[0];

	std::vector<qint16> buffer(noutput_items * 2, 0);
	std::vector<qint16>::iterator it = buffer.begin();

	for (int i = 0; i < noutput_items; i++)
	{
		*it++ = in[i].real() * 32000;
		*it++ = in[i].imag() * 32000;
	}

	// we must push at least 4 bytes into the fifo
	m_sampleFifo->write((const quint8*)buffer.data(),
			    (it - buffer.begin()) * sizeof(qint16));

	// Tell runtime system how many input items we consumed on
	// each input stream.
	consume_each(noutput_items);

	// Tell runtime system how many output items we produced.
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
GnuradioThread::GnuradioThread(QString args, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_args(args),
	m_sampleFifo(sampleFifo)
{
}

GnuradioThread::~GnuradioThread()
{
	stopWork();
}

void GnuradioThread::startWork()
{
	m_startWaitMutex.lock();

	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);

	m_startWaitMutex.unlock();
}

void GnuradioThread::stopWork()
{
	m_running = false;

	m_top->stop();

	wait();
}

void GnuradioThread::run()
{
	m_top = gr::make_top_block( "flowgraph" );
	m_src = osmosdr::source::make( m_args.toStdString() );

	/* now since we've constructed our shared objects, we allow the calling
	 * thread to continue it's work and send some radio settings to us. */
	m_running = true;
	m_startWaiter.wakeAll();

	gr_adaptor_sptr adaptor = make_gr_adaptor(m_sampleFifo);
	m_top->connect(m_src, 0, adaptor, 0);

	m_top->run();

	m_running = false;
}
