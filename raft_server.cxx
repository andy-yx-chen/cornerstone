#include "cornerstone.hxx"

using namespace cornerstone;

const int raft_server::default_snapshot_sync_block_size = 4 * 1024;

resp_msg* raft_server::process_req(req_msg& req) {
    std::lock_guard<std::mutex> guard(lock_);
    l_.debug(
        lstrfmt("Receive a %d message from %d with LastLogIndex=%llu, LastLogTerm=%llu, EntriesLength=%d, CommitIndex=%llu and Term=%llu")
        .fmt(
            req.get_type(),
            req.get_src(),
            req.get_last_log_idx(),
            req.get_last_log_term(),
            req.log_entries().size(),
            req.get_commit_idx(),
            req.get_term()));
    if (req.get_type() == msg_type::append_entries_request ||
        req.get_type() == msg_type::request_vote_request ||
        req.get_type() == msg_type::install_snapshot_request) {
        // we allow the server to be continue after term updated to save a round message
        update_term(req.get_term());

        // Reset stepping down value to prevent this server goes down when leader crashes after sending a LeaveClusterRequest
        if (steps_to_down_ > 0) {
            steps_to_down_ = 2;
        }
    }

    resp_msg* resp = nilptr;
    if (req.get_type() == msg_type::append_entries_request) {
        resp = handle_append_entries(req);
    }
    else if (req.get_type() == msg_type::request_vote_request) {
        resp = handle_vote_req(req);
    }
    else if (req.get_type() == msg_type::client_request) {
        resp = handle_cli_req(req);
    }
    else {
        // extended requests
        resp = handle_extended_msg(req);
    }

    if (resp != nilptr) {
        l_.debug(
            lstrfmt("Response back a %d message to %d with Accepted=%d, Term=%llu, NextIndex=%llu")
            .fmt(
                resp->get_type(),
                resp->get_dst(),
                resp->get_accepted() ? 1 : 0,
                resp->get_term(),
                resp->get_next_idx()));
    }

    return resp;
}

resp_msg* raft_server::handle_append_entries(req_msg& req) {
    if (req.get_term() == state_->get_term()) {
        if (role_ == srv_role::candidate) {
            become_follower();
        }
        else if (role_ == srv_role::leader) {
            l_.debug(
                lstrfmt("Receive AppendEntriesRequest from another leader(%d) with same term, there must be a bug, server exits")
                .fmt(req.get_src()));
            ::exit(-1);
        }
        else {
            restart_election_timer();
        }
    }

    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::append_entries_response, id_, req.get_src());
    bool log_okay = req.get_last_log_idx() == 0 ||
        (req.get_last_log_idx() < log_store_->next_slot() && req.get_last_log_term() == std::unique_ptr<log_entry>(log_store_->entry_at(req.get_last_log_idx()))->get_term());
    if (req.get_term() < state_->get_term() || !log_okay) {
        return resp;
    }

    // follower & log is okay
    if (req.log_entries().size() > 0) {
        // write logs to store, start from overlapped logs
        ulong idx = req.get_last_log_idx() + 1;
        size_t log_idx = 0;
        while (idx < log_store_->next_slot() && log_idx < req.log_entries().size()) {
            std::unique_ptr<log_entry> entry(log_store_->entry_at(idx));
            if (entry->get_term() == req.log_entries().at(log_idx)->get_term()) {
                idx++;
                log_idx++;
            }
            else {
                break;
            }
        }

        // dealing with overwrites
        while (idx < log_store_->next_slot() && log_idx < req.log_entries().size()) {
            if (idx <= config_->get_log_idx()) {
                // we need to restore the configuration from previous committed configuration
                reconfigure(std::shared_ptr<cluster_config>(ctx_->state_mgr_->load_config()));
            }

            std::unique_ptr<log_entry> old_entry(log_store_->entry_at(idx));
            if (old_entry->get_val_type() == log_val_type::app_log) {
                state_machine_.rollback(idx, old_entry->get_buf());
            }

            log_store_->write_at(idx++, *(req.log_entries().at(log_idx++)));
        }

        // append new log entries
        while (log_idx < req.log_entries().size()) {
            log_entry* entry = req.log_entries().at(log_idx);
            log_store_->append(*entry);
            if (entry->get_val_type() == log_val_type::conf) {
                reconfigure(std::shared_ptr<cluster_config>(cluster_config::deserialize(entry->get_buf())));
            }
            else {
                state_machine_.pre_commit(log_store_->next_slot() - 1, entry->get_buf());
            }
        }
    }

    leader_ = req.get_src();
    commit(req.get_commit_idx());
    resp->accept(req.get_last_log_idx() + req.log_entries().size() + 1);
    return resp;
}

resp_msg* raft_server::handle_vote_req(req_msg& req) {
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::request_vote_response, id_, req.get_src());
    bool log_okay = req.get_last_log_term() > log_store_->last_entry().get_term() ||
        (req.get_last_log_term() == log_store_->last_entry().get_term() &&
            log_store_->next_slot() - 1 <= req.get_last_log_idx());
    bool grant = req.get_term() == state_->get_term() && log_okay && (state_->get_voted_for() == req.get_src() || state_->get_voted_for() == -1);
    if (grant) {
        resp->accept(log_store_->next_slot());
        state_->set_voted_for(req.get_src());
        ctx_->state_mgr_->save_state(*state_);
    }

    return resp;
}

