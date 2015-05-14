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
    /// @brief  node�����ݱ������¼�
    /// @param  node - ���·��
    virtual void AttrChange(const std::string & node, const std::string& attr)
    {
        LOG(INFO) << "node's attr changed. node = " << node << " , attr = " << attr;
        google::FlushLogFiles(0);
    }

    /// @brief ���ɾ���¼�
    /// @param node -  �ڵ���
    virtual void NodeDel(const std::string & node)
    {
    }

    /// @brief �ڵ����ݸ����¼�
    /// @param node �ڵ���
    virtual void DataChange(const std::string& node, const std::string& value)
    {
    }

    /// @brief ���ӽڵ㷢����ɾ
    /// @param node �ڵ���
    virtual void ChildEvent(const std::string& node)
    {
    }

    /// @brief ���Ը����¼�
    /// @param name �ڵ�����
    /// @param child ��������
    virtual void ChildChange(const std::string & node, const std::string & child)
    {
        LOG(INFO) << "child add : node = " << node << " child = " << child;
        google::FlushLogFiles(0);
    }

    /// @brief  �������¼�
    virtual void LockAcquired(const std::string & node)
    {
        LOG(INFO) << "node acquired lock. node = " << node;
        google::FlushLogFiles(0);
    }

    /// @brief �ͷ����¼�
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
