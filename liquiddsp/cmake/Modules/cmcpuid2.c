/**
 * Copyright (C) 2013 - present by OpenGamma Inc. and the OpenGamma group of companies
 *
 * Please see distribution for license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


/**
 * The supported instruction set on this machine for use in other functions.
 *
 * These must match up with the numbers used in MultiLib.cmake.
 */
typedef enum instructions_available_e
{
  supports_STANDARD = 1,
  supports_SSE41    = 2,
  supports_SSE42    = 3,
  supports_AVX1     = 4,
  supports_AVX2     = 5
} instructions_available;


instructions_available getSupportedInstructionSet() {

  // probes of cpuid with %eax=0000_0001h
  enum instructions_available_eax0000_0001h_e
  {
    probe01_SSE_3   = 1<<0,
    probe01_SSE_4_1 = 1<<19,
    probe01_SSE_4_2 = 1<<20,
    probe01_AVX1     = 1<<28
  };

  // probes of cpuid with %eax=0000_0007h
  enum instructions_available_eax0000_0007h_e
  {
    probe07_AVX2     = 1<<5
  };

  // the eax register
  int32_t EAX;
  // contends of the returned register
  int32_t supported;

  // Call cpuid with eax=0x00000001 and get ecx
  EAX = 0x00000001;
  __asm__("cpuid"
        :"=c"(supported)         // %ecx contains large feature flag set
        :"0"(EAX)                // call with 0x1
        :"%eax","%ebx","%edx");  // clobbered

  if(supported & probe01_AVX1) // we have at least AVX1
  {
    EAX = 0x00000007;
    __asm__("cpuid"
          :"=b"(supported)         // %ebx contains feature flag AVX2
          :"0"(EAX)                // call with 0x7
          :"%eax","%ecx","%edx");  // clobbered

    if(supported & probe07_AVX2) // we have at least AVX2
    {
      printf("AVX2 SUPPORTED\n");
      return supports_AVX2;
    }
    printf("AVX1 SUPPORTED\n");
    return supports_AVX1;
  }
  else if(supported & probe01_SSE_4_1) // we have at least SSE4.1
  {
    printf("SSE4.2 SUPPORTED\n");
    return supports_SSE42;
  }
  else // we have nothing specifically useful!
  {
    printf("STANDARD SUPPORTED\n");
    return supports_STANDARD;
  }
}


int main(void)
{
  instructions_available ia;
  ia = getSupportedInstructionSet();

  switch(ia)
  {
    case supports_AVX2:
      printf("AVX2\n");
      break;
    case supports_AVX1:
      printf("AVX1\n");
      break;
    case supports_SSE42:
      printf("SSE42\n");
      break;
    case supports_SSE41:
      printf("SSE41\n");
      break;
    case supports_STANDARD:
      printf("STANDARD\n");
      break;
    default:
      printf("Failed to find supported instruction set, this is an error!\n");
      exit(-1);
  }
  return ia;
}