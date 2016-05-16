#include "cornerstone.hxx"

using namespace cornerstone;

extern "C" static void timer_callback(uv_timer_t* timer) {
    //TODO should we dispatch this to thread pool instead of having this in io thread?
    std::unique_ptr<uv_timer_t> timer_to_delete(timer);
    std::unique_ptr<std::shared_ptr<delayed_task>> task(static_cast<std::shared_ptr<delayed_task>*>(timer->data));
    (*task)->execute();
    (*task)->set_context(nilptr);
}

void uv_delayed_task_scheduler::cancel_impl(std::shared_ptr<delayed_task>& task) {
    void* ctx = task->get_context();
    if (ctx != nilptr) {
        std::unique_ptr<uv_timer_t> timer(static_cast<uv_timer_t*>(ctx));
        std::unique_ptr<std::shared_ptr<delayed_task>> task_to_delete(static_cast<std::shared_ptr<delayed_task>*>(timer->data));
        ::uv_timer_stop(timer.get());
        task->set_context(nilptr);
    }
}

void uv_delayed_task_scheduler::schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) {
    uv_timer_t* timer = new uv_timer_t;
    timer->data = new std::shared_ptr<delayed_task>(task);
    task->set_context(timer);
    ::uv_timer_init(&loop_, timer);
    ::uv_timer_start(timer, &timer_callback, milliseconds, 0);
}