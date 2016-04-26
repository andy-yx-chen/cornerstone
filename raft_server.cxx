#include "cornerstone.hxx"

using namespace cornerstone;

const int raft_server::default_snapshot_sync_block_size = 4 * 1024;

resp_msg* raft_server::process_req(req_msg& req) {
    std::lock_guard<std::mutex> guard(lock_);
    l_.debug(
        strfmt<200>("Receive a %d message from %d with LastLogIndex=%lu, LastLogTerm=%lu, EntriesLength=%d, CommitIndex=%lu and Term=%lu")
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
            strfmt<200>("Response back a %d message to %d with Accepted=%d, Term=%lu, NextIndex=%lu")
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
                strfmt<200>("Receive AppendEntriesRequest from another leader(%d) with same term, there must be a bug, server exits")
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
                reconfigure(ctx_->state_mgr_->load_config());
            }

            log_store_->write_at(idx++, *(req.log_entries().at(log_idx++)));
        }

        // append new log entries
        while (log_idx < req.log_entries().size()) {
            log_entry* entry = req.log_entries().at(log_idx);
            log_store_->append(*entry);
            if (entry->get_val_type() == log_val_type::conf) {
                reconfigure(cluster_config::deserialize(entry->get_buf()));
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

        l_.info(strfmt<100>("stepping down (cycles left: %d), skip this election timeout event").fmt(steps_to_down_));
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
    l_.info(strfmt<60>("requestVote started with term %lu").fmt(state_->get_term()));
    state_->set_voted_for(id_);
    ctx_->state_mgr_->save_state(*state_);
    votes_granted_ += 1;
    votes_responded_ += 1;

    // is this the only server?
    if (votes_granted_ > (int)(peers_.size() + 1) / 2) {
        election_completed_ = true;
        become_leader();
        return;
    }

    log_entry& entry = log_store_->last_entry();
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        req_msg* req = new req_msg(state_->get_term(), msg_type::request_vote_request, id_, it->second->get_id(), log_store_->last_entry().get_term(), log_store_->next_slot() - 1, state_->get_commit_idx());
        l_.debug(strfmt<60>("send %d to server %d with term %lu").fmt(req->get_type(), it->second->get_id(), state_->get_term()));
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

    l_.debug(strfmt<100>("Server %d is busy, skip the request").fmt(p.get_id()));
    return false;
}

void raft_server::handle_peer_resp(resp_msg* resp_p, rpc_exception* err_p) {
    std::lock_guard<std::mutex> guard(lock_);
    std::unique_ptr<resp_msg> resp(resp_p);
    std::unique_ptr<rpc_exception> err(err_p);
    if (err) {
        l_.info(strfmt<100>("peer response error: %s").fmt(err->what()));
        return;
    }

    l_.debug(
        strfmt<200>("Receive a %d message from peer %d with Result=%d, Term=%ul, NextIndex=%ul")
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
        l_.err(strfmt<100>("Received an unexpected message %d for response, system exits.").fmt(resp->get_type()));
        ::exit(-1);
        break;
    }
}

void raft_server::handle_append_entries_resp(resp_msg& resp) {
    peer_itor it = peers_.find(resp.get_src());
    if (it == peers_.end()) {
        l_.info(strfmt<100>("the response is from an unkonw peer %d").fmt(resp.get_src()));
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
        l_.info(strfmt<100>("the response is from an unkonw peer %d").fmt(resp.get_src()));
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
            }
        }
    }
}