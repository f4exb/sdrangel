///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_CIRCULARBUFFER_H_
#define INCLUDE_CIRCULARBUFFER_H_

#include <iterator>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <iostream>

// Iterates from oldest item to newest
template<typename T>
class CircularBufferIterator {

    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

public:
    CircularBufferIterator(std::vector<value_type> *b, size_t tail, size_t p) :
        m_buf(b),
        m_tail(tail),
        m_pos(p)
    {
    }

    value_type &operator*() { return (*m_buf)[(m_tail + m_pos) % m_buf->size()]; }
    value_type *operator->() { return &(operator*()); }

    CircularBufferIterator<value_type> &operator++()
    {
        m_pos++;
        return *this;
    }

    friend bool operator!= (const CircularBufferIterator<value_type>& a, const CircularBufferIterator<value_type>& b) { return a.m_buf != b.m_buf || a.m_tail != b.m_tail || a.m_pos != b.m_pos; };

private:
    std::vector<value_type> *m_buf;
    size_type m_tail;
    size_type m_pos;
};

// Circular buffer with fixed capacity, that overwrites oldest item when full.
// Items are stored in a contiguous block of memory, so can be accessed by index or iterated over.
// Index of 0 is the oldest item, index of size()-1 is the newest item.
template <class T>
class CircularBuffer
{
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    std::vector<value_type> m_buffer;
    size_type m_head;
    size_type m_tail;
    size_type m_count;

public:

   CircularBuffer(size_type size) :
        m_buffer(size),
        m_head(0),
        m_tail(0),
        m_count(0)
    {
    }

    bool isFull() const
    {
        return (m_count != 0) && (m_head == m_tail);
    }

    bool isEmpty() const
    {
        return m_count == 0;
    }

    size_type size() const
    {
        return m_count;
    }

    void clear()
    {
        m_head = 0;
        m_tail = 0;
        m_count = 0;
    }

    void append(const_reference item)
    {
        size_type next = (m_head + 1) % m_buffer.size();
        if (isFull()) {
            m_tail = next; // Overwrite oldest item
        } else {
            m_count++;
        }
        m_buffer[m_head] = item;
        m_head = next;
    }

    void removeFirst()
    {
        m_tail = (m_tail + 1) % m_buffer.size();
        m_count--;
    }

    reference takeFirst()
    {
        reference item = m_buffer[m_tail];
        removeFirst();
        return item;
    }

    // newSize of <= 1 not supported
    void resize(size_type newSize)
    {
        // Reduce count to newSize, by removing oldest items first
        while (size() > newSize) {
            removeFirst();
        }

        // Shuffle data to the beginning of the buffer, so we can free up or allocate space at the end
        if (m_head < m_tail)
        {
            std::rotate(m_buffer.begin(), m_buffer.begin() + m_tail, m_buffer.end());
            m_head = m_count;
            m_tail = 0;
        }
        else
        {
            std::rotate(m_buffer.begin(), m_buffer.begin() + m_tail, m_buffer.begin() + m_head);
            m_head = m_count;
            m_tail = 0;
        }

        m_buffer.resize(newSize);

        if (m_head >= newSize) {
            m_head -= newSize;
        }
    }

    // Index of 0 is the oldest item, index of size()-1 is the newest item
    reference operator[] (size_type index)
    {
        return m_buffer[(m_tail + index) % m_buffer.size()];
    }

    const_reference operator[] (size_type index) const
    {
        return m_buffer[(m_tail + index) % m_buffer.size()];
    }

    const_reference at(size_type index) const
    {
        return m_buffer.at((m_tail + index) % m_buffer.size());
    }

    using iterator = CircularBufferIterator<T>;

    iterator begin()
    {
        return iterator(&m_buffer, m_tail, 0);
    }

    iterator end()
    {
        return iterator(&m_buffer, m_tail, m_count);
    }

};

#endif /* INCLUDE_CIRCULARBUFFER_H_ */
