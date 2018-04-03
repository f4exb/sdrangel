/*
 * Copyright (c) 2016-present Orlando Bassotto
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CROSS_COMPILING
/*
 * This is executed when targeting the host.
 */
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define ISA_X86_MMX    (1 << 0)
#define ISA_X86_SSE    (1 << 1)
#define ISA_X86_SSE2   (1 << 2)
#define ISA_X86_SSE3   (1 << 3)
#define ISA_X86_SSSE3  (1 << 4)
#define ISA_X86_SSE41  (1 << 5)
#define ISA_X86_SSE42  (1 << 6)
#define ISA_X86_AVX1   (1 << 7)
#define ISA_X86_AVX2   (1 << 8)
#define ISA_PPC_VMX    (1 << 9)
#define ISA_ARM_NEON   (1 << 10)
#define ISA_ARM64_NEON (1 << 11)

#if !(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64))
static jmp_buf return_jmp;

static void sighandler(int signo)
{
    longjmp(return_jmp, 1);
}
#endif

static void
siginit(void)
{
#if !(defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64))
    signal(SIGSEGV, sighandler);
#ifdef SIGBUS
    signal(SIGBUS, sighandler);
#endif
    signal(SIGILL, sighandler);
#endif
}

static unsigned
check_isa(void)
{
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64)
#if defined(_M_IX86) || defined(_M_AMD64)
	int regs[4];
#endif
    unsigned eax, ebx, ecx, edx;
    int supported;
    unsigned isa = 0;

#if defined(_M_IX86) || defined(_M_AMD64)
	__cpuid(regs, 1);
	ecx = regs[2];
	edx = regs[3];
#else
    eax = 0x00000001;
    __asm__ __volatile__(
            "cpuid"
            :"=c"(ecx)               // %ecx contains large feature flag set
            :"0"(eax)                // call with 0x1
            :"%eax","%ebx","%edx"
            );
#endif

    if (edx & (1 << 23)) {
        /* MMX */
        isa |= ISA_X86_MMX;
    }

    if (edx & (1 << 25)) {
        /* SSE */
        isa |= ISA_X86_SSE;
    }

    if (edx & (1 << 26)) {
        /* SSE */
        isa |= ISA_X86_SSE2;
    }

    if (ecx & (1 << 0)) {
        /* SSE3 */
        isa |= ISA_X86_SSE3;
    }

    if (ecx & (1 << 9)) {
        /* SSSE3 */
        isa |= ISA_X86_SSSE3;
    }

    if (ecx & (1 << 19)) {
        /* SSE4.1 */
        isa |= ISA_X86_SSE41;
    }

    if (ecx & (1 << 20)) {
        /* SSE4.2 */
        isa |= ISA_X86_SSE42;
    }

    if (ecx & (1 << 28)) {
        /* AVX1 */
        isa |= ISA_X86_AVX1;

#if defined(_M_IX86) || defined(_M_AMD64)
		__cpuid(regs, 7);
		ebx = regs[1];
#else
        eax = 0x00000007;
		__asm__ __volatile__(
                "cpuid"
                : "=b"(ebx)
                : "0"(eax)
                : "%eax", "%ecx", "%edx"
                );
#endif

        if (ebx & (1 << 5)) {
            /* AVX2 */
            isa |= ISA_X86_AVX2;
        }
    }

    return isa;
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(__powerpc64le__)
    if (setjmp(return_jmp) == 1)
        return 0;

    __asm__ __volatile__(".long 0x10011000");

    return ISA_PPC_VMX;
#elif defined(__arm__) || defined(_M_ARM)
	/* Windows on ARM is always Thumb-2, and NEON is always available. */
#if !defined(_M_ARM)
    if (setjmp(return_jmp) == 1)
        return 0;

#if defined(__thumb__)
    __asm__ __volatile__(".short 0xef12");
    __asm__ __volatile__(".short 0x0054");
#else
    __asm__ __volatile__(".word 0xf2120054");
#endif
#endif

    return ISA_ARM_NEON;
#elif defined(__aarch64__) || defined(__arm64__)
    return ISA_ARM64_NEON;
#else
    return 0;
#endif
}

int
main()
{
    unsigned isa;

    siginit();
    isa = check_isa();

    if (isa & ISA_X86_MMX) {
        printf("@mmx@");
    }
    if (isa & ISA_X86_SSE) {
        printf("@sse@");
    }
    if (isa & ISA_X86_SSE2) {
        printf("@sse2@");
    }
    if (isa & ISA_X86_SSE3) {
        printf("@sse3@");
    }
    if (isa & ISA_X86_SSSE3) {
        printf("@ssse3@");
    }
    if (isa & ISA_X86_SSE41) {
        printf("@sse41@");
    }
    if (isa & ISA_X86_SSE42) {
        printf("@sse42@");
    }
    if (isa & ISA_X86_AVX1) {
        printf("@avx@");
    }
    if (isa & ISA_X86_AVX2) {
        printf("@avx2@");
    }
    if (isa & ISA_PPC_VMX) {
        printf("@vmx@");
    }
    if (isa & ISA_ARM_NEON) {
        printf("@neon@");
    }
    if (isa & ISA_ARM64_NEON) {
        printf("@neon64@");
    }
    printf("\n");
    return 0;
}
#else
/*
 * This is just compiled when targeting an architecture/system different
 * than the host, it is just compiled hence the checks are on the compiler
 * features available and not on the real CPU characteristics, as such these
 * tests may not do what you want, you may need to force the characteristics
 * in that case.
 *
 * A define TEST_xyz must be defined in order to check the support for the
 * specified characteristic.
 */

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64)
#define _X86
#endif

#if defined(__powerpc__) || defined(__powerpc64__) || defined(__powerpc64le__)
#define _PPC
#endif

#if defined(__arm__)
#define _ARM
#endif

#if defined(__aarch64__) || defined(__arm64__)
#define _ARM64
#endif

#if defined(TEST_mmx) && !(defined(_X86) && defined(__MMX__))
#error "MMX not available"
#endif

#if defined(TEST_sse) && !(defined(_X86) && defined(__SSE__))
#error "SSE not available"
#endif

#if defined(TEST_sse2) && !(defined(_X86) && defined(__SSE2__))
#error "SSE2 not available"
#endif

#if defined(TEST_sse3) && !(defined(_X86) && defined(__SSE3__))
#error "SSE3 not available"
#endif

#if defined(TEST_ssse3) && !(defined(_X86) && defined(__SSSE3__))
#error "SSSE3 not available"
#endif

#if defined(TEST_sse41) && !(defined(_X86) && defined(__SSE4_1__))
#error "SSE4.1 not available"
#endif

#if defined(TEST_sse42) && !(defined(_X86) && defined(__SSE4_2__))
#error "SSE4.2 not available"
#endif

#if defined(TEST_avx) && !(defined(_X86) && defined(__AVX__))
#error "AVX1 not available"
#endif

#if defined(TEST_avx2) && !(defined(_X86) && defined(__AVX2__))
#error "AVX2 not available"
#endif

#if defined(TEST_vmx) && !(defined(_PPC) && defined(__ALTIVEC__))
#error "VMX not available"
#endif

#if defined(TEST_neon) && !(defined(_ARM) && defined(__ARM_NEON__))
#error "NEON not available"
#endif

#if defined(TEST_neon64) && !defined(_ARM64)
#error "NEON64 not available"
#endif

int main()
{
    return 0;
}
#endif

