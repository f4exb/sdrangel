#ifndef SDRBASE_UTIL_STRINGLIST_H_
#define SDRBASE_UTIL_STRINGLIST_H_

#include <QStringList>

#include "export.h"
class SDRBASE_API StringListUtil {

public:
    static bool containsAll(const QStringList &haystack, const QStringList &needles);
    static bool containsAny(const QStringList &haystack, const QStringList &needles);

};

#endif // SDRBASE_UTIL_STRINGLIST_H_
