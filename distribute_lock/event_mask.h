#ifndef TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H
#define TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H

namespace toft {
namespace distribute_lock {

enum ZKEventMask
{
    EVENT_MASK_NONE                 = 0x0000, ///< 无感兴趣事件
    EVENT_MASK_ATTR_CHANGED         = 0x0001, ///< 属性改变事件
    EVENT_MASK_CHILD_CHANGED        = 0x0002, ///< 孩子节点属性的改变
    EVENT_MASK_LOCK_RELEASE         = 0x0004, ///< 锁失效
    EVENT_MASK_DATA_CHANGED         = 0x0008, ///< 值改变事件
    EVENT_MASK_LOCK_ACQUIRED        = 0x0010, ///< 加锁事件
    EVENT_MASK_SESSION_EXPIRED      = 0x0020, ///< session失效事件
    EVENT_MASK_SESSION_RECONNECTED  = 0x0040, ///< session close and connected
    EVENT_MASK_NODE_EVENT           = 0x0080, ///< 节点增删改事件
};

} // distribute_lock
} // toft

#endif // TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H
