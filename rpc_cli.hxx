#ifndef _RPC_CLI_HXX_
#define _RPC_CLI_HXX_

namespace cornerstone {
    class rpc_client {
    __interface_body__(rpc_client)
    public:
        virtual void send(req_msg* req, async_result<resp_msg*>::handler_type& when_done) = 0;
    };
}

#endif //_RPC_CLI_HXX_