resp_msg* raft_server::handle_cli_req(req_msg& req) {
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::append_entries_response, id_, leader_);
    if (role_ != srv_role::leader) {
        return resp;
    }

    std::vector<log_entry*>& entries = req.log_entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        log_store_->append(*entries.at(i));
        state_machine_.pre_commit(log_store_->next_slot() - 1, entries.at(i)->get_buf());
    }

    // urgent commit, so that the commit will not depend on hb
    request_append_entries();
    resp->accept(log_store_->next_slot());
    return resp;
}

void raft_server::handle_election_timeout() {
    std::lock_guard<std::mutex> lock_guard(lock_);
    if (steps_to_down_ > 0) {
        if (--steps_to_down_ == 0) {
            l_.info("no hearing further news from leader, step down");
            ::exit(0);
            return;
        }

        l_.info(sstrfmt("stepping down (cycles left: %d), skip this election timeout event").fmt(steps_to_down_));
        restart_election_timer();
        return;
    }

    if (catching_up_) {
        // this is a new server for the cluster, will not send out vote req until conf that includes this srv is committed
        l_.info("election timeout while joining the cluster, ignore it.");
        restart_election_timer();
        return;
    }

    if (role_ == srv_role::leader) {
        l_.err("A leader should never encounter election timeout, illegal application state, stop the application");
        ::exit(-1);
        return;
    }

    l_.debug("Election timeout, change to Candidate");
    state_->inc_term();
    state_->set_voted_for(-1);
    role_ = srv_role::candidate;
    votes_granted_ = 0;
    votes_responded_ = 0;
    election_completed_ = false;
    ctx_->state_mgr_->save_state(*state_);
    request_vote();

    // restart the election timer if this is not yet a leader
    if (role_ != srv_role::leader) {
        restart_election_timer();
    }
}

void raft_server::request_vote() {
    l_.info(sstrfmt("requestVote started with term %llu").fmt(state_->get_term()));
    state_->set_voted_for(id_);
    ctx_->state_mgr_->save_state(*state_);
    votes_granted_ += 1;
    votes_responded_ += 1;

    // is this the only server?
    if (votes_granted_ > (int32)(peers_.size() + 1) / 2) {
        election_completed_ = true;
        become_leader();
        return;
    }

    log_entry& entry = log_store_->last_entry();
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        req_msg* req = new req_msg(state_->get_term(), msg_type::request_vote_request, id_, it->second->get_id(), log_store_->last_entry().get_term(), log_store_->next_slot() - 1, state_->get_commit_idx());
        l_.debug(sstrfmt("send %d to server %d with term %llu").fmt(req->get_type(), it->second->get_id(), state_->get_term()));
        it->second->send_req(req, resp_handler_);
    }
}

void raft_server::request_append_entries() {
    if (peers_.size() == 0) {
        commit(log_store_->next_slot() - 1);
        return;
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        request_append_entries(*it->second);
    }
}

bool raft_server::request_append_entries(peer& p) {
    if (p.make_busy()) {
        p.send_req(create_append_entries_req(p), resp_handler_);
        return true;
    }

    l_.debug(sstrfmt("Server %d is busy, skip the request").fmt(p.get_id()));
    return false;
}

void raft_server::handle_peer_resp(resp_msg* resp_p, rpc_exception* err_p) {
    std::lock_guard<std::mutex> guard(lock_);
    std::unique_ptr<resp_msg> resp(resp_p);
    std::unique_ptr<rpc_exception> err(err_p);
    if (err) {
        l_.info(sstrfmt("peer response error: %s").fmt(err->what()));
        delete err->req(); // req msg is not used
        return;
    }

    l_.debug(
        lstrfmt("Receive a %d message from peer %d with Result=%d, Term=%llu, NextIndex=%llu")
        .fmt(resp->get_type(), resp->get_src(), resp->get_accepted() ? 1 : 0, resp->get_term(), resp->get_next_idx()));

    // if term is updated, no more action is required
    if (update_term(resp->get_term())) {
        return;
    }

    // ignore the response that with lower term for safety
    switch (resp->get_type())
    {
    case msg_type::request_vote_response:
        handle_voting_resp(*resp);
        break;
    case msg_type::append_entries_response:
        handle_append_entries_resp(*resp);
        break;
    case msg_type::install_snapshot_response:
        handle_install_snapshot_resp(*resp);
        break;
    default:
        l_.err(sstrfmt("Received an unexpected message %d for response, system exits.").fmt(resp->get_type()));
        ::exit(-1);
        break;
    }
}

void raft_server::handle_append_entries_resp(resp_msg& resp) {
    peer_itor it = peers_.find(resp.get_src());
    if (it == peers_.end()) {
        l_.info(sstrfmt("the response is from an unkonw peer %d").fmt(resp.get_src()));
        return;
    }

    // if there are pending logs to be synced or commit index need to be advanced, continue to send appendEntries to this peer
    bool need_to_catchup = true;
    peer* p = it->second;
    if (resp.get_accepted()) {
        {
            std::lock_guard<std::mutex>(p->get_lock());
            p->set_next_log_idx(resp.get_next_idx());
            p->set_matched_idx(resp.get_next_idx() - 1);
        }

        // try to commit with this response
        std::unique_ptr<ulong> matched_indexes(new ulong[peers_.size() + 1]);
        matched_indexes.get()[0] = log_store_->next_slot() - 1;
        int i = 1;
        for (it = peers_.begin(); it != peers_.end(); ++it, ++i) {
            matched_indexes.get()[i] = it->second->get_matched_idx();
        }

        std::sort(matched_indexes.get(), matched_indexes.get() + (peers_.size() + 1), std::greater<ulong>());
        commit(matched_indexes.get()[(peers_.size() + 1) / 2]);
        need_to_catchup = p->clear_pending_commit() || resp.get_next_idx() < log_store_->next_slot();
    }
    else {
        std::lock_guard<std::mutex> guard(p->get_lock());
        p->set_next_log_idx(p->get_next_log_idx() - 1);
    }

    p->set_free();

    // This may not be a leader anymore, such as the response was sent out long time ago
    // and the role was updated by UpdateTerm call
    // Try to match up the logs for this peer
    if (role_ == srv_role::leader && need_to_catchup) {
        request_append_entries(*p);
    }
}

