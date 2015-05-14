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

// ����del ��set��ʱ�䲻�迼��

namespace toft {
namespace distribute_lock {

enum DELT_FLAG
{
    kNotDelt = 0,  // û�д���
    kDelt,         // �����
    kError         // ����
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
    /// @brief ����һ������zookeeper��handle
    /// @param host - zookeeper�������ĵ�ַ�� e.g"127.0.0.1:30000, 127.0.0.1:30031"
    /// @param time_out - ��ʱʱ��
    /// @param listener - ������
    /// @param log_path - log�ļ�·��
    /// @param is_set_acl - Wether set acl
    /// @param auth_str - the authenticate string, in form of "username:password"
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
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

    /// @brief �ر�zk
    void Close();

    /// @brief ����zk�ľ��
    zhandle_t* GetZKHandle()
    {
        return m_handle;
    }

    /// @brief  ����һ��Ŀ¼
    /// @param  node - Ŀ¼����
    /// @param  event_mask - ���ö����dir�ĸ���Ȥ�¼�
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int MkDir(const std::string& dir, ZKEventMask event_mask = EVENT_MASK_NONE);

    /// @brief  ����һ�����
    /// @param  path - �������
    /// @param  event_mask - ���ö�������ĸ���Ȥ�¼�
    /// @param  is_temporary - �Ƿ�����ʱ�ڵ�
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int CreateNode(const std::string& path, ZKEventMask event_mask = EVENT_MASK_NONE,
                    bool is_temporary = false);

    /// @brief �����ݵ�create
    int CreateNodeWithVal(const std::string& path, ZKEventMask event_mask,
                          const char* val_buff, int len, bool is_temporary = false);

    /// @brief  ��ͼȥ��һ���ڵ�
    /// @param  node - �ڵ�����
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int Lock(const std::string& node, bool blocked_type = true);

    /// @brief  �ж�ĳ���ڵ��Ƿ�����
    /// @param  node - �������
    /// @retval true - �� false - ��
    bool IsLocked(const std::string& node);

    /// @brief  �Խ�����
    /// @param  node - �ڵ�����
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int UnLock(const std::string& node);

    /// @brief  ���ý�������
    /// @param  node - �������
    /// @param  attr - ��������
    /// @param  value - ���õ�����ֵ
    /// @param  event_mask - ���ö�������ԵĹ�ע�¼�
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int SetAttr(const std::string& node, const std::string& attr,
                const std::string& value = "",
                int version = -1,
                ZKEventMask event_mask = EVENT_MASK_NONE);

    /// @brief ���ý���ֵ
    ///
    /// @param node - �ڵ�����
    /// @param value - ���õ�ֵ
    /// @param version - �汾��
    ///
    /// @retval 0 - �ɹ� ���� - ʧ��
    int ZooSet(const std::string& node, const std::string& value, int version = -1);

    /// @brief ��ý���ֵ
    ///
    /// @param node - �������
    /// @param value - ��õ�ֵ
    /// @param version - ��õİ汾��
    ///
    /// @retval 0 - �ɹ� ���� - ʧ��
    int ZooGet(const std::string& node, std::string& value, int* version = NULL);

    /// @brief �ж��Ƿ���lock���Խ��
    /// @param node - ���Խ��
    /// @retval 0 - �� -1 - ����
    int IsLockAttr(const std::string& node);

    /// @brief  ��ý�������
    /// @param  node - �������
    /// @param  attr - ��������
    /// @param  value - ��õ�����ֵ
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int GetAttr(const std::string& node, const std::string& attr, std::string& value,
                int* version = NULL);

    /// @brief  ɾ�����
    /// @param  node - ��ɾ����������
    /// @retval 0 - ɾ���ɹ� -1 - ʧ��
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int Unlink(const std::string& node);

    /// @brief  �ж��ļ��Ƿ����
    /// @param  node - �ڵ�·��
    /// @retval 0 - ���ڣ� ���� - ʧ��
    int Exists(const std::string& node);


    /// @brief ����Ŀ¼���ж��ٸ����
    /// @param dir - Ŀ¼��
    /// @param data - �������
    /// @retval 0   - �ɹ�
    ///        ���� - ʧ��
    int ListNode(const std::string& dir, std::vector<std::string>& data);

    /// @brief ����Ŀ¼�µ����Խ��
    /// @param dir  - Ŀ¼��
    /// @param attr - ����
    /// @retval 0 - �ɹ�
    ///        -1 - ʧ��
    int ListAttr(const std::string& dir, std::vector<struct Attr>& attr);


    /// @brief  ���ü�����
    /// @param  node  ��Ҫ�����Ľڵ�
    /// @param  event_mask �¼�����
    /// @retval 0 - �ɹ��� ���� - ʧ��(��ͨ��ErrorString��ӡ������)
    int SetWatcher(const std::string& node, ZKEventMask event_mask);

    /// @brief sessionʧЧʱ�Ĵ���
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

    /// @brief  �жϸ����Ľڵ��Ƿ������Խڵ�
    /// @param  node - �ڵ�����
    /// @retval true - �� �� false - ��
    bool IsAttrNode(const std::string& node);

    /// @brief  ȥ�����Խڵ�ǰ���zoo_attr_
    void GetPureAttrName(std::string* node);

    int ReleaseLock();

    /// @brief ���ñ�ǣ����Ϊsession id
    /// @param node - ����Ľ��
    void SetProcessed(const std::string& node);

    /// @brief ��ȡ����ı��
    /// @param node - ���жϵĽ��
    /// @retval kDelt - �����, kNotDelt - û�д���, kError - ����
    DELT_FLAG GetProcessedFlag(const std::string& node);

    // Lock-free, should be protected by caller.
    int InitZookeeperAcl(int time_out, std::string);

private:
    EventListener*  m_listener;       ///< �ͻ��˴��������¼�������
    uint32_t        m_mask;           ///< ��ע�¼�������

private:
    zhandle_t*      m_handle;         ///< ����zookeeper��handle
    bool            m_is_wait_init;   ///< �Ƿ����ڵȴ�init
    bool            m_is_closed;      ///< whether has closed.

public:
    RecursiveMutex  m_mutex;
    AutoResetEvent       m_sync_event;     ///< ͬ����Ϣ��
    AutoResetEvent       m_init_finish_event;   ///< The init is finished
    AutoResetEvent       m_wait_lock;
    int             m_zk_acl_status;  ///< status of set zk ACL
    int             m_init_status;    ///< status for init
    std::string     m_session_str;    ///< ��Ӧ��session
    std::string     m_host;           ///< ��ʼ����zk��·��
    int64_t         m_processed_time; ///< ��ǰ����ڵ������ʱ��
    char *          m_buff;           ///< ���Ի�����ݵ�buff, ���Ϊ1M
};

} // namespace distribute_lock
} // namespace toft

#endif // TOFT_DISTRIBUTE_LOCK_DISTRIBUTE_LOCK_H
