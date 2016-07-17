#ifndef _SNAPSHOT_SYNC_CTX_HXX_
#define _SNAPSHOT_SYNC_CTX_HXX_

namespace cornerstone {
    struct snapshot_sync_ctx {
    public:
        snapshot_sync_ctx(const ptr<snapshot>& s, ulong offset = 0L)
            : snapshot_(s), offset_(offset) {}

    __nocopy__(snapshot_sync_ctx)
    
    public:
        const ptr<snapshot>& get_snapshot() const {
            return snapshot_;
        }

        ulong get_offset() const {
            return offset_;
        }

        void set_offset(ulong offset) {
            offset_ = offset;
        }
    public:
        ptr<snapshot> snapshot_;
        ulong offset_;
    };
}

#endif //_SNAPSHOT_SYNC_CTX_HXX_
