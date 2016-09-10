#ifndef _ASIO_SERVICE_HXX_
#define _ASIO_SERVICE_HXX_

namespace cornerstone {

    /**
      Declaring this to hide the dependency of asio.hpp from root header file, which can boost the compilation time
    */
    class asio_service_impl;

    class asio_service : public delayed_task_scheduler, public rpc_client_factory {
    public:
        enum log_level {
            debug = 0x0,
            info,
            warnning,
            error
        };

    public:
        asio_service();
        ~asio_service();

        __nocopy__(asio_service)
    public:
        virtual void schedule(ptr<delayed_task>& task, int32 milliseconds) __override__;
        virtual ptr<rpc_client> create_client(const std::string& endpoint) __override__;

        logger* create_logger(log_level level, const std::string& log_file);

        ptr<rpc_listener> create_rpc_listener(ushort listening_port, logger& l);

        void stop();

    private:
        virtual void cancel_impl(ptr<delayed_task>& task) __override__;
    private:
        asio_service_impl* impl_;
    };
}

#endif //_ASIO_SERVICE_HXX_