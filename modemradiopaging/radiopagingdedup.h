///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGDEDUP_H
#define MODEMRADIOPAGING_RADIOPAGINGDEDUP_H

#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Duplicate detection of the receiver of one network, as a pager does it: a pager keeps the last messages it
// received and takes a message that is one of them again as a duplicate. The key of a pager is the frequency of
// the receiver, the protocol, the network (polarity) and the address (with its function, the address number of
// GSC). A message is a duplicate of one of the last m_backlog messages of its key with the same payload, received
// within the window. The least recently used key is dropped when m_maxKeys are kept. Duplicates are not dropped:
// the receiver marks them and outputs them.
class RadioPagingDedup
{
public:
    static const size_t m_backlog = 16;         //!< Messages remembered for each key
    static const size_t m_maxKeys = 4096;

    explicit RadioPagingDedup(const std::string& protocol = std::string(), double window = 60.0) :
        m_protocol(protocol),
        m_window(window)
    {}

    //!< Seconds a message is remembered (default 60 s)
    void setWindow(double seconds) { m_window = seconds; }
    double window() const { return m_window; }
    //!< Frequency of the receiver, Hz: part of the key
    void setFrequency(int64_t frequency) { m_frequency = frequency; }
    void reset()
    {
        m_keys.clear();
        m_index.clear();
    }
    //!< Keys kept
    size_t size() const { return m_keys.size(); }
    //!< Messages remembered at the time now (received within the window), of a polarity (0 normal, 1 inverted) or of both (-1)
    size_t messages(double now, int polarity = -1) const
    {
        size_t count = 0;

        for (const Keyed& keyed : m_keys)
        {
            for (const Entry& e : keyed.second)
            {
                if (((now - e.m_time) <= m_window) && ((polarity < 0) || (e.m_polarity == polarity))) {
                    count++;
                }
            }
        }

        return count;
    }

    //!< True if the message is a duplicate. A message that is not is remembered. A message without payload (tone
    //!< only, a voice page) is not: a pager has nothing of it to store
    bool check(int polarity, const std::string& address, const std::vector<uint8_t>& data, int bits, double time)
    {
        if (bits <= 0) {
            return false;
        }
        const std::string key = std::to_string(m_frequency) + "|" + m_protocol + "|" + std::to_string(polarity) + "|" + address;
        auto found = m_index.find(key);
        Keyed *keyed;

        if (found != m_index.end())
        {
            // Most recently used first
            m_keys.splice(m_keys.begin(), m_keys, found->second);
            keyed = &m_keys.front();
        }
        else
        {
            while (m_keys.size() >= m_maxKeys)
            {
                m_index.erase(m_keys.back().first);
                m_keys.pop_back();
            }

            m_keys.emplace_front(key, std::deque<Entry>());
            m_index[key] = m_keys.begin();
            keyed = &m_keys.front();
        }

        std::deque<Entry>& entries = keyed->second;

        while (!entries.empty() && ((time - entries.front().m_time) > m_window)) {
            entries.pop_front();
        }

        for (const Entry& e : entries)
        {
            if ((e.m_bits == bits) && (e.m_data == data)) {
                return true;
            }
        }

        if (entries.size() >= m_backlog) {
            entries.pop_front();
        }

        entries.push_back(Entry{data, bits, time, polarity});
        return false;
    }

private:
    struct Entry
    {
        std::vector<uint8_t> m_data;
        int m_bits;
        double m_time;
        int m_polarity;
    };

    typedef std::pair<std::string, std::deque<Entry>> Keyed;

    std::string m_protocol;
    int64_t m_frequency = 0;
    double m_window;
    std::list<Keyed> m_keys;
    std::unordered_map<std::string, std::list<Keyed>::iterator> m_index;
};

#endif // MODEMRADIOPAGING_RADIOPAGINGDEDUP_H
