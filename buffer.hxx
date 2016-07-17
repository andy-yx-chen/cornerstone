#ifndef _BUFFER_HXX_
#define _BUFFER_HXX_

namespace cornerstone {
    class buffer {
        buffer() = delete;
        __nocopy__(buffer)
    public:
        static ptr<buffer> alloc(const size_t size);
        static ptr<buffer> copy(const buffer& buf);
        
        size_t size() const;
        size_t pos() const;
        void pos(size_t p);

        int32 get_int();
        ulong get_ulong();
        byte get_byte();
        const char* get_str();
        byte* data() const;

        void put(byte b);
        void put(int32 val);
        void put(ulong val);
        void put(const std::string& str);
        void put(const buffer& buf);
    };

    std::ostream& operator << (std::ostream& out, buffer& buf);
    std::istream& operator >> (std::istream& in, buffer& buf);
}
#endif //_BUFFER_HXX_