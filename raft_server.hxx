#ifndef _RAFT_SERVER_HXX_
#define _RAFT_SERVER_HXX_

namespace cornerstone {
    class raft_server {
    public:
        raft_server(context* ctx)
        : leader_(-1),
        id_(ctx->state_mgr_->server_id()),
        votes_responded_(0),
        votes_granted_(0),
        election_completed_(true),
        config_changing_(false),
        catching_up_(false),
        steps_to_down_(0),
        snp_in_progress_(),
        ctx_(ctx),
        scheduler_(*ctx->scheduler_),
        election_task_(nilptr),
        peers_(),
        role_(srv_role::follower),
        state_(ctx->state_mgr_->read_state()),
        log_store_(ctx->state_mgr_->load_log_store()),
        state_machine_(*ctx->state_machine_),
        l_(*ctx->logger_),
        config_(ctx->state_mgr_->load_config()),
        srv_to_join_(nilptr),
        lock_() {
            unsigned int seed = (unsigned int)std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine engine(seed);
            std::uniform_int_distribution<int> distribution(ctx->params_->election_timeout_lower_bound_, ctx->params_->election_timeout_upper_bound_);
            rand_timeout_ = std::bind(distribution, engine);
            if (state_ == nilptr) {
                state_ = new srv_state();
                state_->set_commit_idx(0);
                state_->set_term(0);
                state_->set_voted_for(-1);
            }

            for (ulong i = std::max(this->state_->get_commit_idx() + 1, log_store_->start_index()); i < log_store_->next_slot(); ++i) {
                std::unique_ptr<log_entry> entry(log_store_->entry_at(i));
                if (entry->get_val_type() == log_val_type::conf) {
                    config_ = cluster_config::deserialize(&entry->get_buf());
                    break;
                }
            }

            for (std::list<srv_config*>::const_iterator it = config_->get_servers().begin(); it != config_->get_servers().end(); ++it) {
                if ((*it)->get_id() != id_) {
                    peers_.insert(std::make_pair((*it)->get_id(), new peer(**it, *ctx_, std::bind(&raft_server::handle_hb_timeout, this, std::placeholders::_1))));
                }
            }

            election_task_ = new timer_task<void>(std::bind(&raft_server::handle_election_timeout, this));
            restart_election_timer();
            //
        }

        ~raft_server() {
            delete ctx_;
            delete election_task_;
            delete state_;
            delete log_store_;
            delete config_;
            for (std::unordered_map<int, peer*>::const_iterator it = peers_.begin(); it != peers_.end(); ++it) {
                delete it->second;
            }

            if (srv_to_join_ != nilptr) {
                delete srv_to_join_;
            }
        }

    __nocopy__(raft_server)
    
    public:
        //

    private:
        void restart_election_timer();
        void handle_hb_timeout(peer& peer);
        void handle_election_timeout();
    private:
        int leader_;
        int id_;
        int votes_responded_;
        int votes_granted_;
        bool election_completed_;
        bool config_changing_;
        bool catching_up_;
        int steps_to_down_;
        std::atomic_bool snp_in_progress_;
        context* ctx_;
        delayed_task_scheduler& scheduler_;
        timer_task<void>* election_task_;
        std::unordered_map<int, peer*> peers_;
        srv_role role_;
        srv_state* state_;
        log_store* log_store_;
        state_machine& state_machine_;
        logger& l_;
        std::function<int()> rand_timeout_;
        cluster_config* config_;
        peer* srv_to_join_;
        std::mutex lock_;
    };
}

#endif //_RAFT_SERVER_HXX_