#ifndef _RAFT_PARAMS_HXX_
#define _RAFT_PARAMS_HXX_

namespace cornerstone {
    struct raft_params {
    public:
        raft_params()
            : election_timeout_upper_bound_(300),
            election_timeout_lower_bound_(150),
            heart_beat_interval_(75),
            rpc_failure_backoff_(25),
            log_sync_batch_size_(1000),
            log_sync_stop_gap_(10),
            snapshot_distance_(0),
            snapshot_block_size_(0) {}
        raft_params(int32 et_upper, int32 et_lower, int32 hb_interval, int32 backoff, int32 sync_batch, int32 sync_gap, int32 snapshot_dist, int32 snapshot_block)
            : election_timeout_upper_bound_(et_upper),
            election_timeout_lower_bound_(et_lower),
            heart_beat_interval_(hb_interval),
            rpc_failure_backoff_(backoff),
            log_sync_batch_size_(sync_batch),
            log_sync_stop_gap_(sync_gap),
            snapshot_distance_(snapshot_dist),
            snapshot_block_size_(snapshot_block) {}

        __nocopy__(raft_params)
    public:
        int max_hb_interval() const {
            return std::max(heart_beat_interval_, election_timeout_lower_bound_ - (heart_beat_interval_ / 2));
        }
    public:
        int32 election_timeout_upper_bound_;
        int32 election_timeout_lower_bound_;
        int32 heart_beat_interval_;
        int32 rpc_failure_backoff_;
        int32 log_sync_batch_size_;
        int32 log_sync_stop_gap_;
        int32 snapshot_distance_;
        int32 snapshot_block_size_;
    };
}

#endif //_RAFT_PARAMS_HXX_