#ifndef _DELAYED_TASK_HXX_
#define _DELAYED_TASK_HXX_

namespace cornerstone {
    class delayed_task {
    public:
        delayed_task()
            : lock_(), cancelled_(false), impl_ctx_(nilptr), impl_ctx_del_() {}
        virtual ~delayed_task() {
            if (impl_ctx_ != nilptr) {
                if (impl_ctx_del_) {
                    impl_ctx_del_(impl_ctx_);
                }
            }
        }

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

        void reset() {
            std::lock_guard<std::mutex> guard(lock_);
            cancelled_ = false;
        }

        void* get_impl_context() const {
            return impl_ctx_;
        }

        void set_impl_context(void* ctx, std::function<void(void*)> del) {
            impl_ctx_ = ctx;
            impl_ctx_del_ = del;
        }

        std::mutex& task_lock() {
            return lock_;
        }

    protected:
        virtual void exec() = 0;

    private:
        std::mutex lock_;
        bool cancelled_;
        void* impl_ctx_;
        std::function<void(void*)> impl_ctx_del_;
    };
}

#endif