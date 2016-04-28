#ifndef _CONTEXT_HXX_
#define _CONTEXT_HXX_

namespace cornerstone {
    struct context {
    public:
        context(state_mgr* mgr, state_machine* m, rpc_listener* listener, logger* l, rpc_client_factory* cli_factory, delayed_task_scheduler* scheduler, raft_params* params = nilptr)
            : state_mgr_(mgr), state_machine_(m), rpc_listener_(listener), logger_(l), rpc_cli_factory_(cli_factory), scheduler_(scheduler), params_(params == nilptr ? new raft_params : params) {}

    __nocopy__(context)
    public:
        std::unique_ptr<state_mgr> state_mgr_;
        std::unique_ptr<state_machine> state_machine_;
        std::unique_ptr<rpc_listener> rpc_listener_;
        std::unique_ptr<logger> logger_;
        std::unique_ptr<rpc_client_factory> rpc_cli_factory_;
        std::unique_ptr<delayed_task_scheduler> scheduler_;
        std::unique_ptr<raft_params> params_;
    };
}

#endif //_CONTEXT_HXX_
