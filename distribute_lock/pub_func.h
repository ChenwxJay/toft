#ifndef TOFT_DISTRIBUTE_LOCK_PUB_FUNC_H
#define TOFT_DISTRIBUTE_LOCK_PUB_FUNC_H

#include "toft/distribute_lock/distribute_lock.h"
#include "toft/distribute_lock/error_code.h"

namespace toft {
namespace distribute_lock {

void Watcher(zhandle_t *handle, int type, int state, const char *path, void *context);
void LockWatcher(zhandle_t *zh, int type, int state, const char *path, void * wathcherCtx);
int Vstrcmp(const void* str1, const void* str2);
void SortChildren(struct String_vector *vectorst);

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_PUB_FUNC_H
