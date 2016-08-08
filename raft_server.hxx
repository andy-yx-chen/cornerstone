#ifndef _RAFT_SERVER_HXX_
#define _RAFT_SERVER_HXX_

namespace cornerstone {
    class raft_server : public msg_handler {
    public:
        raft_server(context* ctx)
        : leader_(-1),
            id_(ctx->state_mgr_.server_id()),
            votes_responded_(0),
            votes_granted_(0),
            quick_commit_idx_(0),
            election_completed_(true),
            config_changing_(false),
            catching_up_(false),
            stopping_(false),
            steps_to_down_(0),
            snp_in_progress_(),
            ctx_(ctx),
            scheduler_(ctx->scheduler_),
            election_exec_(std::bind(&raft_server::handle_election_timeout, this)),
            election_task_(),
            peers_(),
            role_(srv_role::follower),
            state_(ctx->state_mgr_.read_state()),
            log_store_(ctx->state_mgr_.load_log_store()),
            state_machine_(ctx->state_machine_),
            l_(ctx->logger_),
            config_(ctx->state_mgr_.load_config()),
            srv_to_join_(),
            conf_to_add_(),
            lock_(),
            commit_lock_(),
            commit_cv_(),
            stopping_lock_(),
            ready_to_stop_cv_(),
            resp_handler_((rpc_handler)std::bind(&raft_server::handle_peer_resp, this, std::placeholders::_1, std::placeholders::_2)),
            ex_resp_handler_((rpc_handler)std::bind(&raft_server::handle_ext_resp, this, std::placeholders::_1, std::placeholders::_2)){
            uint seed = (uint)(std::chrono::system_clock::now().time_since_epoch().count() * id_);
            std::default_random_engine engine(seed);
            std::uniform_int_distribution<int32> distribution(ctx->params_->election_timeout_lower_bound_, ctx->params_->election_timeout_upper_bound_);
            rand_timeout_ = std::bind(distribution, engine);
            if (!state_) {
                state_ = cs_new<srv_state>();
                state_->set_commit_idx(0);
                state_->set_term(0);
                state_->set_voted_for(-1);
            }

            for (ulong i = std::max(state_->get_commit_idx() + 1, log_store_->start_index()); i < log_store_->next_slot(); ++i) {
                ptr<log_entry> entry(log_store_->entry_at(i));
                if (entry->get_val_type() == log_val_type::conf) {
                    config_ = cluster_config::deserialize(entry->get_buf());
                    break;
                }
            }

            std::list<ptr<srv_config>>& srvs(config_->get_servers());
            for (cluster_config::srv_itor it = srvs.begin(); it != srvs.end(); ++it) {
                if ((*it)->get_id() != id_) {
         	        timer_task<peer&>::executor exec = (timer_task<peer&>::executor)std::bind(&raft_server::handle_hb_timeout, this, std::placeholders::_1);
                    peers_.insert(std::make_pair((*it)->get_id(), cs_new<peer, srv_config&, context&, timer_task<peer&>::executor&>(**it, *ctx_, exec)));
                }
            }

            quick_commit_idx_ = state_->get_commit_idx();
            std::thread commiting_thread = std::thread(std::bind(&raft_server::commit_in_bg, this));
            commiting_thread.detach();
            restart_election_timer();
            l_.debug(strfmt<30>("server %d started").fmt(id_));
        }

        virtual ~raft_server() {
            recur_lock(lock_);
            stopping_ = true;
            std::unique_lock<std::mutex> commit_lock(commit_lock_);
            commit_cv_.notify_all();
            std::unique_lock<std::mutex> lock(stopping_lock_);
            commit_lock.unlock();
            commit_lock.release();
            ready_to_stop_cv_.wait(lock);
            if (election_task_) {
                scheduler_.cancel(election_task_);
            }

            for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
                if (it->second->get_hb_task()) {
                    scheduler_.cancel(it->second->get_hb_task());
                }
            }
        }

