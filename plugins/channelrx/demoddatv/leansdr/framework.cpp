#include "framework.h"

namespace leansdr
{

void fatal(const char *s) {
    perror(s);
}

void fail(const char *s) {
    fprintf(stderr, "** %s\n", s);
}

} // leansdr
