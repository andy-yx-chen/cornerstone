#ifndef _DELAYED_TASK_HXX_
#define _DELAYED_TASK_HXX_

namespace cornerstone {
    class delayed_task {
    public:
        delayed_task()
            : lock_(), cancelled_(false), context_(nilptr) {}
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

        void* get_context() const {
            return context_;
        }

        void set_context(void* ctx) {
            context_ = ctx;
        }

    protected:
        virtual void exec() = 0;

    private:
        std::mutex lock_;
        bool cancelled_;
        void* context_;
    };
}

#endif