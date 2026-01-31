#ifndef SDRBASE_UTIL_STRINGLIST_H_
#define SDRBASE_UTIL_STRINGLIST_H_

#include <QStringList>

namespace StringListUtil {

static bool containsAll(const QStringList &haystack, const QStringList &needles);
static bool containsAny(const QStringList &haystack, const QStringList &needles);

} // namespace StringListUtil

#endif // SDRBASE_UTIL_STRINGLIST_H_
