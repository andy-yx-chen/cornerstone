#include "cornerstone.hxx"

using namespace cornerstone;

ptr<buffer> cluster_config::serialize() {
    size_t sz = 2 * sz_ulong + sz_int;
    std::vector<ptr<buffer>> srv_buffs;
    for (cluster_config::const_srv_itor it = servers_.begin(); it != servers_.end(); ++it) {
        ptr<buffer> buf = (*it)->serialize();
        srv_buffs.push_back(buf);
        sz += buf->size();
    }

    ptr<buffer> result = buffer::alloc(sz);
    result->put(log_idx_);
    result->put(prev_log_idx_);
    result->put((int32)servers_.size());
    for (size_t i = 0; i < srv_buffs.size(); ++i) {
        result->put(*srv_buffs[i]);
    }

    result->pos(0);
    return result;
}

ptr<cluster_config> cluster_config::deserialize(buffer& buf) {
    ulong log_idx = buf.get_ulong();
    ulong prev_log_idx = buf.get_ulong();
    int32 cnt = buf.get_int();
    ptr<cluster_config> conf = cs_new<cluster_config>(log_idx, prev_log_idx);
    while (cnt -- > 0) {
        conf->get_servers().push_back(srv_config::deserialize(buf));
    }

    return conf;
}