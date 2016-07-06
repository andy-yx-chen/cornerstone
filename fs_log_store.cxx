#include "cornerstone.hxx"

using namespace cornerstone;

class log_store_buffer {
public:
    log_store_buffer(ulong start_idx, int32 max_size)
        : buf_(), lock_(), start_idx_(start_idx), max_size_(max_size) {
    }

    ~log_store_buffer() {
        recur_lock(lock_);
        for (std::vector<buffer*>::const_iterator it = buf_.begin(); it != buf_.end(); ++it) {
            buffer::release(*it);
        }
    }

    ulong last_idx() {
        recur_lock(lock_);
        return start_idx_ + buf_.size();
    }

    ulong first_idx() {
        recur_lock(lock_);
        return start_idx_;
    }

    log_entry* last_entry() {
        recur_lock(lock_);
        if (buf_.size() > 0) {
            return log_entry::deserialize(*buf_[buf_.size() - 1]);
        }

        return nilptr;
    }

    log_entry* operator[](ulong idx) {
        recur_lock(lock_);
        int idx_within_buf = static_cast<int>(idx - start_idx_);
        if (idx_within_buf >= buf_.size() || idx_within_buf < 0) {
            return nilptr;
        }

        return log_entry::deserialize(*buf_[idx_within_buf]);
    }

    // [start, end), returns the start_idx_;
    ulong fill(ulong start, ulong end, std::vector<log_entry*>& result) {
        recur_lock(lock_);
        if (end < start_idx_) {
            return start_idx_;
        }

        int offset = static_cast<int>(start - start_idx_);
        if (offset > 0) {
            for (int i = 0; i < static_cast<int>(end - start); ++i, ++offset) {
                result.push_back(log_entry::deserialize(*buf_[offset]));
            }
        }
        else {
            offset *= -1;
            for (int i = 0; i < offset; ++i) {
                result.push_back(nilptr); // make room for items that doesn't found in the buffer
            }

            for (int i = 0; i < static_cast<int>(end - start_idx_); ++i, ++offset) {
                result.push_back(log_entry::deserialize(*buf_[i]));
            }
        }

        return start_idx_;
    }

    // trimming the buffer [start, end)
    void trim(ulong start) {
        recur_lock(lock_);
        int index = static_cast<int>(start - start_idx_);
        if (index < buf_.size()) {
            buf_.erase(buf_.begin() + index, buf_.end());
        }
    }

    void append(log_entry& entry) {
        recur_lock(lock_);
        buf_.push_back(entry.serialize());
        if (max_size_ < buf_.size()) {
            buf_.erase(buf_.begin());
            start_idx_ += 1;
        }
    }

    void reset(ulong start_idx) {
        recur_lock(lock_);
        buf_.clear();
        start_idx_ = start_idx;
    }
private:
    std::vector<buffer*> buf_;
    std::recursive_mutex lock_;
    ulong start_idx_;
    int32 max_size_;
};

