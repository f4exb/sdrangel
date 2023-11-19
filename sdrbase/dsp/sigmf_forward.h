///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
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
/*
 *    Copyright 2019 DeepSig Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef LIBSIGMF_SIGMF_FORWARD_H
#define LIBSIGMF_SIGMF_FORWARD_H

namespace sigmf {

    template<typename... T>
    class Global;

    template<typename... T>
    class Captures;

    template<typename... T>
    class Annotations;

    template<typename T>
    class SigMFVector : public std::vector<T> {
    public:
        T &create_new() {
            T new_element;
            this->emplace_back(new_element);
            return this->back();
        }
    };

    template<typename GlobalType, typename CaptureType, typename AnnotationType>
    struct SigMF {
        GlobalType global;
        SigMFVector<CaptureType> captures;
        SigMFVector<AnnotationType> annotations;
    };

}

// Missing bits...

namespace core {
    class DescrT;
}
namespace sdrangel {
    class DescrT;
}

namespace sigmf {
    template<typename...T> class Capture;
    template<typename...T> class Annotation;
}

#endif // LIBSIGMF_SIGMF_FORWARD_H
