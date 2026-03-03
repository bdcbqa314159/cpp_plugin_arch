// extern "C" wrapper macro for stable ABI across compilers.
// Adapted from https://caiorss.github.io/C-Cpp-Notes/CwrapperToQtLibrary.html
// Modified by Bernardo Cohen

#pragma once

#ifdef __cplusplus
  #define EXPORT_C extern "C"
#else
  #define EXPORT_C
#endif
