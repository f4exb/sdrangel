///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// http://stackoverflow.com/questions/10990618/calculate-rolling-moving-average-in-c //
//                                                                                   //
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                       //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef _UTIL_MOVINGAVERAGE_H_
#define _UTIL_MOVINGAVERAGE_H_

#include <algorithm>

template <typename T, typename Total, int N>
class MovingAverageUtil
{
  public:
    MovingAverageUtil()
      : m_num_samples(0), m_index(0), m_total(0)
    { }

    void reset()
    {
        m_num_samples = 0;
        m_index = 0;
        m_total = 0;
    }

    void operator()(T sample)
    {
        if (m_num_samples < N) // fill up
        {
            m_samples[m_num_samples++] = sample;
            m_total += sample;
        }
        else // roll
        {
            T& oldest = m_samples[m_index];
            m_total += sample - oldest;
            oldest = sample;
            m_index = (m_index + 1) % N;
        }
    }

    double asDouble() const { return ((double)m_total) / N; }
    float asFloat() const { return ((float)m_total) / N; }
    operator T() const { return  m_total / N; }
    T instantAverage() const { return m_total / (m_num_samples == 0 ? 1 : m_num_samples); }

  private:
    T m_samples[N];
    int m_num_samples;
    unsigned int m_index;
    Total m_total;
};


template <typename T, typename Total>
class MovingAverageUtilVar
{
  public:
    MovingAverageUtilVar(unsigned int size)
      : m_num_samples(0), m_index(0), m_total(0)
    {
        m_samples.resize(size);
		m_samplesSizeInvF = 1.0f / size;
		m_samplesSizeInvD = 1.0 / size;
	}

    void reset()
    {
        m_num_samples = 0;
        m_index = 0;
        m_total = 0;
    }

    void resize(unsigned int size)
    {
        reset();
        m_samples.resize(size);
		m_samplesSizeInvF = 1.0f / size;
		m_samplesSizeInvD = 1.0 / size;
	}

    unsigned int size() const
    {
        return m_samples.size();
    }

    void operator()(T sample)
    {
        if (m_num_samples < m_samples.size()) // fill up
        {
            m_samples[m_num_samples++] = sample;
            m_total += sample;
        }
        else // roll
        {
            T& oldest = m_samples[m_index];
            m_total += sample - oldest;
            oldest = sample;
            m_index++;
            if (m_index == m_samples.size())
                m_index = 0;
        }
    }

    double asDouble() const { return m_total * m_samplesSizeInvD; }
    float asFloat() const { return m_total * m_samplesSizeInvF; }
    operator T() const { return  m_total / m_samples.size(); }
    T instantAverage() const { return m_total / (m_num_samples == 0 ? 1 : m_num_samples); }

  private:
    std::vector<T> m_samples;
    unsigned int m_num_samples;
    unsigned int m_index;
    Total m_total;
	float m_samplesSizeInvF;
	double m_samplesSizeInvD;
};


#endif /* _UTIL_MOVINGAVERAGE_H_ */