void raft_server::handle_install_snapshot_resp(resp_msg& resp) {
    peer_itor it = peers_.find(resp.get_src());
    if (it == peers_.end()) {
        l_.info(sstrfmt("the response is from an unkonw peer %d").fmt(resp.get_src()));
        return;
    }

    // if there are pending logs to be synced or commit index need to be advanced, continue to send appendEntries to this peer
    bool need_to_catchup = true;
    peer* p = it->second;
    if (resp.get_accepted()) {
        std::lock_guard<std::mutex> guard(p->get_lock());
        snapshot_sync_ctx* sync_ctx = p->get_snapshot_sync_ctx();
        if (sync_ctx == nilptr) {
            l_.info("no snapshot sync context for this peer, drop the response");
            need_to_catchup = false;
        }
        else {
            if (resp.get_next_idx() >= sync_ctx->get_snapshot().size()) {
                l_.debug("snapshot sync is done");
                p->set_next_log_idx(sync_ctx->get_snapshot().get_last_log_idx() + 1);
                p->set_matched_idx(sync_ctx->get_snapshot().get_last_log_idx());
                p->set_snapshot_in_sync(nilptr);
                need_to_catchup = p->clear_pending_commit() || resp.get_next_idx() < log_store_->next_slot();
            }
            else {
                l_.debug(sstrfmt("continue to sync snapshot at offset %llu").fmt(resp.get_next_idx()));
                sync_ctx->set_offset(resp.get_next_idx());
            }
        }
    }
    else {
        l_.info("peer declines to install the snapshot, will retry");
    }

    p->set_free();
    // This may not be a leader anymore, such as the response was sent out long time ago
    // and the role was updated by UpdateTerm call
    // Try to match up the logs for this peer
    if (role_ == srv_role::leader && need_to_catchup) {
        request_append_entries(*p);
    }
}

void raft_server::handle_voting_resp(resp_msg& resp) {
    votes_responded_ += 1;
    if (election_completed_) {
        l_.info("Election completed, will ignore the voting result from this server");
        return;
    }

    if (resp.get_accepted()) {
        votes_granted_ += 1;
    }

    if (votes_responded_ >= (int32)(peers_.size() + 1)) {
        election_completed_ = true;
    }

    if (votes_granted_ > (int32)((peers_.size() + 1) / 2)) {
        l_.info(sstrfmt("Server is elected as leader for term %llu").fmt(state_->get_term()));
        election_completed_ = true;
        become_leader();
    }
}

void raft_server::handle_hb_timeout(peer& p) {
    std::lock_guard<std::mutex> guard(lock_);
    l_.debug(sstrfmt("Heartbeat timeout for %d").fmt(p.get_id()));
    if (role_ == srv_role::leader) {
        request_append_entries(p);
        {
            std::lock_guard<std::mutex> guard(p.get_lock());
            if (p.is_hb_enabled()) {
                // Schedule another heartbeat if heartbeat is still enabled 
                ctx_->scheduler_->schedule(p.get_hb_task(), p.get_current_hb_interval());
            }
            else {
                l_.debug(sstrfmt("heartbeat is disabled for peer %d").fmt(p.get_id()));
            }
        }
    }
    else {
        l_.info(sstrfmt("Receive a heartbeat event for %d while no longer as a leader").fmt(p.get_id()));
    }
}

void raft_server::restart_election_timer() {
    // don't start the election timer while this server is still catching up the logs
    if (catching_up_) {
        return;
    }

    if (election_task_) {
        election_task_->cancel();
    }

    election_task_.reset(new timer_task<void>(election_exec_));
    scheduler_.schedule(election_task_, rand_timeout_());
}

void raft_server::stop_election_timer() {
    if (!election_task_) {
        l_.warn("Election Timer is never started but is requested to stop, protential a bug");
        return;
    }

    election_task_->cancel();
    election_task_.reset();
}

void raft_server::become_leader() {
    stop_election_timer();
    role_ = srv_role::leader;
    leader_ = id_;
    srv_to_join_.reset();
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        it->second->set_next_log_idx(log_store_->next_slot());
        it->second->set_snapshot_in_sync(nilptr);
        enable_hb_for_peer(*(it->second));
    }

    request_append_entries();
}

void raft_server::enable_hb_for_peer(peer& p) {
    p.enable_hb(true);
    p.resume_hb_speed();
    scheduler_.schedule(p.get_hb_task(), p.get_current_hb_interval());
}

void raft_server::become_follower() {
    // stop hb for all peers
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        it->second->enable_hb(false);
    }

    srv_to_join_.reset();
    role_ = srv_role::follower;
    restart_election_timer();
}

bool raft_server::update_term(ulong term) {
    if (term > state_->get_term()) {
        state_->set_term(term);
        state_->set_voted_for(-1);
        election_completed_ = false;
        votes_granted_ = 0;
        votes_responded_ = 0;
        ctx_->state_mgr_->save_state(*state_);
        become_follower();
        return true;
    }

    return false;
}

