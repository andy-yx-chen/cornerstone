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
        raft_params(int et_upper, int et_lower, int hb_interval, int backoff, int sync_batch, int sync_gap, int snapshot_dist, int snapshot_block)
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
        int election_timeout_upper_bound_;
        int election_timeout_lower_bound_;
        int heart_beat_interval_;
        int rpc_failure_backoff_;
        int log_sync_batch_size_;
        int log_sync_stop_gap_;
        int snapshot_distance_;
        int snapshot_block_size_;
    };
}

#endif //_RAFT_PARAMS_HXX_