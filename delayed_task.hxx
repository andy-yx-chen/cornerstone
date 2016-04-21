#ifndef _DELAYED_TASK_HXX_
#define _DELAYED_TASK_HXX_

namespace cornerstone {
    class delayed_task {
    public:
        delayed_task()
            : lock_(), cancelled_(false) {}
        virtual ~delayed_task() {}

    __nocopy__(delayed_task)
    public:
        void execute() {
            std::lock_guard<std::mutex> guard(lock_);
            if (!cancelled_) {
                exec();
            }
        }

        void cancel() {
            std::lock_guard<std::mutex> guard(lock_);
            cancelled_ = true;
        }

    protected:
        virtual void exec() = 0;

    private:
        std::mutex lock_;
        bool cancelled_;
    };
}

#endif