void raft_server::commit(ulong target_idx) {
    if (target_idx > state_->get_commit_idx()) {
        bool snapshot_in_action = false;
        try {
            while (state_->get_commit_idx() < target_idx && state_->get_commit_idx() < (log_store_->next_slot() - 1)) {
                ulong idx_to_commit = state_->get_commit_idx() + 1;
                std::unique_ptr<log_entry> entry(log_store_->entry_at(idx_to_commit));
                if (entry->get_val_type() == log_val_type::app_log) {
                    state_machine_.commit(idx_to_commit, entry->get_buf());
                }
                else if (entry->get_val_type() == log_val_type::conf) {
                    ctx_->state_mgr_->save_config(*config_);
                    if (catching_up_ && config_->get_server(id_) != nilptr) {
                        l_.info("this server is committed as one of cluster members");
                        catching_up_ = false;
                    }
                }

                state_->set_commit_idx(target_idx);

                // see if we need to do snapshot
                bool f = false;
                if (ctx_->params_->snapshot_distance_ > 0
                    && (idx_to_commit - log_store_->start_index()) > ctx_->params_->snapshot_distance_
                    && snp_in_progress_.compare_exchange_strong(f, true)) {
                    snapshot_in_action = true;
                    std::unique_ptr<snapshot> snp(state_machine_.last_snapshot());
                    if (snp && (idx_to_commit - snp->get_last_log_idx()) < ctx_->params_->snapshot_distance_) {
                        l_.info(sstrfmt("a very recent snapshot is available at index %llu, will skip this one").fmt(snp->get_last_log_idx()));
                        snp_in_progress_.store(false);
                        snapshot_in_action = false;
                    }
                    else {
                        l_.info(sstrfmt("creating a snapshot for index %llu").fmt(idx_to_commit));

                        // get the latest configuration info
                        std::shared_ptr<cluster_config> conf(config_);
                        while (conf->get_log_idx() > idx_to_commit && conf->get_prev_log_idx() >= log_store_->start_index()) {
                            std::unique_ptr<log_entry> conf_log(log_store_->entry_at(conf->get_prev_log_idx()));
                            conf.reset(cluster_config::deserialize(conf_log->get_buf()));
                        }

                        if (conf->get_log_idx() > idx_to_commit &&
                            conf->get_prev_log_idx() > 0 &&
                            conf->get_prev_log_idx() < log_store_->start_index()) {
                            std::unique_ptr<snapshot> s(state_machine_.last_snapshot());
                            if (!s) {
                                l_.err("No snapshot could be found while no configuration cannot be found in current committed logs, this is a system error, exiting");
                                ::exit(-1);
                                return;
                            }

                            conf = s->get_last_config();
                        }
                        else if (conf->get_log_idx() > idx_to_commit && conf->get_prev_log_idx() == 0) {
                            l_.err("BUG!!! stop the system, there must be a configuration at index one");
                            ::exit(-1);
                            return;
                        }

                        ulong idx_to_compact = idx_to_commit - 1;
                        std::unique_ptr<log_entry> last_log_entry(log_store_->entry_at(idx_to_commit));
                        ulong log_term_to_compact = last_log_entry->get_term();
                        std::shared_ptr<snapshot> new_snapshot(new snapshot(idx_to_commit, log_term_to_compact, conf));
                        state_machine_.create_snapshot(
                            *new_snapshot, 
                            (async_result<bool>::handler_type)(std::bind(&raft_server::on_snapshot_completed, this, new_snapshot, std::placeholders::_1, std::placeholders::_2)));
                        snapshot_in_action = false;
                    }
                }
            }
        }
        catch (...) {
            l_.err(sstrfmt("failed to commit to index %llu due to errors").fmt(target_idx));
            if (snapshot_in_action) {
                bool t = true;
                snp_in_progress_.compare_exchange_strong(t, false);
            }
        }

        // save the commitment state
        ctx_->state_mgr_->save_state(*state_);

        // if this is a leader notify peers to commit as well
        // for peers that are free, send the request, otherwise, set pending commit flag for that peer
        if (role_ == srv_role::leader) {
            for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
                if (!request_append_entries(*(it->second))) {
                    it->second->set_pending_commit();
                }
            }
        }
    }
}

void raft_server::on_snapshot_completed(std::shared_ptr<snapshot> s, bool result, std::exception* err) {
    do {
        if (err != nilptr) {
            l_.err(lstrfmt("failed to create a snapshot due to %s").fmt(err->what()));
            break;
        }

        if (!result) {
            l_.info("the state machine rejects to create the snapshot");
            break;
        }

        {
            std::lock_guard<std::mutex> guard(lock_);
            l_.debug("snapshot created, compact the log store");
            log_store_->compact(s->get_last_log_idx());
        }
    } while (false);
    snp_in_progress_.store(false);
}

