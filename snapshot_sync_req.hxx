#ifndef _SNAPSHOT_SYNC_REQ_HXX_
#define _SNAPSHOT_SYNC_REQ_HXX_

namespace cornerstone {
    class snapshot_sync_req {
    public:
        snapshot_sync_req(snapshot& s, ulong offset, buffer* buf, bool done)
            : snapshot_(s), offset_(offset), data_(buf), done_(done) {}

        ~snapshot_sync_req() {
            buffer::release(data_);
        }

    __nocopy__(snapshot_sync_req)
    public:
        static snapshot_sync_req* deserialize(buffer* buf);

        snapshot& get_snapshot() const {
            return snapshot_;
        }

        ulong get_offset() const { return offset_; }

        buffer& get_data() const { return *data_; }

        bool is_done() const { return done_; }

        buffer* serialize();
    private:
        snapshot& snapshot_;
        ulong offset_;
        buffer* data_;
        bool done_;
    };
}

#endif //_SNAPSHOT_SYNC_REQ_HXX_
