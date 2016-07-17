#ifndef _SNAPSHOT_SYNC_REQ_HXX_
#define _SNAPSHOT_SYNC_REQ_HXX_

namespace cornerstone {
    class snapshot_sync_req {
    public:
        snapshot_sync_req(const ptr<snapshot>& s, ulong offset, const ptr<buffer>& buf, bool done)
            : snapshot_(s), offset_(offset), data_(buf), done_(done) {}

    __nocopy__(snapshot_sync_req)
    public:
        static ptr<snapshot_sync_req> deserialize(buffer& buf);

        snapshot& get_snapshot() const {
            return *snapshot_;
        }

        ulong get_offset() const { return offset_; }

        buffer& get_data() const { return *data_; }

        bool is_done() const { return done_; }

        ptr<buffer> serialize();
    private:
        ptr<snapshot> snapshot_;
        ulong offset_;
        ptr<buffer> data_;
        bool done_;
    };
}

#endif //_SNAPSHOT_SYNC_REQ_HXX_
