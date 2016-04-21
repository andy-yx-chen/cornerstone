#ifndef _SNAPSHOT_SYNC_CTX_HXX_
#define _SNAPSHOT_SYNC_CTX_HXX_

namespace cornerstone {
    struct snapshot_sync_ctx {
    public:
        snapshot_sync_ctx(snapshot* s, ulong offset)
            : snapshot_(s), offset_(offset) {}

        ~snapshot_sync_ctx() {
            if (snapshot_ != nilptr) {
                delete snapshot_;
            }
        }

    __nocopy__(snapshot_sync_ctx)
    public:
        snapshot* snapshot_;
        ulong offset_;
    };
}

#endif //_SNAPSHOT_SYNC_CTX_HXX_
