#pragma once

#ifdef DEBUG
    #define ASSERT(expr) do { \
        if (!(expr)) { \
            __disable_irq(); \
            __asm volatile ("bkpt #0"); \
            while(1) {} \
        } \
    } while(0)
#else
    #define ASSERT(expr) ((void)(expr))
#endif