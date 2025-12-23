#ifndef ASSERT_H
#define ASSERT_H

#define assert(expression) \
    if (!(expression)) { \
        printf("Assertion failed: %s, file %s, line %d\n", #expression, __FILE__, __LINE__); \
        while (1); \
    }

#endif