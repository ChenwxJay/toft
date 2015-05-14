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
    std::string name;               ///< 节点名字
    std::string val;                ///< 节点数据
};

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_DATA_STRUCT_H
