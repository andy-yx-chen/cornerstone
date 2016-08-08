#include "../cornerstone.hxx"
#include <cassert>

#define LOG_INDEX_FILE "store.idx"
#define LOG_DATA_FILE "store.dat"
#define LOG_START_INDEX_FILE "store.sti"
#define LOG_INDEX_FILE_BAK "store.idx.bak"
#define LOG_DATA_FILE_BAK "store.dat.bak"
#define LOG_START_INDEX_FILE_BAK "store.sti.bak"

using namespace cornerstone;

static void cleanup() {
    std::remove(LOG_INDEX_FILE);
    std::remove(LOG_DATA_FILE);
    std::remove(LOG_START_INDEX_FILE);
    std::remove(LOG_INDEX_FILE_BAK);
    std::remove(LOG_DATA_FILE_BAK);
    std::remove(LOG_START_INDEX_FILE_BAK);
}

static ptr<log_entry> rnd_entry(std::function<int32()>& rnd) {
    ptr<buffer> buf = buffer::alloc(rnd() % 100 + 8);
    for (size_t i = 0; i < buf->size(); ++i) {
        buf->put(static_cast<byte>(rnd() % 256));
    }

    buf->pos(0);
    log_val_type t = (log_val_type)(rnd() % 5 + 1);
    return cs_new<log_entry>(rnd(), buf, t);
}

static bool entry_equals(log_entry& entry1, log_entry& entry2) {
    bool result = entry1.get_term() == entry2.get_term() 
                    && entry1.get_val_type() == entry2.get_val_type()
                    && entry1.get_buf().size() == entry2.get_buf().size();
    if (result) {
        for (size_t i = 0; i < entry1.get_buf().size(); ++i) {
            byte b1 = entry1.get_buf().data()[i];
            byte b2 = entry2.get_buf().data()[i];
            result = b1 == b2;
        }
    }

    return result;
}

void test_log_store() {
    uint seed = (uint)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine engine(seed);
    std::uniform_int_distribution<int32> distribution(1, 10000);
    std::function<int32()> rnd = std::bind(distribution, engine);

    cleanup();
    fs_log_store store(".", 100);

    // initialization test
    ptr<log_entry> entry(store.last_entry());
    assert(entry->get_term() == 0);
    assert(store.next_slot() == 1);
    assert(store.entry_at(1) == nilptr);

    std::vector<ptr<log_entry>> logs;
    for (int i = 0; i < 100 + rnd() % 100; ++i) {
        ptr<log_entry> item(rnd_entry(rnd));
        store.append(item);
        logs.push_back(item);
    }

    // overall test
    assert(logs.size() == static_cast<size_t>(store.next_slot() - 1));
    ptr<log_entry> last(store.last_entry());
    assert(entry_equals(*last, *(logs[logs.size() - 1])));

    // random item test
    for (int i = 0; i < 20; ++i) {
        size_t idx = (size_t)(rnd() % logs.size());
        ptr<log_entry> item(store.entry_at(idx + 1));
        assert(entry_equals(*item, *(logs[idx])));
    }

    // random range test
    size_t rnd_idx = (size_t)rnd() % logs.size();
    size_t rnd_sz = (size_t)rnd() % (logs.size() - rnd_idx);
    ptr<std::vector<ptr<log_entry>>> entries = store.log_entries(rnd_idx + 1, rnd_idx + rnd_sz + 1);
    for (size_t i = rnd_idx; i < rnd_idx + rnd_sz; ++i) {
        assert(entry_equals(*(logs[i]), *(*entries)[i - rnd_idx]));
    }
    
    store.close();
    fs_log_store store1(".", 100);
    // overall test
    assert(logs.size() == static_cast<size_t>(store1.next_slot() - 1));
    ptr<log_entry> last1(store1.last_entry());
    assert(entry_equals(*last1, *(logs[logs.size() - 1])));

    // random item test
    for (int i = 0; i < 20; ++i) {
        size_t idx = (size_t)rnd() % logs.size();
        ptr<log_entry> item(store1.entry_at(idx + 1));
        assert(entry_equals(*item, *(logs[idx])));
    }

    // random range test
    rnd_idx = (size_t)rnd() % logs.size();
    rnd_sz = (size_t)rnd() % (logs.size() - rnd_idx);
    ptr<std::vector<ptr<log_entry>>> entries1(store1.log_entries(rnd_idx + 1, rnd_idx + rnd_sz + 1));
    for (size_t i = rnd_idx; i < rnd_idx + rnd_sz; ++i) {
        assert(entry_equals(*(logs[i]), *(*entries1)[i - rnd_idx]));
    }

    // test write at
    ptr<log_entry> item(rnd_entry(rnd));
    rnd_idx = rnd() % store1.next_slot() + 1;
    store1.write_at(rnd_idx, item);
    assert(rnd_idx + 1 == store1.next_slot());
    ptr<log_entry> last2(store1.last_entry());
    assert(entry_equals(*item, *last2));
    store1.close();
    fs_log_store store2(".", 100);
    ptr<log_entry> last3(store2.last_entry());
    assert(entry_equals(*item, *last3));
    store2.close();
    cleanup();
}