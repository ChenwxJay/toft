#ifndef TOFT_DISTRIBUTE_LOCK_ERROR_CODE_H
#define TOFT_DISTRIBUTE_LOCK_ERROR_CODE_H

namespace toft {
namespace distribute_lock {

const char* StateString(int state);
const char* TypeString(int type);
const char* ErrorString(int ret);

} // namespace distribute_lock
} // toft

#endif // TOFT_DISTRIBUTE_LOCK_ERROR_CODE_H
