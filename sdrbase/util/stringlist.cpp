#include "stringlist.h"

static bool StringListUtil::containsAll(const QStringList &haystack, const QStringList &needles)
{
    for (const auto &s : needles) {
        if (!haystack.contains(s))   // optionally add Qt::CaseSensitivity
            return false;
    }
    return true;
}

static bool StringListUtil::containsAny(const QStringList &haystack, const QStringList &needles)
{
    for (const auto &s : needles) {
        if (haystack.contains(s))    // optionally add Qt::CaseSensitivity
            return true;
    }
    return false;
}
