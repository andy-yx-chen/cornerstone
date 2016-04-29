#include "cornerstone.hxx"

#define __is_big_block(p) (1 & (ulong)(p))
#define __init_block(p, s, t) ((t*)(p))[0] = (t)s;\
    ((t*)(p))[0] = 0
#define __init_s_block(p, s) __init_block(p, s, ushort)
#define __init_b_block(p, s) __init_block(p, s, size_t);\
    (p) = (void*)(((ulong)(p)) | 1)
#define __size_of_block(p) (__is_big_block(p)) ? *((size_t*)(((ulong)(p)) ^ 1)) : *((ushort*)(p))
#define __pos_of_block(p) (__is_big_block(p)) ? ((size_t*)(((ulong)(p)) ^ 1))[1] : ((ushort*)(p))[1]
#define __mv_fw_block(p, d) if(__is_big_block(p)){\
    ((size_t*)(((ulong)(p)) ^ 1))[1] += (d);\
    }\
    else{\
    ((ushort*)(p))[1] += (d);\
    }
#define __data_of_block(p) (__is_big_block(p)) ? (byte*) (((byte*)(((size_t*)(((ulong)(p)) ^ 1)) + 2)) + __pos_of_block(p)) : (byte*) (((byte*)(((ushort*)(((ulong)(p)) ^ 1)) + 2)) + __pos_of_block(p))
using namespace cornerstone;

buffer* buffer::alloc(const size_t size) {
    if (size > std::numeric_limits<ushort>::max()) {
        void* ptr = ::malloc(size + sizeof(size_t) * 2);
        __init_b_block(ptr, size);
        return (buffer*)ptr;
    }

    void* ptr = ::malloc(size + sizeof(ushort) * 2);
    __init_s_block(ptr, size);
    return (buffer*)ptr;
}

void buffer::release(buffer* buf) {
    ::free((void*)buf);
}

size_t buffer::size() const {
    return (size_t)(__size_of_block(this));
}

size_t buffer::pos() const {
    return (size_t)(__pos_of_block(this));
}

byte* buffer::data() {
    return __data_of_block(this);
}