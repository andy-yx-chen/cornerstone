#define ASIO_STANDALONE 1
#define ASIO_HAS_STD_CHRONO 1
#include "cornerstone.hxx"
#include <fstream>
#include <queue>
#include <asio.hpp>
#include <ctime>

namespace cornerstone {

    namespace impls {
        class fs_based_logger : public logger {
        public:
            fs_based_logger(const std::string& log_file, cornerstone::asio_service::log_level level)
                : level_(level), fs_(log_file), buffer_(), lock_() {}

            virtual ~fs_based_logger() {
                flush();
                fs_.flush();
                fs_.close();
            }

            __nocopy__(fs_based_logger)
        public:
            virtual void debug(const std::string& log_line) {
                if (level_ <= 0) {
                    write_log("dbug", log_line);
                }
            }

            virtual void info(const std::string& log_line) {
                if (level_ <= 1) {
                    write_log("info", log_line);
                }
            }

            virtual void warn(const std::string& log_line) {
                if (level_ <= 2) {
                    write_log("warn", log_line);
                }
            }

            virtual void err(const std::string& log_line) {
                if (level_ <= 3) {
                    write_log("errr", log_line);
                }
            }

            void flush();
        private:
            void write_log(const std::string& level, const std::string& log_line);
        private:
            cornerstone::asio_service::log_level level_;
            std::ofstream fs_;
            std::queue<std::string> buffer_;
            std::mutex lock_;
        };
    }

    class asio_service_impl {
    public:
        asio_service_impl();
        ~asio_service_impl();

    private:
        void stop();

        void worker_entry();
        void flush_all_loggers(asio::error_code err);

    private:
        asio::io_service io_svc_;
        asio::steady_timer log_flush_tm_;
        std::atomic_int continue_;
        std::mutex logger_list_lock_;
        std::list<impls::fs_based_logger*> loggers_;
        friend asio_service;
    };
}

using namespace cornerstone;
using namespace cornerstone::impls;

void _free_timer_(void* ptr) {
    asio::steady_timer* timer = static_cast<asio::steady_timer*>(ptr);
    delete timer;
}

void _timer_handler_(std::shared_ptr<delayed_task>& task, asio::error_code err) {
    if (!err) {
        task->execute();
    }
}


asio_service_impl::asio_service_impl()
    : io_svc_(), log_flush_tm_(io_svc_), continue_(1), logger_list_lock_(), loggers_() {
    // set expires_after to a very large value so that this will not affect the overall performance
    log_flush_tm_.expires_after(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)));
    log_flush_tm_.async_wait(std::bind(&asio_service_impl::flush_all_loggers, this, std::placeholders::_1));
    unsigned int cpu_cnt = std::thread::hardware_concurrency();
    if (cpu_cnt == 0) {
        cpu_cnt = 1;
    }

    for (unsigned int i = 0; i < cpu_cnt; ++i) {
        std::thread t(std::bind(&asio_service_impl::worker_entry, this));
        t.detach();
    }
}

asio_service_impl::~asio_service_impl() {
    stop();
}

void asio_service_impl::worker_entry() {
    try {
        io_svc_.run();
    }
    catch (...) {
        // ignore all exceptions
    }
}

void asio_service_impl::flush_all_loggers(asio::error_code err) {
    if (!err && continue_.load() == 1) {
        log_flush_tm_.expires_after(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1000)));
        log_flush_tm_.async_wait(std::bind(&asio_service_impl::flush_all_loggers, this, std::placeholders::_1));
        {
            std::lock_guard<std::mutex> guard(logger_list_lock_);
            for (std::list<fs_based_logger*>::iterator it = loggers_.begin(); it != loggers_.end(); ++it) {
                (*it)->flush();
            }
        }
    }
}

void asio_service_impl::stop() {
    int running = 1;
    if (continue_.compare_exchange_strong(running, 0)) {
        log_flush_tm_.cancel();
    }
}

void fs_based_logger::flush() {
    std::queue<std::string> backup;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (buffer_.size() > 0) {
            backup.swap(buffer_);
        }
    }

    while (backup.size() > 0) {
        std::string& line = backup.front();
        fs_ << line << std::endl;
        backup.pop();
    }
}

void fs_based_logger::write_log(const std::string& level, const std::string& log_line) {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    int ms = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&now_c);
    std::hash<std::thread::id> hasher;
    std::string line(sstrfmt("%d/%d/%d %d:%d:%d.%d\t[%d]\t%s\t").fmt(tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, ms, hasher(std::this_thread::get_id()), level.c_str()));
    line += log_line;
    {
        std::lock_guard<std::mutex> guard(lock_);
        buffer_.push(line);
    }
}

asio_service::asio_service()
    : impl_(new asio_service_impl) {
    
}

asio_service::~asio_service() {
    delete impl_;
}

void asio_service::schedule(std::shared_ptr<delayed_task>& task, int32 milliseconds) {
    if (task->get_impl_context() == nilptr) {
        task->set_impl_context(new asio::steady_timer(impl_->io_svc_), &_free_timer_);
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

void asio_service::stop() {
    impl_->stop();
}

rpc_client* asio_service::create_client(const std::string& endpoint) {
    return nilptr;
}

logger* asio_service::create_logger(log_level level, const std::string& log_file) {
    fs_based_logger* l = new fs_based_logger(log_file, level);
    {
        std::lock_guard<std::mutex> guard(impl_->logger_list_lock_);
        impl_->loggers_.push_back(l);
    }

    return l;
}

rpc_listener* asio_service::create_rpc_listener(int listening_port) {
    return nilptr;
}