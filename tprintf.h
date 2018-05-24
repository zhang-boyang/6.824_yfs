#ifndef TPRINTF_H
#define TPRINTF_H

#define tprintf(args...) do { \
        struct timeval tv;     \
        gettimeofday(&tv, 0); \
        printf("%lu:\t", tv.tv_sec * 1000 + tv.tv_usec / 1000);\
        printf("[tid:%lld:%s:%s] line:%d\n", (long long int)pthread_self(),__FILE__, __func__, __LINE__);\
        printf(args);   \
        } while (0);
#endif
