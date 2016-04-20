#ifndef _LOG_ENTRY_HXX_
#define _LOG_ENTRY_HXX_
namespace cornerstone{
    class log_entry{
	public:
		log_entry(ulong term, buffer* buff, log_val_type value_type = log_val_type::app_log)
			: term_(term), buff_(buff), value_type_(value_type) {
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

    private:
        ulong term_;
		log_val_type value_type_;
		buffer* buff_;
    };
}
#endif //_LOG_ENTRY_HXX_