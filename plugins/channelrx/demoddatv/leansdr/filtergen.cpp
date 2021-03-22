
#include <stdio.h>

#include "filtergen.h"

namespace leansdr
{
namespace filtergen
{

void dump_filter(const char *name, int ncoeffs, float *coeffs)
{
    fprintf(stderr, "%s = [", name);

    for (int i = 0; i < ncoeffs; ++i) {
        fprintf(stderr, "%s %f", (i ? "," : ""), coeffs[i]);
    }

    fprintf(stderr, " ];\n");
}

} // filtergen
} // leansdr
