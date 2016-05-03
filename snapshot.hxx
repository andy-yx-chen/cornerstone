#ifndef _SNAPSHOT_HXX_
#define _SNAPSHOT_HXX_

namespace cornerstone {
    class snapshot {
    public:
        snapshot(ulong last_log_idx, ulong last_log_term, const std::shared_ptr<cluster_config>& last_config, ulong size = 0)
            : last_log_idx_(last_log_idx), last_log_term_(last_log_term), size_(size), last_config_(last_config){}

        __nocopy__(snapshot)

    public:
        ulong get_last_log_idx() const {
            return last_log_idx_;
        }

        ulong get_last_log_term() const {
            return last_log_term_;
        }

        ulong size() const {
            return size_;
        }

        const std::shared_ptr<cluster_config>& get_last_config() const {
            return last_config_;
        }

        static snapshot* deserialize(buffer& buf);

        buffer* serialize();

    private:
        ulong last_log_idx_;
        ulong last_log_term_;
        ulong size_;
        std::shared_ptr<cluster_config> last_config_;
    };
}

#endif