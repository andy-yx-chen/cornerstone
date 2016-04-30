#ifndef _DELAYED_TASK_SCHEDULER_HXX_
#define _DELAYED_TASK_SCHEDULER_HXX_

namespace cornerstone {
    class delayed_task_scheduler {
    __interface_body__(delayed_task_scheduler)

    public:
        virtual void schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds);
    };
}

#endif //_DELAYED_TASK_SCHEDULER_HXX_
