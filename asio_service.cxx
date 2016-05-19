#include "cornerstone.hxx"

using namespace cornerstone;

void _free_timer_(void* ptr) {
    asio::steady_timer* timer = static_cast<asio::steady_timer*>(ptr);
    delete timer;
}

void _timer_handler_(std::shared_ptr<delayed_task>& task, asio::error_code err) {
    if (!err) {
        task->execute();
    }
}

asio_service::asio_service()
    : io_svc_(), keep_alive_tm_(io_svc_), continue_(1) {
    // set expires_after to a very large value so that this will not affect the overall performance
    keep_alive_tm_.expires_after(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1000)));
    keep_alive_tm_.async_wait(std::bind(&asio_service::keep_alive, this, std::placeholders::_1));
    unsigned int cpu_cnt = std::thread::hardware_concurrency();
    if (cpu_cnt == 0) {
        cpu_cnt = 1;
    }

    for (unsigned int i = 0; i < cpu_cnt; ++i) {
        std::thread t(std::bind(&asio_service::worker_entry, this));
        t.detach();
    }
}

void asio_service::worker_entry() {
    io_svc_.run();
}

void asio_service::schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) {
    if (task->get_impl_context() == nilptr) {
        task->set_impl_context(new asio::steady_timer(io_svc_), &_free_timer_);
    }
    else {
        // this task has been scheduled before, ensure it's not in cancelled state
        task->reset();
    }

    asio::steady_timer* timer = static_cast<asio::steady_timer*>(task->get_impl_context());
    timer->expires_after(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(milliseconds)));
    timer->async_wait(std::bind(&_timer_handler_, task, std::placeholders::_1));
}

void asio_service::cancel_impl(std::shared_ptr<delayed_task>& task) {
    if (task->get_impl_context() != nilptr) {
        static_cast<asio::steady_timer*>(task->get_impl_context())->cancel();
    }
}

rpc_client* asio_service::create_client(const std::string& endpoint) {
    return nilptr;
}