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