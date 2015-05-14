#include "toft/distribute_lock/error_code.h"
#include "thirdparty/zookeeper/zookeeper.h"

namespace toft {
namespace distribute_lock {

const char* StateString(int state)
{
    if (state == 0)
        return "kStatusClosedState";
    if (state == ZOO_CONNECTING_STATE)
        return "kStatusConnectingState";
    if (state == ZOO_ASSOCIATING_STATE)
        return "kStatusAssociatingState";
    if (state == ZOO_CONNECTED_STATE)
        return "kStatusConnectedState";
    if (state == ZOO_EXPIRED_SESSION_STATE)
        return "kStatusExpiredSessionState";
    if (state == ZOO_AUTH_FAILED_STATE)
        return "kStatusAuthFailedState";

    return "kStateInvalidState";
}

const char* TypeString(int type)
{
    if (type == 1)
        return "CREATED_EVENT_DEF";
    if (type == 2)
        return "DELETED_EVENT_DEF";
    if (type == 3)
        return "CHANGED_EVENT_DEF";
    if (type == 4)
        return "CHILD_EVENT_DEF";
    if (type == -1)
        return "SESSION_EVENT_DEF";
    if (type == -2)
        return "NOTWATCHING_EVENT_DEF";

    return "INVALID_TYPE";
}

const char* ErrorString(int ret)
{
    return zerror(ret);
}

} // namespace distribute_lock
} // namespace toft
