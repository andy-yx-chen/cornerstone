#ifndef _PEER_HXX_
#define _PEER_HXX_

namespace cornerstone {
    class peer {
    public:
        peer(const srv_config& config, const context& ctx, timer_task<peer&>::executor& hb_exec)
            : config_(config),
            rpc_(ctx.rpc_cli_factory_->create_client(config.get_endpoint())),
            current_hb_interval_(ctx.params_->heart_beat_interval_),
            hb_interval_(ctx.params_->heart_beat_interval_),
            rpc_backoff_(ctx.params_->rpc_failure_backoff_),
            max_hb_interval_(ctx.params_->max_hb_interval()),
            next_log_idx_(0),
            matched_idx_(0),
            busy_flag_(false),
            pending_commit_flag_(false),
            hb_enabled_(false),
            hb_exec_(hb_exec),
            hb_task_(new timer_task<peer&>(hb_exec, *this)),
            snp_sync_ctx_(nilptr),
            lock_(){
        }

    __nocopy__(peer)
    
    public:
        int32 get_id() const {
            return config_.get_id();
        }

        const srv_config& get_config() {
            return config_;
        }

        std::shared_ptr<delayed_task>& get_hb_task() {
            return hb_task_;
        }

        std::mutex& get_lock() {
            return lock_;
        }

        int32 get_current_hb_interval() const {
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
            if (!enable) {
                hb_task_->cancel();
                hb_task_.reset(new timer_task<peer&>(hb_exec_, *this));
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
                snp_sync_ctx_.reset();
            }
            else {
                snp_sync_ctx_.reset(new snapshot_sync_ctx(s));
            }
        }

        snapshot_sync_ctx* get_snapshot_sync_ctx() const {
            return snp_sync_ctx_.get();
        }

        void slow_down_hb() {
            current_hb_interval_ = std::min(max_hb_interval_, current_hb_interval_ + rpc_backoff_);
        }

        void resume_hb_speed() {
            current_hb_interval_ = hb_interval_;
        }

        void send_req(req_msg* req, rpc_handler& handler);
    private:
        void handle_rpc_result(req_msg* req, rpc_result* pending_result, resp_msg* resp, rpc_exception* err);
    private:
        const srv_config& config_;
        std::unique_ptr<rpc_client> rpc_;
        int32 current_hb_interval_;
        int32 hb_interval_;
        int32 rpc_backoff_;
        int32 max_hb_interval_;
        ulong next_log_idx_;
        ulong matched_idx_;
        std::atomic_bool busy_flag_;
        std::atomic_bool pending_commit_flag_;
        bool hb_enabled_;
        timer_task<peer&>::executor hb_exec_;
        std::shared_ptr<delayed_task> hb_task_;
        std::unique_ptr<snapshot_sync_ctx> snp_sync_ctx_;
        std::mutex lock_;
    };
}

#endif //_PEER_HXX_