req_msg* raft_server::create_append_entries_req(peer& p) {
    ulong cur_nxt_idx(0L);
    ulong commit_idx(0L);
    ulong last_log_idx(0L);
    ulong term(0L);
    ulong starting_idx(1L);

    {
        std::lock_guard<std::mutex> guard(lock_);
        starting_idx = log_store_->start_index();
        cur_nxt_idx = log_store_->next_slot();
        commit_idx = state_->get_commit_idx();
        term = state_->get_term();
    }

    {
        std::lock_guard<std::mutex> guard(p.get_lock());
        if (p.get_next_log_idx() == 0L) {
            p.set_next_log_idx(cur_nxt_idx);
        }

        last_log_idx = p.get_next_log_idx() - 1;
    }

    if (last_log_idx >= cur_nxt_idx) {
        l_.err(sstrfmt("Peer's lastLogIndex is too large %llu v.s. %llu, server exits").fmt(last_log_idx, cur_nxt_idx));
        ::exit(-1);
        return nilptr;
    }

    // for syncing the snapshots
    if (last_log_idx > 0 && last_log_idx < starting_idx) {
        return create_sync_snapshot_req(p, last_log_idx, term, commit_idx);
    }

    std::unique_ptr<log_entry> last_entry(log_store_->entry_at(last_log_idx));
    std::unique_ptr<std::vector<log_entry*>> log_entries((last_log_idx + 1) >= cur_nxt_idx ? nilptr : log_store_->log_entries(last_log_idx + 1, cur_nxt_idx));
    l_.debug(
        lstrfmt("An AppendEntries Request for %d with LastLogIndex=%llu, LastLogTerm=%llu, EntriesLength=%d, CommitIndex=%llu and Term=%llu")
        .fmt(
            p.get_id(),
            last_log_idx,
            last_entry->get_term(),
            log_entries->size(),
            commit_idx,
            term));
    req_msg* req = new req_msg(term, msg_type::append_entries_request, id_, p.get_id(), last_entry->get_term(), last_log_idx, commit_idx);
    std::vector<log_entry*>& v = req->log_entries();
    v.insert(v.end(), log_entries->begin(), log_entries->end());
    return req;
}

void raft_server::reconfigure(const std::shared_ptr<cluster_config>& new_config) {
    // according to our design, the new configuration never send to a server that is removed
    // so, in this method, we are not considering self get removed scenario
    l_.debug(
        lstrfmt("system is reconfigured to have %d servers, last config index: %llu, this config index: %llu")
        .fmt(new_config->get_servers().size(), new_config->get_prev_log_idx(), new_config->get_log_idx()));

    // we only allow one server to be added or removed at a time
    int32 srv_removed = -1;
    srv_config* srv_added = nilptr;
    std::list<srv_config*>& new_srvs(new_config->get_servers());
    for (std::list<srv_config*>::const_iterator it = new_srvs.begin(); it != new_srvs.end(); ++it) {
        peer_itor pit = peers_.find((*it)->get_id());
        if (pit == peers_.end()) {
            srv_added = *it;
            break;
        }
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        if (new_config->get_server(it->first) == nilptr) {
            srv_removed = it->first;
            break;
        }
    }

    if (srv_added != nilptr && srv_added->get_id() != id_) {
        peer* p = new peer(*srv_added, *ctx_, (timer_task<peer&>::executor)std::bind(&raft_server::handle_hb_timeout, this, std::placeholders::_1));
        peers_.insert(std::make_pair(srv_added->get_id(), p));
        l_.info(sstrfmt("server %d is added to cluster").fmt(srv_added->get_id()));
        if (role_ == srv_role::leader) {
            l_.info(sstrfmt("enable heartbeating for server %d").fmt(srv_added->get_id()));
            enable_hb_for_peer(*p);
        }
    }

    if (srv_removed >= 0) {
        peer_itor pit = peers_.find(srv_removed);
        if (pit != peers_.end()) {
            std::unique_ptr<peer> p(pit->second);
            p->enable_hb(false);
            peers_.erase(pit);
        }
        else {
            l_.info(sstrfmt("peer %d cannot be found, no action for removing").fmt(srv_removed));
        }
    }

    config_ = new_config;
}

resp_msg* raft_server::handle_extended_msg(req_msg& req) {
    switch (req.get_type())
    {
    case msg_type::add_server_request:
        return handle_add_srv_req(req);
    case msg_type::remove_server_request:
        return handle_rm_srv_req(req);
    case msg_type::sync_log_request:
        return handle_log_sync_req(req);
    case msg_type::join_cluster_request:
        return handle_join_cluster_req(req);
    case msg_type::leave_cluster_request:
        return handle_leave_cluster_req(req);
    case msg_type::install_snapshot_request:
        return handle_install_snapshot_req(req);
    default:
        l_.err(sstrfmt("receive an unknown request %d, for safety, step down.").fmt(req.get_type()));
        ::exit(-1);
        break;
    }

    return nilptr;
}

resp_msg* raft_server::handle_install_snapshot_req(req_msg& req) {
    if (req.get_term() == state_->get_term() && !catching_up_) {
        if (role_ == srv_role::candidate) {
            become_follower();
        }
        else if (role_ == srv_role::leader) {
            l_.err(lstrfmt("Receive InstallSnapshotRequest from another leader(%d) with same term, there must be a bug, server exits").fmt(req.get_src()));
            ::exit(-1);
            return nilptr;
        }
        else {
            restart_election_timer();
        }
    }

    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::install_snapshot_response, id_, req.get_src());
    if (!catching_up_ && req.get_term() < state_->get_term()) {
        l_.info("received an install snapshot request which has lower term than this server, decline the request");
        return resp;
    }

    std::vector<log_entry*>& entries(req.log_entries());
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::snp_sync_req) {
        l_.warn("Receive an invalid InstallSnapshotRequest due to bad log entries or bad log entry value");
        return resp;
    }

    std::unique_ptr<snapshot_sync_req> sync_req(snapshot_sync_req::deserialize(entries[0]->get_buf()));
    if (handle_snapshot_sync_req(*sync_req)) {
        resp->accept(sync_req->get_offset() + sync_req->get_data().size());
    }
    
    return resp;
}

