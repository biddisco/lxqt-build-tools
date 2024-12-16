//  Copyright (c) 2007-2019 Hartmut Kaiser
//  Copyright (c)      2011 Bryce Lelbach
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#if defined(DOXYGEN)
/// Marks a class or function to be exported from GROX or imported if it is
/// consumed.
# define GROX_EXPORT
#else

# if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#  if !defined(GROX_MODULE_STATIC_LINKING)
#   define GROX_SYMBOL_EXPORT __declspec(dllexport)
#   define GROX_SYMBOL_IMPORT __declspec(dllimport)
#   define GROX_SYMBOL_INTERNAL /* empty */
#  endif
# elif defined(__NVCC__) || defined(__CUDACC__)
#  define GROX_SYMBOL_EXPORT   /* empty */
#  define GROX_SYMBOL_IMPORT   /* empty */
#  define GROX_SYMBOL_INTERNAL /* empty */
# elif defined(GROX_HAVE_ELF_HIDDEN_VISIBILITY)
#  define GROX_SYMBOL_EXPORT __attribute__((visibility("default")))
#  define GROX_SYMBOL_IMPORT __attribute__((visibility("default")))
#  define GROX_SYMBOL_INTERNAL __attribute__((visibility("hidden")))
# endif

// make sure we have reasonable defaults
# if !defined(GROX_SYMBOL_EXPORT)
#  define GROX_SYMBOL_EXPORT /* empty */
# endif
# if !defined(GROX_SYMBOL_IMPORT)
#  define GROX_SYMBOL_IMPORT /* empty */
# endif
# if !defined(GROX_SYMBOL_INTERNAL)
#  define GROX_SYMBOL_INTERNAL /* empty */
# endif

///////////////////////////////////////////////////////////////////////////////
# if defined(GROX_EXPORTS)
#  define GROX_EXPORT GROX_SYMBOL_EXPORT
# else
#  define GROX_EXPORT GROX_SYMBOL_IMPORT
# endif

///////////////////////////////////////////////////////////////////////////////
// helper macro for symbols which have to be exported from the runtime and all
// components
# define GROX_ALWAYS_EXPORT GROX_SYMBOL_EXPORT
# define GROX_ALWAYS_IMPORT GROX_SYMBOL_IMPORT
#endif
