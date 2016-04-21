#ifndef _SRV_ROLE_HXX_
#define _SRV_ROLE_HXX_

namespace cornerstone {
    enum srv_role {
        follower = 0x1,
        candidate,
        leader
    };
}

#endif