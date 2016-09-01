#ifndef _LOG_ENTRY_HXX_
#define _LOG_ENTRY_HXX_
namespace cornerstone{
    class log_entry{
    public:
        log_entry(ulong term, const ptr<buffer>& buff, log_val_type value_type = log_val_type::app_log)
            : term_(term), value_type_(value_type), buff_(buff) {
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
            // we accept nil buffer, but in that case, the get_buf() shouldn't be called, throw runtime exception instead of having segment fault (AV on Windows)
            if (!buff_) {
                throw std::runtime_error("get_buf cannot be called for a log_entry with nil buffer");
            }

            return *buff_;
        }

        ptr<buffer> serialize() {
            buff_->pos(0);
            ptr<buffer> buf = buffer::alloc(sizeof(ulong) + sizeof(char) + buff_->size());
            buf->put(term_);
            buf->put((static_cast<byte>(value_type_)));
            buf->put(*buff_);
            buf->pos(0);
            return buf;
        }

        static ptr<log_entry> deserialize(buffer& buf) {
            ulong term = buf.get_ulong();
            log_val_type t = static_cast<log_val_type>(buf.get_byte());
            ptr<buffer> data = buffer::copy(buf);
            return cs_new<log_entry>(term, data, t);
        }

        static ulong term_in_buffer(buffer& buf) {
            ulong term = buf.get_ulong();
            buf.pos(0); // reset the position
            return term;
        }

    private:
        ulong term_;
        log_val_type value_type_;
        ptr<buffer> buff_;
    };
}
#endif //_LOG_ENTRY_HXX_