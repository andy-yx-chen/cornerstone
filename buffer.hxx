#ifndef _BUFFER_HXX_
#define _BUFFER_HXX_

namespace cornerstone {
    class buffer {
    public:
        static void release(buffer* buff);
        static buffer* alloc(const size_t size);

        size_t size() const;
        size_t pos() const;
		void pos(size_t p);

        int32 get_int();
        ulong get_ulong();
        byte get_byte();
        const char* get_str();
        byte* data();

        void put(byte b);
        void put(int32 val);
        void put(ulong val);
        void put(const std::string& str);
    };
}
#endif //_BUFFER_HXX_