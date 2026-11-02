#ifndef PUBLIC_ERR_H_
#define PUBLIC_ERR_H_

typedef enum {
    MCFW_NOT_FOUND,
    MCFW_INVALID_ARG,
    MCFW_EXPIRED,
    MCFW_MALFORMED
} McfwError; 

#define make_result(T, prefix)  \
    typedef struct {            \
        bool is_ok;             \
        union {                 \
            T value;            \
            McfwError err;      \
        };                      \
    } prefix##Result

#endif // PUBLIC_ERR_H_