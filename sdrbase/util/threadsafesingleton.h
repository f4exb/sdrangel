///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2016 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
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

#ifndef INCLUDE_UTIL_THREADSAFESINGLETON_H_
#define INCLUDE_UTIL_THREADSAFESINGLETON_H_

//! Template class used to make from a class a singleton.
/** S is the type of the singleton object to instantiate.
    <br> This class permits to gather all singleton code in the same place
    and not to pollute object interface with singleton methods.
    <br> Accessing to the instance of S is made through ThreadSafeSingleton<S>::instance()
    <br><B>Note :</B> The class using this adapter should have constructor and
    destructor protected and copy constructor and operator= private.
    <br>Besides it should declare the class ThreadSafeSingleton<S> friend in order
    this class can make the construction.
*/

template <class S> class ThreadSafeSingleton
{
public:
    static S& instance() { return *_pInstance; }

protected:
    ThreadSafeSingleton() {}
    ~ThreadSafeSingleton() { delete _pInstance; }
    static S* _pInstance;

private:
    ThreadSafeSingleton(const ThreadSafeSingleton&) {}
    ThreadSafeSingleton& operator = (const ThreadSafeSingleton&) {return *this;}
};

template <class S> S *ThreadSafeSingleton<S>::_pInstance = new S;

#endif /* INCLUDE_UTIL_THREADSAFESINGLETON_H_ */
