#ifndef _RPC_LISTENER_HXX_
#define _RPC_LISTENER_HXX_

namespace cornerstone {
    // for backward compatibility
    class raft_server;
    typedef raft_server msg_handler;

    class rpc_listener {
    __interface_body__(rpc_listener)
    public:
        virtual void listen(ptr<msg_handler>& handler) = 0;
        virtual void stop() = 0;
    };
}

#endif //_RPC_LISTENER_HXX_