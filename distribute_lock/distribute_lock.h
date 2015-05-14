#ifndef TOFT_DISTRIBUTE_LOCK_DISTRIBUTE_LOCK_H
#define TOFT_DISTRIBUTE_LOCK_DISTRIBUTE_LOCK_H

#include <string>
#include <vector>

#include "thirdparty/zookeeper/zookeeper.h"

#include "toft/system/threading/event.h"
#include "toft/system/threading/mutex.h"

// (for $(root)release/... only), use current dir
#include "toft/distribute_lock/data_struct.h"
#include "toft/distribute_lock/event_listener.h"
#include "toft/distribute_lock/event_mask.h"

// 对于del 和set的时间不予考虑

namespace toft {
namespace distribute_lock {

enum DELT_FLAG
{
    kNotDelt = 0,  // 没有处理
    kDelt,         // 处理过
    kError         // 出错
};

class DistributeLock {
public:
    DistributeLock();
    virtual ~DistributeLock();

public:
    static const int kBuffLen = 1 << 20;
    static const int kMaxLen  = 1000;
    static const int kRetry    = 3;

public:
    /// @brief 建立一个连接zookeeper的handle
    /// @param host - zookeeper服务器的地址族 e.g"127.0.0.1:30000, 127.0.0.1:30031"
    /// @param time_out - 超时时间
    /// @param listener - 监听器
    /// @param log_path - log文件路径
    /// @param is_set_acl - Wether set acl
    /// @param auth_str - the authenticate string, in form of "username:password"
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int Init(
        const std::string& host,
        EventListener* event_listener,
        int time_out,
        std::string log_path = "",
        bool is_set_acl = false,
        std::string auth_str = "",
        ZooLogLevel log_level = ZOO_LOG_LEVEL_INFO
    );

    int Init(clientid_t* clientid,
             const std::string& host,
             EventListener* event_listener,
             int time_out,
             std::string log_path = "",
             bool is_set_acl = false,
             std::string auth_str = "",
             ZooLogLevel log_level = ZOO_LOG_LEVEL_INFO
    );

    /// @brief 关闭zk
    void Close();

    /// @brief 返回zk的句柄
    zhandle_t* GetZKHandle()
    {
        return m_handle;
    }

    /// @brief  创建一个目录
    /// @param  node - 目录名称
    /// @param  event_mask - 设置对这个dir的感兴趣事件
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int MkDir(const std::string& dir, ZKEventMask event_mask = EVENT_MASK_NONE);

    /// @brief  创建一个结点
    /// @param  path - 结点名称
    /// @param  event_mask - 设置对这个结点的感兴趣事件
    /// @param  is_temporary - 是否是临时节点
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int CreateNode(const std::string& path, ZKEventMask event_mask = EVENT_MASK_NONE,
                    bool is_temporary = false);

    /// @brief 带数据的create
    int CreateNodeWithVal(const std::string& path, ZKEventMask event_mask,
                          const char* val_buff, int len, bool is_temporary = false);

    /// @brief  试图去锁一个节点
    /// @param  node - 节点名称
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int Lock(const std::string& node, bool blocked_type = true);

    /// @brief  判断某个节点是否有锁
    /// @param  node - 结点名称
    /// @retval true - 有 false - 无
    bool IsLocked(const std::string& node);

    /// @brief  对结点解锁
    /// @param  node - 节点名称
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int UnLock(const std::string& node);

    /// @brief  设置结点的属性
    /// @param  node - 结点名称
    /// @param  attr - 属性名称
    /// @param  value - 设置的属性值
    /// @param  event_mask - 设置对这个属性的关注事件
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int SetAttr(const std::string& node, const std::string& attr,
                const std::string& value = "",
                int version = -1,
                ZKEventMask event_mask = EVENT_MASK_NONE);

    /// @brief 设置结点的值
    ///
    /// @param node - 节点名称
    /// @param value - 设置的值
    /// @param version - 版本号
    ///
    /// @retval 0 - 成功 其它 - 失败
    int ZooSet(const std::string& node, const std::string& value, int version = -1);

    /// @brief 获得结点的值
    ///
    /// @param node - 结点名称
    /// @param value - 获得的值
    /// @param version - 获得的版本号
    ///
    /// @retval 0 - 成功 其他 - 失败
    int ZooGet(const std::string& node, std::string& value, int* version = NULL);

