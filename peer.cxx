#include "cornerstone.hxx"

using namespace cornerstone;

void peer::send_req(req_msg* req, async_result<resp_msg*>::handler_type& handler) {
    async_result<resp_msg*>* pending = new async_result<resp_msg*>(handler);
    rpc_->send(req, std::bind(&peer::handle_rpc_result, this, req, pending, std::placeholders::_1, std::placeholders::_2));
}

void peer::handle_rpc_result(req_msg* req, async_result<resp_msg*>* pending_result, resp_msg* resp, std::exception* err) {
    if (err == nilptr) {
        resume_hb_speed();
        pending_result->set_result(resp, nilptr);
    }
    else {
        if (req->get_type() == msg_type::append_entries_request) {
            set_free();
        }

        slow_down_hb();
        pending_result->set_result(nilptr, err);
        if (resp != nilptr) {
            delete resp;
        }
    }

    delete pending_result;
}