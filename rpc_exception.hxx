#ifndef _RPC_EXCEPTION_HXX_
#define _RPC_EXCEPTION_HXX_

namespace cornerstone {
    class rpc_exception : public std::exception {
    public:
        rpc_exception(const std::string& err, req_msg* req)
            : req_(req), err_(err.c_str()) {}

        __nocopy__(rpc_exception)
    public:
        req_msg* req() const { return req_; }

        virtual const char* what() const throw() __override__ {
            return err_;
        }
    private:
        req_msg* req_;
        const char* err_;
    };
}

#endif //_RPC_EXCEPTION_HXX_
