#ifndef _UV_DELAYED_TASK_SCHEDULER_HXX_
#define _UV_DELAYED_TASK_SCHEDULER_HXX_

namespace cornerstone {
    class uv_delayed_task_scheduler : public delayed_task_scheduler {
    public:
        uv_delayed_task_scheduler(uv_loop_t& loop)
            : loop_(loop){}

        __nocopy__(uv_delayed_task_scheduler)
    public:
        virtual void schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) override;

    private:
        virtual void cancel_impl(std::shared_ptr<delayed_task>& task) override;

    private:
        uv_loop_t& loop_;
    };
}

#endif
