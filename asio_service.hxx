#ifndef _ASIO_SERVICE_HXX_
#define _ASIO_SERVICE_HXX_

namespace cornerstone {
    class asio_service : public delayed_task_scheduler, public rpc_client_factory {
    public:
        asio_service()
            : io_svc_() {}

        __nocopy__(asio_service)
    public:
        virtual void schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) __override__;
        virtual rpc_client* create_client(const std::string& endpoint) __override__;

    private:
        virtual void cancel_impl(std::shared_ptr<delayed_task>& task) __override__;
    private:
        asio::io_service io_svc_;
    };
}

#endif //_ASIO_SERVICE_HXX_