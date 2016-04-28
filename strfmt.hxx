#ifndef _STRING_FORMATTER_HXX_
#define _STRING_FORMATTER_HXX_

namespace cornerstone {
    template<int N>
    class strfmt {
    public:
        strfmt(const char* fmt)
            : fmt_(fmt) {
        }

        template<typename ... TArgs>
        const char* fmt(TArgs... args) {
            ::snprintf(buf_, N, fmt_, args...);
            return buf_;
        }

    __nocopy__(strfmt)
    private:
        char buf_[N];
        const char* fmt_;
    };

    typedef strfmt<100> sstrfmt;
    typedef strfmt<200> lstrfmt;
}

#endif //_STRING_FORMATTER_HXX_