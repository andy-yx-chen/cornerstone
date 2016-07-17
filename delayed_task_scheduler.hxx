#ifndef _DELAYED_TASK_SCHEDULER_HXX_
#define _DELAYED_TASK_SCHEDULER_HXX_

namespace cornerstone {
    class delayed_task_scheduler {
    __interface_body__(delayed_task_scheduler)

    public:
        virtual void schedule(ptr<delayed_task>& task, int32 milliseconds) = 0;

        void cancel(ptr<delayed_task>& task) {
            cancel_impl(task);
            task->cancel();
        }

    private:
        virtual void cancel_impl(ptr<delayed_task>& task) = 0;
    };
}

#endif //_DELAYED_TASK_SCHEDULER_HXX_
