#ifndef CURVE_UTIL_GLOBAL_H
#define CURVE_UTIL_GLOBAL_H

#ifdef _MSC_VER
#define CURVE_UTIL_DECL_EXPORT __declspec(dllexport)
#define CURVE_UTIL_DECL_IMPORT __declspec(dllimport)
#else
#define CURVE_UTIL_DECL_EXPORT __attribute__((visibility("default")))
#define CURVE_UTIL_DECL_IMPORT __attribute__((visibility("default")))
#endif

#ifndef CURVE_UTIL_EXPORT
#ifdef CURVE_UTIL_STATIC
#define CURVE_UTIL_EXPORT
#else
#ifdef CURVE_UTIL_LIBRARY
#define CURVE_UTIL_EXPORT CURVE_UTIL_DECL_EXPORT
#else
#define CURVE_UTIL_EXPORT CURVE_UTIL_DECL_IMPORT
#endif
#endif
#endif

#endif // CURVE_UTIL_GLOBAL_H