bool raft_server::handle_snapshot_sync_req(snapshot_sync_req& req) {
    try {
        state_machine_.save_snapshot_data(req.get_snapshot(), req.get_offset(), req.get_data());
        if (req.is_done()) {
            l_.debug("sucessfully receive a snapshot from leader");
            if (log_store_->compact(req.get_snapshot().get_last_log_idx())) {
                l_.info("successfully compact the log store, will now ask the statemachine to apply the snapshot");
                if (!state_machine_.apply_snapshot(req.get_snapshot())) {
                    l_.info("failed to apply the snapshot after log compacted, to ensure the safety, will shutdown the system");
                    ::exit(-1);
                    return false;
                }

                reconfigure(req.get_snapshot().get_last_config());
                ctx_->state_mgr_->save_config(*config_);
                state_->set_commit_idx(req.get_snapshot().get_last_log_idx());
                ctx_->state_mgr_->save_state(*state_);
                l_.info("snapshot is successfully applied");
            }
            else {
                l_.err("failed to compact the log store after a snapshot is received, will ask the leader to retry");
                return false;
            }
        }
    }
    catch (...) {
        l_.err("failed to handle snapshot installation due to system errors");
        ::exit(-1);
        return false;
    }

    return true;
}

void raft_server::handle_ext_resp(resp_msg* presp, rpc_exception* perr) {
    std::lock_guard<std::mutex> guard(lock_);
    std::unique_ptr<resp_msg> resp(presp);
    std::unique_ptr<rpc_exception> err(perr);
    if (err) {
        handle_ext_resp_err(*err);
        return;
    }

    l_.debug(
        lstrfmt("Receive an extended %d message from peer %d with Result=%d, Term=%llu, NextIndex=%llu")
        .fmt(
            resp->get_type(),
            resp->get_src(),
            resp->get_accepted() ? 1 : 0,
            resp->get_term(),
            resp->get_next_idx()));

    switch (resp->get_type())
    {
    case msg_type::sync_log_response:
        if (srv_to_join_) {
            // we are reusing heartbeat interval value to indicate when to stop retry
            srv_to_join_->resume_hb_speed();
            srv_to_join_->set_next_log_idx(resp->get_next_idx());
            srv_to_join_->set_matched_idx(resp->get_next_idx() - 1);
            sync_log_to_new_srv(resp->get_next_idx()); 
        }
        break;
    case msg_type::join_cluster_response:
        if (srv_to_join_) {
            if (resp->get_accepted()) {
                l_.debug("new server confirms it will join, start syncing logs to it");
                sync_log_to_new_srv(1);
            }
            else {
                l_.debug("new server cannot accept the invitation, give up");
            }
        }
        else {
            l_.debug("no server to join, drop the message");
        }
        break;
    case msg_type::leave_cluster_response:
        if (!resp->get_accepted()) {
            l_.debug("peer doesn't accept to stepping down, stop proceeding");
            return;
        }

        l_.debug("peer accepted to stepping down, removing this server from cluster");
        rm_srv_from_cluster(resp->get_src());
        break;
    case msg_type::install_snapshot_response:
        {
            if (!srv_to_join_) {
                l_.info("no server to join, the response must be very old.");
                return;
            }

            if (!resp->get_accepted()) {
                l_.info("peer doesn't accept the snapshot installation request");
                return;
            }

            snapshot_sync_ctx* sync_ctx = srv_to_join_->get_snapshot_sync_ctx();
            if (sync_ctx == nilptr) {
                l_.err("Bug! SnapshotSyncContext must not be null");
                ::exit(-1);
                return;
            }

            if (resp->get_next_idx() >= sync_ctx->get_snapshot().size()) {
                // snapshot is done
                l_.debug("snapshot has been copied and applied to new server, continue to sync logs after snapshot");
                srv_to_join_->set_snapshot_in_sync(nilptr);
                srv_to_join_->set_next_log_idx(sync_ctx->get_snapshot().get_last_log_idx() + 1);
                srv_to_join_->set_matched_idx(sync_ctx->get_snapshot().get_last_log_idx());
            }
            else {
                sync_ctx->set_offset(resp->get_next_idx());
                l_.debug(sstrfmt("continue to send snapshot to new server at offset %llu").fmt(resp->get_next_idx()));
            }

            sync_log_to_new_srv(srv_to_join_->get_next_log_idx());
        }
        break;
    default:
        l_.err(lstrfmt("received an unexpected response message type %d, for safety, stepping down").fmt(resp->get_type()));
        ::exit(-1);
        break;
    }
}

void raft_server::handle_ext_resp_err(rpc_exception& err) {
    l_.debug(lstrfmt("receive an rpc error response from peer server, %s").fmt(err.what()));
    req_msg* req = err.req();
    if (req->get_type() == msg_type::sync_log_request ||
        req->get_type() == msg_type::join_cluster_request ||
        req->get_type() == msg_type::leave_cluster_request) {
        peer* p = nilptr;
        if (req->get_type() == msg_type::leave_cluster_request) {
            peer_itor pit = peers_.find(req->get_dst());
            if (pit != peers_.end()) {
                p = pit->second;
            }
        }
        else {
            p = srv_to_join_.get();
        }

        if (p != nilptr) {
            if (p->get_current_hb_interval() > ctx_->params_->max_hb_interval()) {
                if (req->get_type() == msg_type::leave_cluster_request) {
                    l_.info(lstrfmt("rpc failed again for the removing server (%d), will remove this server directly").fmt(p->get_id()));
                    rm_srv_from_cluster(p->get_id());
                }
                else {
                    l_.info(lstrfmt("rpc failed again for the new coming server (%d), will stop retry for this server").fmt(p->get_id()));
                    config_changing_ = false;
                    srv_to_join_.reset();
                }

                delete req;
            }
            else {
                // reuse the heartbeat interval value to indicate when to stop retrying, as rpc backoff is the same
                l_.debug("retry the request");
                p->slow_down_hb();
                std::shared_ptr<delayed_task> task(new timer_task<void>((timer_task<void>::executor)std::bind(&raft_server::on_retryable_req_err, this, p, req)));
                scheduler_.schedule(task, p->get_current_hb_interval());
            }
        }
    }
}

