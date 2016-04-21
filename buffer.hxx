#ifndef _BUFFER_HXX_
#define _BUFFER_HXX_

class buffer {
public:
    static void release(buffer* buff);
    static buffer* alloc(const size_t size);
};

#endif //_BUFFER_HXX_