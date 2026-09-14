#ifndef USDK_PLATFORM_H
#define USDK_PLATFORM_H

/*
 * Export and calling-convention macros for the USDK public C ABI.
 * Every public header that declares an exported symbol includes this
 * file rather than hand-rolling __declspec/visibility attributes, so
 * there is exactly one place that encodes "how does this platform
 * export a symbol from a shared library".
 */

#if defined(_WIN32)
#  define USDK_CALL __cdecl
#  if defined(USDK_BUILDING_SHARED)
#    define USDK_EXPORT __declspec(dllexport)
#  elif defined(USDK_STATIC)
#    define USDK_EXPORT
#  else
#    define USDK_EXPORT __declspec(dllimport)
#  endif
#else
#  define USDK_CALL
#  if defined(USDK_STATIC)
#    define USDK_EXPORT
#  else
#    define USDK_EXPORT __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
#  define USDK_BEGIN_DECLS extern "C" {
#  define USDK_END_DECLS }
#else
#  define USDK_BEGIN_DECLS
#  define USDK_END_DECLS
#endif

#endif /* USDK_PLATFORM_H */
