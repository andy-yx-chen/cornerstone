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

void asio_service::schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) {
    if (task->get_impl_context() == nilptr) {
        task->set_impl_context(new asio::steady_timer(io_svc_), &_free_timer_);
    }

    asio::steady_timer* timer = static_cast<asio::steady_timer*>(task->get_impl_context());
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