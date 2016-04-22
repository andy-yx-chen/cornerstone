#ifndef _PEER_HXX_
#define _PEER_HXX_

namespace cornerstone {
    class peer {
    public:
        peer(const srv_config& config, const context& ctx, timer_task<peer&>::executor hb_exec)
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
            snp_sync_ctx_(nilptr),
            lock_(){
                rpc_ = ctx.rpc_cli_factory_->create_client(config.get_endpoint());
                hb_task_ = new timer_task<peer&>(hb_exec, *this);
        }

        ~peer() {
            delete rpc_;
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

        std::mutex& get_lock() {
            return lock_;
        }

        int get_current_hb_interval() const {
            return current_hb_interval_;
        }

        bool make_busy() {
            bool f = false;
            return busy_flag_.compare_exchange_strong(f, true);
        }

        void set_free() {
            busy_flag_.store(false);
        }

        bool is_hb_enabled() const {
            return hb_enabled_;
        }

        void enable_hb(bool enable) {
            hb_enabled_ = enable;
            if (!enable && hb_task_ != nilptr) {
                hb_task_->cancel();
            }
        }

        ulong get_next_log_idx() const {
            return next_log_idx_;
        }

        void set_next_log_idx(ulong idx) {
            next_log_idx_ = idx;
        }

        ulong get_matched_idx() const {
            return matched_idx_;
        }

        void set_matched_idx(ulong idx) {
            matched_idx_ = idx;
        }

        void set_pending_commit() {
            pending_commit_flag_.store(true);
        }

        bool clear_pending_commit() {
            bool t = true;
            return pending_commit_flag_.compare_exchange_strong(t, false);
        }

        void set_snapshot_in_sync(snapshot* s) {
            if (s == nilptr) {
                if (snp_sync_ctx_ != nilptr) {
                    delete snp_sync_ctx_;
                    snp_sync_ctx_ = nilptr;
                }
            }
            else {
                snp_sync_ctx_ = new snapshot_sync_ctx(s);
            }
        }

        snapshot_sync_ctx& get_snapshot_sync_ctx() const {
            return *snp_sync_ctx_;
        }

        void slow_down_hb() {
            current_hb_interval_ = std::min(max_hb_interval_, current_hb_interval_ + rpc_backoff_);
        }

        void resume_hb_speed() {
            current_hb_interval_ = hb_interval_;
        }

        void send_req(req_msg* req, async_result<resp_msg*>::handler_type& handler);
    private:
        void handle_rpc_result(req_msg* req, async_result<resp_msg*>* pending_result, resp_msg* resp, std::exception* err);
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
        std::mutex lock_;
    };
}

#endif //_PEER_HXX_
