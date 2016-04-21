#ifndef _PEER_HXX_
#define _PEER_HXX_

namespace cornerstone {
    class peer {
    public:
        peer(const srv_config& config, const context& ctx, timer_task<peer&>::executor& hb_exec)
            : config_(config),
            current_hb_interval_(ctx.params_->heart_beat_interval_),
            hb_interval_(ctx.params_->heart_beat_interval_),
            rpc_backoff_(ctx.params_->rpc_failure_backoff_),
            max_hb_interval_(ctx.params_->max_hb_interval()),
            next_log_idx_(0),
            matched_idx_(0),
            busy_flag_(false),
            pending_commit_flag_(false),
            hb_enabled_(false),
            snp_sync_ctx_(nilptr){
                rpc_ = ctx.rpc_cli_factory_->create_client(config.get_endpoint());
                hb_task_ = new timer_task<peer&>(hb_exec, *this);
        }

        ~peer() {
            if (hb_task_ != nilptr) {
                delete hb_task_;
            }

            if (snp_sync_ctx_ != nilptr) {
                delete snp_sync_ctx_;
            }
        }

    __nocopy__(peer)
    
    public:
        int get_id() const {
            return config_.get_id();
        }

        const srv_config& get_config() {
            return config_;
        }

        timer_task<peer&>& get_hb_task() const {
            return *hb_task_;
        }
    private:
        const srv_config& config_;
        rpc_client* rpc_;
        int current_hb_interval_;
        int hb_interval_;
        int rpc_backoff_;
        int max_hb_interval_;
        ulong next_log_idx_;
        ulong matched_idx_;
        std::atomic_bool busy_flag_;
        std::atomic_bool pending_commit_flag_;
        bool hb_enabled_;
        timer_task<peer&>* hb_task_;
        snapshot_sync_ctx* snp_sync_ctx_;
    };
}

#endif //_PEER_HXX_
