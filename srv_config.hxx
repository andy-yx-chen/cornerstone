#ifndef _SRV_CONFIG_HXX_
#define _SRV_CONFIG_HXX_

namespace cornerstone {
    class srv_config {
    public:
        srv_config(int32 id, const std::string& endpoint)
            : id_(id), endpoint_(endpoint) {}

        __nocopy__(srv_config)

    public:
        static ptr<srv_config> deserialize(buffer& buf);

        int32 get_id() const {
            return id_;
        }

        const std::string& get_endpoint() const {
            return endpoint_;
        }

        ptr<buffer> serialize() const;
    private:
        int32 id_;
        std::string endpoint_;
    };
}

#endif