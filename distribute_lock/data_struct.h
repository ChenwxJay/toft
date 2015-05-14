#ifndef TOFT_DISTRIBUTE_LOCK_DATA_STRUCT_H
#define TOFT_DISTRIBUTE_LOCK_DATA_STRUCT_H

#include <string>

namespace toft {
namespace distribute_lock {

struct Attr
{
    Attr()
    {
        name    = "";
        val     = "";
    }
    std::string name;               ///< �ڵ�����
    std::string val;                ///< �ڵ�����
};

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_DATA_STRUCT_H
