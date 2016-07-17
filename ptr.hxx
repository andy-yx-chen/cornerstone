#ifndef _CS_PTR_HXX_
#define _CS_PTR_HXX_
namespace cornerstone {
    typedef std::atomic<int32> ref_counter_t;
    template<typename T> class ptr;
    template<typename T> class ptr<T&>;
    template<typename T> class ptr<T*>;
    template<typename T, typename ... TArgs>
    ptr<T> cs_new(TArgs... args) {
        size_t sz = sizeof(ref_counter_t) + sizeof(T);
        any_ptr p = ::malloc(sz);
        if (p == nilptr) {
            throw std::runtime_error("no memory");
        }

        ref_counter_t* p_int = new (p) ref_counter_t(0);
        T* p_t = new (reinterpret_cast<any_ptr>(p_int + 1)) T(args...);
        return ptr<T>(p);
    }

    template<typename T>
    ptr<T> cs_alloc(size_t size) {
        size_t sz = size + sizeof(ref_counter_t);
        any_ptr p = ::malloc(sz);
        if (p == nilptr) {
            throw std::runtime_error("no memory");
        }

        ref_counter_t* p_int = new (p) ref_counter_t(0);
        return ptr<T>(p);
    }

    template<typename T>
    inline ptr<T> cs_safe(T* t) {
        return ptr<T>(reinterpret_cast<any_ptr>(reinterpret_cast<ref_counter_t*>(t) - 1));
    }

    template<typename T>
    class ptr {
    private:
        ptr(any_ptr p)
            : p_(p) {
            _inc_ref();
        }
    public:
        ptr() : p_(nilptr) {}

        template<typename T1>
        ptr(const ptr<T1>& other)
            : p_(other.p_) {
            T* p = other.get();
            _inc_ref();
        }

        ptr(const ptr<T>& other)
            : p_(other.p_) {
            _inc_ref();
        }

        template<typename T1>
        ptr(ptr<T1>&& other)
            : p_(other.p_) {
            T* p = other.get();
            other.p_ = nilptr;
        }

        ptr(ptr<T>&& other)
            : p_(other.p_) {
            other.p_ = nilptr;
        }

        ~ptr() {
            if (p_ != nilptr) {
                _dec_ref_and_free();
            }
        }

        template<typename T1>
        ptr<T>& operator = (const ptr<T1>& other) {
            T* p = other.get();
            _dec_ref_and_free();
            p_ = other.p_;
            _inc_ref();
            return *this;
        }

        ptr<T>& operator = (const ptr<T>& other) {
            _dec_ref_and_free();
            p_ = other.p_;
            _inc_ref();
            return *this;
        }

    public:
        inline T* get() const {
            return p_ == nilptr ? nilptr : reinterpret_cast<T*>(reinterpret_cast<ref_counter_t*>(p_) + 1);
        }

        inline void reset() {
            _dec_ref_and_free();
            p_ = nilptr;
        }

        inline T* operator -> () const {
            return get();
        }

        inline T& operator *() const {
            return *get();
        }

        inline operator bool() const {
            return p_ != nilptr;
        }

        inline bool operator == (const ptr<T>& other) const {
            return p_ == other.p_;
        }

        inline bool operator != (const ptr<T>& other) const {
            return p_ != other.p_;
        }

        inline bool operator == (const T* p) const {
            if (p_ == nilptr) {
                return p == nilptr;
            }

            return get() == p;
        }

        inline bool operator != (const T* p) const {
            if (p_ == nilptr) {
                return p != nilptr;
            }

            return get() != p;
        }

    private:
        inline void _inc_ref() {
            if (p_ != nilptr)++ *reinterpret_cast<ref_counter_t*>(p_);
        }

        inline void _dec_ref_and_free() {
            if (p_ != nilptr && 0 == (-- *reinterpret_cast<ref_counter_t*>(p_))) {
                reinterpret_cast<ref_counter_t*>(p_)->~atomic<int32>();
                get()->~T();
                ::free(p_);
            }
        }
    private:
        any_ptr p_;
    public:
        template<typename T1>
        friend class ptr;
        template<typename T1, typename ... TArgs>
        friend ptr<T1> cs_new(TArgs... args);
        template<typename T1>
        friend ptr<T1> cs_safe(T1* t);
        template<typename T1>
        friend ptr<T1> cs_alloc(size_t size);
    };
}
#endif //_CS_PTR_HXX_
