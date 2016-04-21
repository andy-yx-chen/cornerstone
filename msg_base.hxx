#ifndef _MSG_BASE_HXX_
#define _MSG_BASE_HXX_
namespace cornerstone {

    class msg_base {
    public:
        msg_base(ulong term, msg_type type, int src, int dst)
            : term_(term), type_(type), src_(src), dst_(dst) {}

        virtual ~msg_base() {}

        ulong get_term() const {
            return this->term_;
        }

        msg_type get_type() const {
            return this->type_;
        }

        int get_src() const {
            return this->src_;
        }

        int get_dst() const {
            return this->dst_;
        }

    __nocopy__(msg_base)

    private:
        ulong term_;
        msg_type type_;
        int src_;
        int dst_;
    };
}
#endif //_MSG_BASE_HXX_