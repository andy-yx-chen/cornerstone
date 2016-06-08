#include "../cornerstone.hxx"
#include <queue>
#include <iostream>
#include <cassert>

using namespace cornerstone;

class in_mem_log_store : public log_store {
public:
    in_mem_log_store()
        : log_entries_(), lock_() {
        log_entries_.push_back(std::move(std::shared_ptr<log_entry>(new log_entry(0L, nilptr))));
    }

    __nocopy__(in_mem_log_store)

public:
    /**
    ** The first available slot of the store, starts with 1
    */
    virtual ulong next_slot() const {
        auto_lock(lock_);
        return (ulong)log_entries_.size();
    }

    /**
    ** The start index of the log store, at the very beginning, it must be 1
    ** however, after some compact actions, this could be anything greater or equals to one
    */
    virtual ulong start_index() const {
        return 1;
    }

    /**
    * The last log entry in store
    * @return a dummy constant entry with value set to null and term set to zero if no log entry in store
    */
    virtual log_entry& last_entry() const {
        auto_lock(lock_);
        return *log_entries_[log_entries_.size() - 1];
    }

    /**
    * Appends a log entry to store
    * @param entry
    */
    virtual void append(log_entry& entry) {
        auto_lock(lock_);
        std::shared_ptr<log_entry> item(new log_entry(entry.get_term(), buffer::copy(entry.get_buf()), entry.get_val_type()));
        log_entries_.push_back(item);
    }

    /**
    * Over writes a log entry at index of {@code index}
    * @param index a value < this->next_slot(), and starts from 1
    * @param entry
    */
    virtual void write_at(ulong index, log_entry& entry) {
        auto_lock(lock_);
        if(index >= (ulong)log_entries_.size() || index < 1){
            throw std::overflow_error("index out of range");
        }
        
        std::shared_ptr<log_entry> item(new log_entry(entry.get_term(), buffer::copy(entry.get_buf()), entry.get_val_type()));
        log_entries_[(size_t)index] = item;
        if((ulong)log_entries_.size() - index > 1){
            log_entries_.erase(log_entries_.begin() + (size_t)index + 1, log_entries_.end());
        }
    }

    /**
    * Get log entries with index between start and end
    * @param start, the start index of log entries
    * @param end, the end index of log entries (exclusive)
    * @return the log entries between [start, end)
    */
    virtual std::vector<log_entry*>* log_entries(ulong start, ulong end) {
        if(start >= end || start >= log_entries_.size()){
            return nilptr;
        }
        
        std::vector<log_entry*>* v = new std::vector<log_entry*>();
        for (size_t i = (size_t)start; i < (size_t)end; ++i) {
            v->push_back(entry_at(i));
        }

        return v;
    }

    /**
    * Gets the log entry at the specified index
    * @param index, starts from 1
    * @return the log entry or null if index >= this->next_slot()
    */
    virtual log_entry* entry_at(ulong index) {
        if ((size_t)index >= log_entries_.size()) {
            return nilptr;
        }

        std::shared_ptr<log_entry>& p = log_entries_[index];
        return new log_entry(p->get_term(), buffer::copy(p->get_buf()), p->get_val_type());
    }

    virtual ulong term_at(ulong index) {
        if ((size_t)index >= log_entries_.size()) {
            return 0L;
        }

        std::shared_ptr<log_entry>& p = log_entries_[index];
        return p->get_term();
    }

    /**
    * Pack cnt log items starts from index
    * @param index
    * @param cnt
    * @return log pack
    */
    virtual buffer* pack(ulong index, int32 cnt) {
        return nilptr;
    }

    /**
    * Apply the log pack to current log store, starting from index
    * @param index, the log index that start applying the pack, index starts from 1
    * @param pack
    */
    virtual void apply_pack(ulong index, buffer& pack) {
        //
    }

    /**
    * Compact the log store by removing all log entries including the log at the last_log_index
    * @param last_log_index
    * @return compact successfully or not
    */
    virtual bool compact(ulong last_log_index) {
        return true;
    }

private:
    std::vector<std::shared_ptr<log_entry>> log_entries_;
    mutable std::mutex lock_;
};

