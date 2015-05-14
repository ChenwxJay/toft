#ifndef TOFT_DISTRIBUTE_LOCK_EVENT_LISTENER_H
#define TOFT_DISTRIBUTE_LOCK_EVENT_LISTENER_H

#include <string>
#include "thirdparty/glog/logging.h"

namespace toft {
namespace distribute_lock {

class EventListener{
public:
    EventListener(){}
    virtual ~EventListener(){}

public:
    /// @brief  node的数据被更改事件
    /// @param  node - 结点路径
    virtual void AttrChange(const std::string & node, const std::string& attr)
    {
        LOG(INFO) << "node's attr changed. node = " << node << " , attr = " << attr;
        google::FlushLogFiles(0);
    }

    /// @brief 结点删除事件
    /// @param node -  节点名
    virtual void NodeDel(const std::string & node)
    {
    }

    /// @brief 节点数据更改事件
    /// @param node 节点名
    virtual void DataChange(const std::string& node, const std::string& value)
    {
    }

    /// @brief 孩子节点发生增删
    /// @param node 节点名
    virtual void ChildEvent(const std::string& node)
    {
    }

    /// @brief 属性更改事件
    /// @param name 节点名称
    /// @param child 属性名称
    virtual void ChildChange(const std::string & node, const std::string & child)
    {
        LOG(INFO) << "child add : node = " << node << " child = " << child;
        google::FlushLogFiles(0);
    }

    /// @brief  申请锁事件
    virtual void LockAcquired(const std::string & node)
    {
        LOG(INFO) << "node acquired lock. node = " << node;
        google::FlushLogFiles(0);
    }

    /// @brief 释放锁事件
    virtual void LockReleased(const std::string & node)
    {
        LOG(INFO) << "node lose lock. node = " << node;
        google::FlushLogFiles(0);
    }

    virtual void NodeCreate(const std::string &node)
    {
    }

    virtual void SessionExpired()
    {
        LOG(INFO) << "session expired";
        google::FlushLogFiles(0);
    }

    virtual void SessionReConnected()
    {
        LOG(INFO) << "session re-connected";
        google::FlushLogFiles(0);
    }
};

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_EVENT_LISTENER_H