void raft_server::on_retryable_req_err(peer* p, req_msg* req) {
    l_.debug(sstrfmt("retry the request %d for %d").fmt(req->get_type(), p->get_id()));
    p->send_req(req, ex_resp_handler_);
}

resp_msg* raft_server::handle_rm_srv_req(req_msg& req) {
    std::vector<log_entry*>& entries(req.log_entries());
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::remove_server_response, id_, leader_);
    if (entries.size() != 1 || entries[0]->get_buf().size() != sz_int) {
        l_.info("bad remove server request as we are expecting one log entry with value type of int");
        return resp;
    }

    if (role_ != srv_role::leader) {
        l_.info("this is not a leader, cannot handle RemoveServerRequest");
        return resp;
    }

    if (config_changing_ || config_->get_log_idx() == 0 || config_->get_log_idx() > state_->get_commit_idx()) {
        // the previous config has not committed yet
        l_.info("previous config has not committed yet");
        return resp;
    }

    int32 srv_id = entries[0]->get_buf().get_int();
    if (srv_id == id_) {
        l_.info("cannot request to remove leader");
        return resp;
    }

    peer_itor pit = peers_.find(srv_id);
    if (pit == peers_.end()) {
        l_.info(sstrfmt("server %d does not exist").fmt(srv_id));
        return resp;
    }

    peer* p = pit->second;
    config_changing_ = true;
    req_msg* leave_req = new req_msg(state_->get_term(), msg_type::leave_cluster_request, id_, srv_id, 0, log_store_->next_slot() - 1, state_->get_commit_idx());
    p->send_req(leave_req, ex_resp_handler_);
    resp->accept(log_store_->next_slot());
    return resp;
}

resp_msg* raft_server::handle_add_srv_req(req_msg& req) {
    std::vector<log_entry*>& entries(req.log_entries());
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::add_server_response, id_, leader_);
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::cluster_server) {
        l_.debug("bad add server request as we are expecting one log entry with value type of ClusterServer");
        return resp;
    }

    if (role_ != srv_role::leader) {
        l_.info("this is not a leader, cannot handle AddServerRequest");
        return resp;
    }

    std::unique_ptr<srv_config> srv_conf(srv_config::deserialize(entries[0]->get_buf()));
    if (peers_.find(srv_conf->get_id()) != peers_.end() || id_ == srv_conf->get_id()) {
        l_.warn(lstrfmt("the server to be added has a duplicated id with existing server %d").fmt(srv_conf->get_id()));
        return resp;
    }

    if (config_changing_ || config_->get_log_idx() == 0 || config_->get_log_idx() > state_->get_commit_idx()) {
        // the previous config has not committed yet
        l_.info("previous config has not committed yet");
        return resp;
    }

    config_changing_ = true;
    conf_to_add_ = std::move(srv_conf);
    srv_to_join_.reset(new peer(*conf_to_add_, *ctx_, (timer_task<peer&>::executor)std::bind(&raft_server::handle_hb_timeout, this, std::placeholders::_1)));
    invite_srv_to_join_cluster();
    resp->accept(log_store_->next_slot());
    return resp;
}

resp_msg* raft_server::handle_log_sync_req(req_msg& req) {
    std::vector<log_entry*>& entries = req.log_entries();
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::sync_log_response, id_, req.get_src());
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::log_pack) {
        l_.info("receive an invalid LogSyncRequest as the log entry value doesn't meet the requirements");
        return resp;
    }

    if (!catching_up_) {
        l_.info("This server is ready for cluster, ignore the request");
        return resp;
    }

    log_store_->apply_pack(req.get_last_log_idx() + 1, entries[0]->get_buf());
    commit(log_store_->next_slot() - 1);
    resp->accept(log_store_->next_slot());
    return resp;
}

void raft_server::sync_log_to_new_srv(ulong start_idx) {
    // only sync committed logs
    int32 gap = (int32)(state_->get_commit_idx() - start_idx);
    if (gap < ctx_->params_->log_sync_stop_gap_) {
        l_.info(lstrfmt("LogSync is done for server %d with log gap %d, now put the server into cluster").fmt(srv_to_join_->get_id(), gap));
        cluster_config* new_conf = new cluster_config(log_store_->next_slot(), config_->get_log_idx());
        new_conf->get_servers().insert(new_conf->get_servers().end(), config_->get_servers().begin(), config_->get_servers().end());
        new_conf->get_servers().push_back(conf_to_add_.release());
        std::unique_ptr<log_entry> entry(new log_entry(state_->get_term(), new_conf->serialize(), log_val_type::conf));
        log_store_->append(*entry);
        config_.reset(new_conf);
        enable_hb_for_peer(*srv_to_join_);
        peers_.insert(std::make_pair(srv_to_join_->get_id(), srv_to_join_.release()));
        config_changing_ = false;
        request_append_entries();
        return;
    }

    req_msg* req = nilptr;
    if (start_idx > 0 && start_idx < log_store_->start_index()) {
        req = create_sync_snapshot_req(*srv_to_join_, start_idx, state_->get_term(), state_->get_commit_idx());
    }
    else {
        int32 size_to_sync = std::min(gap, ctx_->params_->log_sync_batch_size_);
        buffer* log_pack = log_store_->pack(start_idx, size_to_sync);
        req = new req_msg(state_->get_term(), msg_type::sync_log_request, id_, srv_to_join_->get_id(), 0L, start_idx - 1, state_->get_commit_idx());
        req->log_entries().push_back(new log_entry(state_->get_term(), log_pack, log_val_type::log_pack));
    }

    srv_to_join_->send_req(req, ex_resp_handler_);
}

