#pragma once

#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#define HIPE_PAUSE __builtin_ia32_pause
#elif defined(_MSC_VER)
#define HIPE_PAUSE _mm_pause
#else
#define HIPE_PAUSE __builtin_ia32_pause
#endif