class in_memory_state_mgr : public state_mgr {
public:
    in_memory_state_mgr(int32 srv_id)
        : srv_id_(srv_id) {}

public:
    virtual cluster_config* load_config() {
        cluster_config* conf = new cluster_config;
        conf->get_servers().push_back(new srv_config(1, "port1"));
        conf->get_servers().push_back(new srv_config(2, "port2"));
        conf->get_servers().push_back(new srv_config(3, "port3"));
        return conf;
    }

    virtual void save_config(const cluster_config& config) {}
    virtual void save_state(const srv_state& state) {}
    virtual srv_state* read_state() {
        return new srv_state;
    }

    virtual log_store* load_log_store() {
        return new in_mem_log_store;
    }

    virtual int32 server_id() {
        return srv_id_;
    }

    virtual void system_exit(const int exit_code) {
        std::cout << "system exiting with code " << exit_code << std::endl;
    }

private:
    int32 srv_id_;
};

class dummy_state_machine : public state_machine {
public:
    virtual void commit(const ulong log_idx, buffer& data) {
        std::cout << "commit message:" << data.get_str() << std::endl;
    }

    virtual void pre_commit(const ulong log_idx, buffer& data) {}
    virtual void rollback(const ulong log_idx, buffer& data) {}
    virtual void save_snapshot_data(snapshot& s, const ulong offset, buffer& data) {}
    virtual bool apply_snapshot(snapshot& s) {
        return false;
    }

    virtual int read_snapshot_data(snapshot& s, const ulong offset, buffer& data) {
        return 0;
    }

    virtual snapshot* last_snapshot() {
        return nilptr;
    }

    virtual void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done) {}
};

class msg_bus {
public:
    typedef std::pair<req_msg*, std::shared_ptr<async_result<resp_msg*>>> message;
    
    class msg_queue {
    public:
        msg_queue()
            : queue_(), cv_(), lock_() {}
        __nocopy__(msg_queue)
    public:
        void enqueue(const message& msg) {
            {
                auto_lock(lock_);
                queue_.push(msg);
            }

            cv_.notify_one();
        }

        message dequeue() {
            std::unique_lock<std::mutex> u_lock(lock_);
            if (queue_.size() == 0) {
                cv_.wait(u_lock);
            }

            message& front = queue_.front();
            message m = std::make_pair(front.first, front.second);
            queue_.pop();
            return m;
        }
    private:
        std::queue<message> queue_;
        std::condition_variable cv_;
        std::mutex lock_;
    };

    typedef std::unordered_map<std::string, std::shared_ptr<msg_queue>> msg_q_map;

public:
    msg_bus()
        : q_map_() {
        q_map_.insert(std::make_pair("port1", std::shared_ptr<msg_queue>(new msg_queue)));
        q_map_.insert(std::make_pair("port2", std::shared_ptr<msg_queue>(new msg_queue)));
        q_map_.insert(std::make_pair("port3", std::shared_ptr<msg_queue>(new msg_queue)));
    }

    __nocopy__(msg_bus)

public:
    void send_msg(const std::string& port, message& msg) {
        msg_q_map::const_iterator itor = q_map_.find(port);
        if (itor != q_map_.end()) {
            itor->second->enqueue(msg);
            return;
        }

        // this is for test usage, if port is not found, faulted
        throw std::runtime_error("bad port for msg_bus");
    }

    msg_queue& get_queue(const std::string& port) {
        msg_q_map::iterator itor = q_map_.find(port);
        if (itor == q_map_.end()) {
            throw std::runtime_error("bad port for msg_bus, no queue found.");
        }

        return *itor->second;
    }

private:
    msg_q_map q_map_;
};

class test_rpc_client : public rpc_client {
public:
    test_rpc_client(msg_bus& bus, const std::string& port)
        : bus_(bus), port_(port) {}

    __nocopy__(test_rpc_client)

    virtual void send(req_msg* req, rpc_handler& when_done) __override__ {
        std::shared_ptr<async_result<resp_msg*>> result(new async_result<resp_msg*>());
	async_result<resp_msg*>::handler_type handler([req, when_done](resp_msg* resp, std::exception* err) -> void {
            if (err != nilptr) {
                when_done(nilptr, new rpc_exception(err->what(), req));
            }
            else {
                when_done(resp, nilptr);
            }

            delete req;
        });
        result->when_ready(handler);
        
        msg_bus::message msg(std::make_pair(req, result));
        bus_.send_msg(port_, msg);
    }
private:
    msg_bus& bus_;
    std::string port_;
};

