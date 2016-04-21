#ifndef _CONTEXT_HXX_
#define _CONTEXT_HXX_

namespace cornerstone {
	struct context {
	public:
		context(state_mgr* mgr, state_machine* m, rpc_listener* listener, logger* l, rpc_client_factory* cli_factory, raft_params* params = nilptr)
			: state_mgr_(mgr), state_machine_(m), rpc_listener_(listener), logger_(l), rpc_cli_factory_(cli_factory), params_(params == nilptr ? new raft_params : params) {}

		~context() {
			delete state_mgr_;
			delete state_machine_;
			delete rpc_listener_;
			delete logger_;
			delete rpc_cli_factory_;
			delete params_;
		}

	public:
		state_mgr* state_mgr_;
		state_machine* state_machine_;
		rpc_listener* rpc_listener_;
		logger* logger_;
		rpc_client_factory* rpc_cli_factory_;
		raft_params* params_;
	};
}

#endif //_CONTEXT_HXX_
