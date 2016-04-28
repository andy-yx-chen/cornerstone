#ifndef _BUFFER_HXX_
#define _BUFFER_HXX_

class buffer {
public:
    static void release(buffer* buff);
    static buffer* alloc(const size_t size);

    size_t size() const;
    size_t avail() const;
    size_t limit() const;

    int get_int();
    ulong get_ulong();
};

#endif //_BUFFER_HXX_