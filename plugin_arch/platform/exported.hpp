// Symbol visibility macros for cross-platform shared libraries.
// Plugins are always loaded at runtime (dlopen / LoadLibrary), never linked
// via import libraries, so we always export — dllimport is never needed.
//
// Adapted from https://atomheartother.github.io/
// Modified by Bernardo Cohen

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef __GNUC__
    #define EXPORTED __attribute__((dllexport))
  #else
    #define EXPORTED __declspec(dllexport)
  #endif
  #define NOT_EXPORTED
#else
  #if __GNUC__ >= 4
    #define EXPORTED __attribute__((visibility("default")))
    #define NOT_EXPORTED __attribute__((visibility("hidden")))
  #else
    #define EXPORTED
    #define NOT_EXPORTED
  #endif
#endif