    /// @brief 判断是否是lock属性结点
    /// @param node - 属性结点
    /// @retval 0 - 是 -1 - 不是
    int IsLockAttr(const std::string& node);

    /// @brief  获得结点的属性
    /// @param  node - 结点名称
    /// @param  attr - 属性名称
    /// @param  value - 获得的属性值
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int GetAttr(const std::string& node, const std::string& attr, std::string& value,
                int* version = NULL);

    /// @brief  删除结点
    /// @param  node - 待删除结点的名称
    /// @retval 0 - 删除成功 -1 - 失败
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int Unlink(const std::string& node);

    /// @brief  判断文件是否存在
    /// @param  node - 节点路径
    /// @retval 0 - 存在， 其他 - 失败
    int Exists(const std::string& node);


    /// @brief 遍历目录下有多少个结点
    /// @param dir - 目录名
    /// @param data - 结点名称
    /// @retval 0   - 成功
    ///        其他 - 失败
    int ListNode(const std::string& dir, std::vector<std::string>& data);

    /// @brief 遍历目录下的属性结点
    /// @param dir  - 目录名
    /// @param attr - 属性
    /// @retval 0 - 成功
    ///        -1 - 失败
    int ListAttr(const std::string& dir, std::vector<struct Attr>& attr);


    /// @brief  设置监听器
    /// @param  node  需要监听的节点
    /// @param  event_mask 事件掩码
    /// @retval 0 - 成功， 其他 - 失败(可通过ErrorString打印错误码)
    int SetWatcher(const std::string& node, ZKEventMask event_mask);

    /// @brief session失效时的处理
    void OnSessionExpired();

    void OnSessionReConnected();

    AutoResetEvent& GetSyncEvent()      { return m_sync_event;}
    AutoResetEvent& GetWaitLockSyncEvent() {return m_wait_lock;}
    AutoResetEvent& GetInitFinishEvent() { return m_init_finish_event; }

    bool IsWaitInit() { return m_is_wait_init; }
    bool IsClosed() { return m_is_closed; }

    void SetZookeeperACLStatus(int status) { m_zk_acl_status = status; }

    int GetInitStatus() { return m_init_status; }

    std::string GetSessionID()              { return m_session_str;}

    virtual void OnNodeChanged(const std::string& path, int state);
    virtual void OnNodeDeleted(const std::string& path, int state);
    virtual void OnChildNode(const std::string& path, int state);
    virtual void OnNodeCreate(const std::string& path, int state);

private:
    int GetBaseName(const std::string& src, std::string &dst);

    int GetPathName(const std::string& src, std::string &dst);

    /// @brief  判断给定的节点是否是属性节点
    /// @param  node - 节点名称
    /// @retval true - 是 ， false - 否
    bool IsAttrNode(const std::string& node);

    /// @brief  去掉属性节点前面的zoo_attr_
    void GetPureAttrName(std::string* node);

    int ReleaseLock();

    /// @brief 设置标记，标记为session id
    /// @param node - 处理的结点
    void SetProcessed(const std::string& node);

    /// @brief 获取处理的标记
    /// @param node - 待判断的结点
    /// @retval kDelt - 处理过, kNotDelt - 没有处理, kError - 错误
    DELT_FLAG GetProcessedFlag(const std::string& node);

    // Lock-free, should be protected by caller.
    int InitZookeeperAcl(int time_out, std::string);

private:
    EventListener*  m_listener;       ///< 客户端传进来的事件监听器
    uint32_t        m_mask;           ///< 关注事件的掩码

private:
    zhandle_t*      m_handle;         ///< 连接zookeeper的handle
    bool            m_is_wait_init;   ///< 是否正在等待init
    bool            m_is_closed;      ///< whether has closed.

public:
    RecursiveMutex  m_mutex;
    AutoResetEvent       m_sync_event;     ///< 同步消息器
    AutoResetEvent       m_init_finish_event;   ///< The init is finished
    AutoResetEvent       m_wait_lock;
    int             m_zk_acl_status;  ///< status of set zk ACL
    int             m_init_status;    ///< status for init
    std::string     m_session_str;    ///< 对应的session
    std::string     m_host;           ///< 初始化的zk的路径
    int64_t         m_processed_time; ///< 当前处理节点的最早时间
    char *          m_buff;           ///< 用以获得数据的buff, 最大为1M
};

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_DISTRIBUTE_LOCK_H
