#include "cornerstone.hxx"

using namespace cornerstone;

const int raft_server::default_snapshot_sync_block_size = 4 * 1024;

resp_msg* raft_server::process_req(req_msg& req) {
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

    resp_msg* resp = new resp_msg(state_->get_term(), msg_type::append_entries_response, id_, req.get_src(), 0L, false);
    bool log_okay = req.get_last_log_idx() == 0 ||
        (req.get_last_log_idx() < log_store_->next_slot() && req.get_last_log_term() == std::unique_ptr<log_entry>(log_store_->entry_at(req.get_last_log_idx()))->get_term());
    if (req.get_term() < state_->get_term() || !log_okay) {
        return resp;
    }

    // follower & log is okay
    if (req.log_entries().size() > 0) {
        // write logs to store, start from overlapped logs
        ulong idx = req.get_last_log_idx() + 1;
        int log_idx = 0;
    }

    return resp;
}