#ifndef _RPC_CLI_HXX_
#define _RPC_CLI_HXX_

namespace cornerstone {
    typedef async_result<resp_msg*, rpc_exception*> rpc_result;
    typedef rpc_result::handler_type rpc_handler;

    class rpc_client {
    __interface_body__(rpc_client)
    public:
        virtual void send(req_msg* req, rpc_handler& when_done) = 0;
    };
}

#endif //_RPC_CLI_HXX_
