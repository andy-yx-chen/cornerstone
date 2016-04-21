#ifndef _CLUSTER_CONFIG_HXX_
#define _CLUSTER_CONFIG_HXX_

namespace cornerstone {
    class cluster_config {
    public:
        cluster_config(ulong log_idx = 0L, ulong prev_log_idx = 0L)
            : log_idx_(log_idx), prev_log_idx_(prev_log_idx), servers_() {}

        ~cluster_config() {
            for (srv_itor it = servers_.begin(); it != servers_.end(); ++it) {
                delete (*it);
            }
        }

        __nocopy__(cluster_config)
    public:
        static cluster_config* deserialize(buffer* buf);

        ulong get_log_idx() const {
            return log_idx_;
        }

        void set_log_idx(ulong log_idx) {
            prev_log_idx_ = log_idx_;
            log_idx_ = log_idx;
        }

        ulong get_prev_log_idx() const {
            return prev_log_idx_;
        }

        std::list<srv_config*>& get_servers() {
            return servers_;
        }

        const srv_config*  get_server(int id) const {
            for (const_srv_itor it = servers_.begin(); it != servers_.end(); ++it) {
                if ((*it)->get_id() == id) {
                    return *it;
                }
            }

            return nilptr;
        }

        buffer* serialize();
    private:
        typedef std::list<srv_config*>::iterator srv_itor;
        typedef std::list<srv_config*>::const_iterator const_srv_itor;
        ulong log_idx_;
        ulong prev_log_idx_;
        std::list<srv_config*> servers_;
    };
}

#endif //_CLUSTER_CONFIG_HXX_