#ifndef _LOG_ENTRY_HXX_
#define _LOG_ENTRY_HXX_
namespace cornerstone{
    class log_entry{
    public:
        log_entry(ulong term, buffer* buff, log_val_type value_type = log_val_type::app_log)
            : term_(term), value_type_(value_type), buff_(buff) {
        }

        ~log_entry() {
            buffer::release(this->buff_);
        }

    __nocopy__(log_entry)

    public:
        ulong get_term() const {
            return term_;
        }

        log_val_type get_val_type() const {
            return value_type_;
        }

        buffer& get_buf() const {
            return *buff_;
        }

        buffer* serialize() {
            buff_->pos(0);
            buffer* buf = buffer::alloc(sizeof(ulong) + sizeof(char) + buff_->size());
            buf->put(term_);
            buf->put((static_cast<byte>(value_type_)));
            buf->put(*buff_);
            buf->pos(0);
            return buf;
        }

        static log_entry* deserialize(buffer& buf) {
            ulong term = buf.get_ulong();
            log_val_type t = static_cast<log_val_type>(buf.get_byte());
            buffer* data = buffer::copy(buf);
            return new log_entry(term, data, t);
        }

    private:
        ulong term_;
        log_val_type value_type_;
        buffer* buff_;
    };
}
#endif //_LOG_ENTRY_HXX_