#pragma once

#if defined(__WIN32__) || defined(_WIN32)
#define DEBUG_BREAK __debugbreak
#else
#error platform not supported
#endif

