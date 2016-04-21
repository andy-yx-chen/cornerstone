#ifndef _RPC_EXCEPTION_HXX_
#define _RPC_EXCEPTION_HXX_

namespace cornerstone {
    class rpc_exception : public std::exception {
    public:
        rpc_exception(std::exception& cause, req_msg* req)
            : std::exception(cause), req_(req) {}

        __nocopy__(rpc_exception)
    public:
        req_msg* req() const { return req_; }
    private:
        req_msg* req_;
    };
}

#endif //_RPC_EXCEPTION_HXX_