void raft_server::invite_srv_to_join_cluster() {
    req_msg* req = new req_msg(state_->get_term(), msg_type::join_cluster_request, id_, srv_to_join_->get_id(), 0L, log_store_->next_slot() - 1, state_->get_commit_idx());
    req->log_entries().push_back(new log_entry(state_->get_term(), config_->serialize(), log_val_type::conf));
    srv_to_join_->send_req(req, ex_resp_handler_);
}

resp_msg* raft_server::handle_join_cluster_req(req_msg& req) {
    std::vector<log_entry*>& entries = req.log_entries();
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::join_cluster_response, id_, req.get_src());
    if (entries.size() != 1 || entries[0]->get_val_type() != log_val_type::conf) {
        l_.info("receive an invalid JoinClusterRequest as the log entry value doesn't meet the requirements");
        return resp;
    }

    if (catching_up_) {
        l_.info("this server is already in log syncing mode");
        return resp;
    }

    catching_up_ = true;
    role_ = srv_role::follower;
    leader_ = req.get_src();
    state_->set_commit_idx(0);
    state_->set_voted_for(-1);
    state_->set_term(req.get_term());
    ctx_->state_mgr_->save_state(*state_);
    reconfigure(std::shared_ptr<cluster_config>(cluster_config::deserialize(entries[0]->get_buf())));
    resp->accept(log_store_->next_slot());
    return resp;
}

resp_msg* raft_server::handle_leave_cluster_req(req_msg& req) {
    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::leave_cluster_response, id_, req.get_src());
    steps_to_down_ = 2;
    resp->accept(log_store_->next_slot());
    return resp;
}

void raft_server::rm_srv_from_cluster(int32 srv_id) {
    peer_itor it = peers_.find(srv_id);
    if (it == peers_.end()) {
        return;
    }

    peer* p = it->second;
    p->enable_hb(false);
    peers_.erase(it);
    cluster_config* new_conf = new cluster_config(log_store_->next_slot(), config_->get_log_idx());
    for (cluster_config::const_srv_itor it = config_->get_servers().begin(); it != config_->get_servers().end(); ++it) {
        if ((*it)->get_id() != srv_id) {
            new_conf->get_servers().push_back(*it);
        }
    }

    config_changing_ = false;
    std::unique_ptr<log_entry> entry(new log_entry(state_->get_term(), new_conf->serialize(), log_val_type::conf));
    log_store_->append(*entry);
    config_.reset(new_conf);
    request_append_entries();
}

int32 raft_server::get_snapshot_sync_block_size() const {
    int32 block_size = ctx_->params_->snapshot_block_size_;
    return block_size == 0 ? default_snapshot_sync_block_size : block_size;
}

req_msg* raft_server::create_sync_snapshot_req(peer& p, ulong last_log_idx, ulong term, ulong commit_idx) {
    std::lock_guard<std::mutex> guard(p.get_lock());
    snapshot_sync_ctx* sync_ctx = p.get_snapshot_sync_ctx();
    snapshot* snp = sync_ctx == nilptr ? nilptr : &sync_ctx->get_snapshot();
    std::unique_ptr<snapshot> last_snp(state_machine_.last_snapshot());
    if (snp == nilptr || (last_snp && last_snp->get_last_log_idx() > snp->get_last_log_idx())) {
        snp = last_snp.release();
        if (snp == nilptr || last_log_idx > snp->get_last_log_idx()) {
            l_.err(
                lstrfmt("system is running into fatal errors, failed to find a snapshot for peer %d(snapshot null: %d, snapshot doesn't contais lastLogIndex: %d")
                .fmt(p.get_id(), snp == nilptr, last_log_idx > snp->get_last_log_idx()));
            ::exit(-1);
            return nilptr;
        }

        if (snp->size() < 1L) {
            l_.err("invalid snapshot, this usually means a bug from state machine implementation, stop the system to prevent further errors");
            ::exit(-1);
            return nilptr;
        }

        l_.info(sstrfmt("trying to sync snapshot with last index %llu to peer %d").fmt(snp->get_last_log_idx(), p.get_id()));
        p.set_snapshot_in_sync(snp);
    }

    ulong offset = p.get_snapshot_sync_ctx()->get_offset();
    int32 sz_left = (int32)(snp->size() - offset);
    int32 blk_sz = get_snapshot_sync_block_size();
    buffer* data = buffer::alloc((size_t)(std::min(blk_sz, sz_left)));
    int32 sz_rd = state_machine_.read_snapshot_data(*snp, offset, *data);
    if ((size_t)sz_rd < data->size()) {
        l_.err(lstrfmt("only %d bytes could be read from snapshot while %d bytes are expected, must be something wrong, exit.").fmt(sz_rd, data->size()));
        ::exit(-1);
        return nilptr;
    }

    std::unique_ptr<snapshot_sync_req> sync_req(new snapshot_sync_req(*snp, offset, data, (offset + (ulong)data->size()) >= snp->size()));
    req_msg* req = new req_msg(term, msg_type::install_snapshot_request, id_, p.get_id(), snp->get_last_log_term(), snp->get_last_log_idx(), commit_idx);
    req->log_entries().push_back(new log_entry(term, sync_req->serialize(), log_val_type::snp_sync_req));
    return req;
}