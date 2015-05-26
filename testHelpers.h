#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <functional>

typedef std::function<std::size_t(int)> IntHashFunction;

inline std::size_t dummyIntHash(int)
{
    return 7;
}

#endif