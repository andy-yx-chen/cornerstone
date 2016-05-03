#include "cornerstone.hxx"

using namespace cornerstone;

snapshot* snapshot::deserialize(buffer& buf) {
    ulong last_log_idx = buf.get_ulong();
    ulong last_log_term = buf.get_ulong();
    ulong size = buf.get_ulong();
    std::shared_ptr<cluster_config> conf(cluster_config::deserialize(buf));
    return new snapshot(last_log_idx, last_log_term, conf, size);
}

buffer* snapshot::serialize() {
    buffer::safe_buffer conf_buf(std::move(buffer::make_safe(last_config_->serialize())));
    buffer* buf = buffer::alloc(conf_buf->size() + sz_ulong * 3);
    buf->put(last_log_idx_);
    buf->put(last_log_term_);
    buf->put(size_);
    buf->put(*conf_buf);
    buf->pos(0);
    return buf;
}