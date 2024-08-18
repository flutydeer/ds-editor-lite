#ifndef LANGGLOBAL_H
#define LANGGLOBAL_H

#ifdef _MSC_VER
#  define LANG_MANAGER_DECL_EXPORT __declspec(dllexport)
#  define LANG_MANAGER_DECL_IMPORT __declspec(dllimport)
#else
#  define LANG_MANAGER_DECL_EXPORT __attribute__((visibility("default")))
#  define LANG_MANAGER_DECL_IMPORT __attribute__((visibility("default")))
#endif

#ifndef LANG_MANAGER_EXPORT
#  ifdef LANG_MANAGER_STATIC
#    define LANG_MANAGER_EXPORT
#  else
#    ifdef LANG_MANAGER_LIBRARY
#      define LANG_MANAGER_EXPORT LANG_MANAGER_DECL_EXPORT
#    else
#      define LANG_MANAGER_EXPORT LANG_MANAGER_DECL_IMPORT
#    endif
#  endif
#endif

#endif //LANGGLOBAL_H
