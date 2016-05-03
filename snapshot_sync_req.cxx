#include "cornerstone.hxx"

using namespace cornerstone;

snapshot_sync_req* snapshot_sync_req::deserialize(buffer& buf) {
    std::shared_ptr<snapshot> snp(snapshot::deserialize(buf));
    ulong offset = buf.get_ulong();
    bool done = buf.get_byte() == 1;
    byte* src = buf.data();
    buffer* b;
    if (buf.pos() < buf.size()) {
        size_t sz = buf.size() - buf.pos();
        b = buffer::alloc(sz);
        ::memcpy(b->data(), src, sz);
    }
    else {
        b = buffer::alloc(0);
    }

    return new snapshot_sync_req(snp, offset, b, done);
}

buffer* snapshot_sync_req::serialize() {
    buffer::safe_buffer sbuf(buffer::make_safe(snapshot_->serialize()));
    buffer* buf = buffer::alloc(sbuf->size() + sz_ulong + sz_byte + data_->size());
    buf->put(*sbuf);
    buf->put(offset_);
    buf->put(done_ ? (byte)1 : (byte)0);
    buf->put(*data_);
    return buf;
}