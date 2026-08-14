#ifndef LIBCOUNTER_H
#define LIBCOUNTER_H

#if defined(_WIN32) && defined(LIBCOUNTER_BUILD)
#define LIBCOUNTER_API __declspec(dllexport)
#elif defined(_WIN32)
#define LIBCOUNTER_API __declspec(dllimport)
#else
#define LIBCOUNTER_API
#endif

LIBCOUNTER_API void counter_reset(void);
LIBCOUNTER_API void counter_bump(void);
LIBCOUNTER_API int counter_get(void);

#endif
