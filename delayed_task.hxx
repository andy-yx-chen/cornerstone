#ifndef _DELAYED_TASK_HXX_
#define _DELAYED_TASK_HXX_

namespace cornerstone {
    class delayed_task {
    public:
        delayed_task()
            : cancelled_(false), impl_ctx_(nilptr), impl_ctx_del_() {}
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
            if (!cancelled_.load()) {
                exec();
            }
        }

        void cancel() {
            cancelled_.store(true);
        }

        void reset() {
            cancelled_.store(false);
        }

        void* get_impl_context() const {
            return impl_ctx_;
        }

        void set_impl_context(void* ctx, std::function<void(void*)> del) {
            impl_ctx_ = ctx;
            impl_ctx_del_ = del;
        }

    protected:
        virtual void exec() = 0;

    private:
        std::atomic<bool> cancelled_;
        void* impl_ctx_;
        std::function<void(void*)> impl_ctx_del_;
    };
}

#endif