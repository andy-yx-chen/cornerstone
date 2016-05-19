#ifndef _ASIO_SERVICE_HXX_
#define _ASIO_SERVICE_HXX_

namespace cornerstone {
    class asio_service : public delayed_task_scheduler, public rpc_client_factory {
    public:
        asio_service();

        __nocopy__(asio_service)
    public:
        virtual void schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) __override__;
        virtual rpc_client* create_client(const std::string& endpoint) __override__;
        void stop() {
            continue_.store(0);
            keep_alive_tm_.cancel();
        }

    private:
        virtual void cancel_impl(std::shared_ptr<delayed_task>& task) __override__;
        void worker_entry();
        void keep_alive(asio::error_code err) {
            if (!err && continue_.load() == 1) {
                keep_alive_tm_.expires_after(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1000)));
                keep_alive_tm_.async_wait(std::bind(&asio_service::keep_alive, this, std::placeholders::_1));
            }
        }
    private:
        asio::io_service io_svc_;
        asio::steady_timer keep_alive_tm_;
        std::atomic_int continue_;
    };
}

#endif //_ASIO_SERVICE_HXX_