class test_rpc_cli_factory : public rpc_client_factory {
public:
    test_rpc_cli_factory(msg_bus& bus)
        : bus_(bus) {}
    __nocopy__(test_rpc_cli_factory)
public:
    virtual rpc_client* create_client(const std::string& endpoint) __override__ {
        return new test_rpc_client(bus_, endpoint);
    }
private:
    msg_bus& bus_;
};

class test_rpc_listener : public rpc_listener {
public:
    test_rpc_listener(const std::string& port, msg_bus& bus)
        : queue_(bus.get_queue(port)), stopped_(false){}
    __nocopy__(test_rpc_listener)
public:
    virtual void listen(msg_handler* handler) __override__{
        std::thread t(std::bind(&test_rpc_listener::do_listening, this, handler));
        t.detach();
    }

    virtual void stop() {
        stopped_ = true;
    }
private:
    void do_listening(msg_handler* handler) {
        while (!stopped_) {
            msg_bus::message msg = queue_.dequeue();
            resp_msg* resp = handler->process_req(*(msg.first));
            msg.second->set_result(resp, nilptr);
        }
    }

private:
    msg_bus::msg_queue& queue_;
    bool stopped_;
};

msg_bus bus;
test_rpc_cli_factory rpc_factory(bus);
std::condition_variable stop_cv;
std::mutex lock;
asio_service asio_svc;
std::mutex stop_test_lock;
std::condition_variable stop_test_cv;

void run_raft_instance(int srv_id);
void test_raft_server() {
    std::thread t1(std::bind(&run_raft_instance, 1));
    t1.detach();
    std::thread t2(std::bind(&run_raft_instance, 2));
    t2.detach();
    std::thread t3(std::bind(&run_raft_instance, 3));
    t3.detach();
    std::cout << "waiting for leader election..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::unique_ptr<rpc_client> client(rpc_factory.create_client("port1"));
    req_msg* msg = new req_msg(0, msg_type::client_request, 0, 1, 0, 0, 0);
    buffer* buf = buffer::alloc(100);
    buf->put("hello");
    buf->pos(0);
    msg->log_entries().push_back(new log_entry(0, buf));
    rpc_handler handler = (rpc_handler)([&client](resp_msg* resp, rpc_exception* err) -> void {
        std::unique_ptr<resp_msg> rsp(resp);
        assert(rsp->get_accepted() || rsp->get_dst() > 0);
        if (!rsp->get_accepted()) {
            client.reset(rpc_factory.create_client(sstrfmt("port%d").fmt(rsp->get_dst())));
            req_msg* msg = new req_msg(0, msg_type::client_request, 0, 1, 0, 0, 0);
            buffer* buf = buffer::alloc(100);
            buf->put("hello");
            buf->pos(0);
            msg->log_entries().push_back(new log_entry(0, buf));
            rpc_handler handler = (rpc_handler)([&client](resp_msg* resp1, rpc_exception* err1) -> void {
                std::unique_ptr<resp_msg> rsp1(resp1);
                assert(rsp1->get_accepted());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                stop_test_cv.notify_all();
            });
            client->send(msg, handler);
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            stop_test_cv.notify_all();
        }
    });
    client->send(msg, handler);
    {
        std::unique_lock<std::mutex> l(stop_test_lock);
        stop_test_cv.wait(l);
    }

    stop_cv.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    asio_svc.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::remove("log1.log");
    std::remove("log2.log");
    std::remove("log3.log");
}

void run_raft_instance(int srv_id) {
    test_rpc_listener listener(sstrfmt("port%d").fmt(srv_id), bus);
    in_memory_state_mgr smgr(srv_id);
    dummy_state_machine smachine;
    std::unique_ptr<logger> l(asio_svc.create_logger(asio_service::log_level::debug, sstrfmt("log%d.log").fmt(srv_id)));
    raft_params* params(new raft_params());
    (*params).with_election_timeout_lower(200)
        .with_election_timeout_upper(400)
        .with_hb_interval(100)
        .with_max_append_size(100)
        .with_rpc_failure_backoff(50);
    context* ctx(new context(smgr, smachine, listener, *l, rpc_factory, asio_svc, params));
    raft_server srv(ctx);
    listener.listen(&srv);
    {
        std::unique_lock<std::mutex> ulock(lock);
        stop_cv.wait(ulock);
        listener.stop();
    }
}
