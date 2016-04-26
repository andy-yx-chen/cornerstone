#ifndef _RESP_MSG_HXX_
#define _RESP_MSG_HXX_

namespace cornerstone {
    class resp_msg : public msg_base {
    public:
        resp_msg(ulong term, msg_type type, int src, int dst, ulong next_idx = 0L, bool accepted = false)
            : msg_base(term, type, src, dst), next_idx_(next_idx), accepted_(accepted) {}

        __nocopy__(resp_msg)

    public:
        ulong get_next_idx() const {
            return next_idx_;
        }

        bool get_accepted() const {
            return accepted_;
        }

        void accept(ulong next_idx) {
            next_idx_ = next_idx;
            accepted_ = true;
        }
    private:
        ulong next_idx_;
        bool accepted_;
    };
}

#endif //_RESP_MSG_HXX_