    __nocopy__(raft_server)
    
    public:
        virtual ptr<resp_msg> process_req(req_msg& req) __override__;

    private:
        typedef std::unordered_map<int32, ptr<peer>>::const_iterator peer_itor;

    private:
        ptr<resp_msg> handle_append_entries(req_msg& req);
        ptr<resp_msg> handle_vote_req(req_msg& req);
        ptr<resp_msg> handle_cli_req(req_msg& req);
        ptr<resp_msg> handle_extended_msg(req_msg& req);
        ptr<resp_msg> handle_install_snapshot_req(req_msg& req);
        ptr<resp_msg> handle_rm_srv_req(req_msg& req);
        ptr<resp_msg> handle_add_srv_req(req_msg& req);
        ptr<resp_msg> handle_log_sync_req(req_msg& req);
        ptr<resp_msg> handle_join_cluster_req(req_msg& req);
        ptr<resp_msg> handle_leave_cluster_req(req_msg& req);
        bool handle_snapshot_sync_req(snapshot_sync_req& req);
        void request_vote();
        void request_append_entries();
        bool request_append_entries(peer& p);
        void handle_peer_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err);
        void handle_append_entries_resp(resp_msg& resp);
        void handle_install_snapshot_resp(resp_msg& resp);
        void handle_voting_resp(resp_msg& resp);
        void handle_ext_resp(ptr<resp_msg>& resp, ptr<rpc_exception>& err);
        void handle_ext_resp_err(rpc_exception& err);
        ptr<req_msg> create_append_entries_req(peer& p);
        ptr<req_msg> create_sync_snapshot_req(peer& p, ulong last_log_idx, ulong term, ulong commit_idx);
        void commit(ulong target_idx);
        void snapshot_and_compact(ulong committed_idx);
        bool update_term(ulong term);
        void reconfigure(const ptr<cluster_config>& new_config);
        void become_leader();
        void become_follower();
        void enable_hb_for_peer(peer& p);
        void restart_election_timer();
        void stop_election_timer();
        void handle_hb_timeout(peer& peer);
        void handle_election_timeout();
        void sync_log_to_new_srv(ulong start_idx);
        void invite_srv_to_join_cluster();
        void rm_srv_from_cluster(int32 srv_id);
        int get_snapshot_sync_block_size() const;
        void on_snapshot_completed(ptr<snapshot>& s, bool result, ptr<std::exception>& err);
        void on_retryable_req_err(ptr<peer>& p, ptr<req_msg>& req);
        ulong term_for_log(ulong log_idx);
        void commit_in_bg();
    private:
        static const int default_snapshot_sync_block_size;
        int32 leader_;
        int32 id_;
        int32 votes_responded_;
        int32 votes_granted_;
        ulong quick_commit_idx_;
        bool election_completed_;
        bool config_changing_;
        bool catching_up_;
        bool stopping_;
        int32 steps_to_down_;
        std::atomic_bool snp_in_progress_;
        std::unique_ptr<context> ctx_;
        delayed_task_scheduler& scheduler_;
        timer_task<void>::executor election_exec_;
        ptr<delayed_task> election_task_;
        std::unordered_map<int32, ptr<peer>> peers_;
        srv_role role_;
        ptr<srv_state> state_;
        ptr<log_store> log_store_;
        state_machine& state_machine_;
        logger& l_;
        std::function<int32()> rand_timeout_;
        ptr<cluster_config> config_;
        ptr<peer> srv_to_join_;
        ptr<srv_config> conf_to_add_;
        std::recursive_mutex lock_;
        std::mutex commit_lock_;
        std::condition_variable commit_cv_;
        std::mutex stopping_lock_;
        std::condition_variable ready_to_stop_cv_;
        rpc_handler resp_handler_;
        rpc_handler ex_resp_handler_;
    };
}

#endif //_RAFT_SERVER_HXX_
