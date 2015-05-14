#include <string>

#include "toft/system/time/clock.h"
#include "thirdparty/glog/logging.h"
#include "thirdparty/glog/raw_logging.h"

#include "toft/distribute_lock/error_code.h"
#include "toft/distribute_lock/pub_func.h"

namespace toft {
namespace distribute_lock {

DistributeLock::DistributeLock()
{
    m_handle      = NULL;
    m_listener    = NULL;
    m_buff        = new char[kBuffLen];
    m_is_wait_init = false;
    m_is_closed = false;
}

DistributeLock::~DistributeLock()
{
    Close();
    LOG(INFO) << "Distribute lock destroyed.";
    if (m_buff != NULL) {
        delete []m_buff;
        m_buff = NULL;
    }
}

void DistributeLock::Close()
{
//    MutexLocker locker(&m_mutex);
    if (m_handle != NULL) {
        VLOG(1) << "to close distribute lock handle " << m_handle
            << " for session " << m_session_str;
        zookeeper_close(m_handle);
        m_handle = NULL;
        m_listener = NULL;
    }
    m_is_closed = true;
}

int DistributeLock::Init(
        const std::string& host,
        EventListener* listener,
        int time_out,
        std::string log_path,
        bool is_set_acl,
        std::string auth_str,
        ZooLogLevel log_level)
{
    return Init(NULL, host, listener, time_out, log_path, is_set_acl, auth_str, log_level);
}

int DistributeLock::Init(clientid_t* clientid,
        const std::string& host,
        EventListener* listener,
        int time_out,
        std::string log_path,
        bool is_set_acl,
        std::string auth_str,
        ZooLogLevel log_level)
{
    MutexLocker locker(&m_mutex);

    m_init_status = ZSYSTEMERROR;

    m_host = host;
    zoo_deterministic_conn_order(0);   // 随机选择一个zookeeper服务器

    // zhandle_t* previous_handle = m_handle;

    m_handle = NULL;
    VLOG(1) << "reset zkhandle and begin to init zk";

    m_is_wait_init = true;
    m_sync_event.Reset();

    int64_t pre_init = RealtimeClock.MilliSeconds();
    m_handle = zookeeper_init(m_host.c_str(), Watcher, 0.8*time_out, clientid, this, 0);
    int64_t post_init = RealtimeClock.MilliSeconds();

    if (!m_handle) {
        m_is_wait_init = false;
        LOG(ERROR) << "zookeeper init failed. errno = " << errno << ", error code = " <<
                strerror(errno);
        m_init_status = ZSYSTEMERROR;
        m_init_finish_event.Set();
        return m_init_status;
    }
    VLOG(1) << "try to wait for init process, handle = " << m_handle;

    bool ret = m_sync_event.TimedWait(time_out);
    int64_t post_wait = RealtimeClock.MilliSeconds();

    m_is_wait_init = false;
    if (!ret) // time_out
    {
        VLOG(1) << "init failed. time out";
        m_init_status = ZOPERATIONTIMEOUT;
        m_init_finish_event.Set();
        return m_init_status;
    }

    int init_state = zoo_state(m_handle);
    if (init_state != ZOO_CONNECTED_STATE) {
        // TODO(aaronzou): better policy to deal with kStatusConnectingState
        // and kStatusAssociatingState
        LOG(ERROR) << "Init failed with wrong ZK state "
            << std::string(StateString(init_state));
        m_init_status = ZINVALIDSTATE;
        m_init_finish_event.Set();
        return m_init_status;
    }


    const clientid_t* cid = zoo_client_id(m_handle);
    int64_t session = cid->client_id;
    char prefix[30];
#if defined(__x86_64__)
    snprintf(prefix, sizeof(prefix), "%016lx", session);
#else
    snprintf(prefix, sizeof(prefix), "%016llx", session);
#endif
    m_session_str = std::string(prefix, strlen(prefix));

    if (is_set_acl) {
        m_init_status = InitZookeeperAcl(time_out, auth_str);
        if (m_init_status != ZOK) {
            m_init_finish_event.Set();
            return m_init_status;
        }
    }

    if (listener != NULL) {
        m_listener = listener;
    }
    int t_out = zoo_recv_timeout(m_handle);

    // Close previous handle to avoid dangering handle point to old session.
    // When have multiple handles, the result is hard to predicate.
    // if (previous_handle != NULL) {
    //    VLOG(1) << "close previous_handle " << previous_handle << " to avoid leak handles";
    //    zookeeper_close(previous_handle);
    //    previous_handle = NULL;
    // }

    LOG(INFO) << "zookeeper init success. session = " << m_session_str << " time_out = " << t_out;

    m_is_closed = false;
    m_init_status = ZOK;
    m_init_finish_event.Set();

    if (post_wait - pre_init > time_out) {
        LOG(ERROR) << "slow zookeeper init, cost "
            << (post_init - pre_init) << " milli-seconds to call zoo_init, "
            << (post_wait - post_init) << " milli-seconds to wait init callback.";
    }

    return m_init_status;
}


int DistributeLock::InitZookeeperAcl(int time_out, std::string zookeeper_auth_str) {
    m_zk_acl_status = ZSYSTEMERROR;
#ifndef _WIN32

    // Sync function, don't care callback.
    const int32_t add_ret = zoo_add_auth(m_handle, "digest",
                                         zookeeper_auth_str.c_str(),
                                         zookeeper_auth_str.length(),
                                         0, 0);
    m_zk_acl_status = add_ret;
    if (m_zk_acl_status != ZOK) {
        // 调用zoo_add_auth失败
        LOG(ERROR) << "add auth failed, error_code = "
                   << std::string(distribute_lock::ErrorString(add_ret));
    } else {
        VLOG(1) << "Init zookeeper acl OK";
    }

    return m_zk_acl_status;
#else
    return m_zk_acl_status;
#endif
}

int DistributeLock::CreateNode(const std::string& path, ZKEventMask event_mask, bool is_temporary)
{

    return CreateNodeWithVal(path, event_mask, NULL, -1, is_temporary);
}

int DistributeLock::CreateNodeWithVal(const std::string& path, ZKEventMask event_mask,
        const char* val_buff, int len, bool is_temporary)
{
    int node_mask;
    MutexLocker locker(&m_mutex);
    if (is_temporary)
        node_mask = ZOO_EPHEMERAL;
    else
        node_mask = 0;
    int ret = zoo_create(
            m_handle,                   //  zookeeper句柄
            path.c_str(),               //  node的名字
            val_buff,                   //  写在node中的数据
            len,                        //  写在node中的数据的长度
            &ZOO_OPEN_ACL_UNSAFE,       //  ACL
            node_mask,                  //  节点类型
            m_buff,
            kMaxLen
    );
    if (ret == ZOK ||  ret == ZNODEEXISTS)
    {
        SetWatcher(path, event_mask);
        ret = ZOK;
    }
    else
    {
        LOG(ERROR) << "create node failed. error code = " << ErrorString(ret);
    }
    return ret;
}

int DistributeLock::MkDir(const std::string& dir, ZKEventMask event_mask)
{
    // 创建的是永久性节点
    return CreateNode(dir, event_mask);
}

int DistributeLock::Lock(const std::string& node, bool blocked_type)
{
    std::string node_path = m_host + node;
    // test whether the node exists
    std::string path = node + "/zoo_attr_lock"; // 在lock属性下创建子节点
    int exists = -1;
    {
        MutexLocker locker(&m_mutex);
        if (m_handle == NULL)
        {
            VLOG(3) << "handle maybe closed.";
            return -1;
        }
        exists =  zoo_exists(m_handle, path.c_str(), 0, NULL);
    }
    int ret, idx, retry_cnt = 0;
    bool locked = false;

    // 在使用阻塞式lock时，为了避免长期持有mutex，所有用临时buff代替成员buff
    char buff[kMaxLen], retbuf[kMaxLen];

    while ((exists == ZCONNECTIONLOSS || exists == ZNONODE) && retry_cnt < kRetry)
    {
        ++retry_cnt;
        if (exists == ZCONNECTIONLOSS)  // retry
        {
            MutexLocker locker(&m_mutex);
            if (m_handle == NULL)
            {
                VLOG(3) << "handle maybe closed";
                return -1;
            }
            exists = zoo_exists(m_handle, node.c_str(), 0, NULL);
        }
        else if (exists == ZNONODE) // lock应该是正常节点
        {
            {
                MutexLocker locker(&m_mutex);
                if (m_handle == NULL)
                {
                    VLOG(3) << "handle maybe closed";
                    return -1;
                }
                ret = zoo_create(m_handle, path.c_str(), NULL, -1, &ZOO_OPEN_ACL_UNSAFE,
                        0, buff, kMaxLen);
            }
            if (ret != ZOK && ret != ZNODEEXISTS)
            {
                VLOG(3) <<  "create lock node failed, node =  " << node_path
                    << ". error code = " << ErrorString(ret);
                return ret;
            }
        }
    }

    if (exists == ZCONNECTIONLOSS)
    {
        VLOG(3) << "cann't access the server.";
        return exists;
    }

    //  获得lock的子节点
    struct String_vector  vectorst = {0, NULL};
    struct String_vector  vec_children = {0, NULL};
    struct Stat stat;
    do
    {
        {
            MutexLocker locker(&m_mutex);
            if (m_handle == NULL)
            {
                VLOG(3) << "handle maybe closed";
                return -1;
            }
            ret = zoo_get_children(m_handle, path.c_str(), 0, &vectorst);
        }
        if (ret != ZOK)
        {
            VLOG(3) << "get children operation failed. error code = " << ErrorString(ret);
            deallocate_String_vector(&vectorst); // 释放掉资源
            return ret;
        }
        locked = false;
        char *lock_id;
        std::string lock_full_path;
        lock_id = NULL;
        //  找该session对应的lock-node
        for (idx = 0; idx < vectorst.count; ++idx)
        {
            char * child = vectorst.data[idx];
            if (strncmp(m_session_str.data(), child, m_session_str.length()) == 0) {
                lock_id = strdup(child);
                break;
            }
        }
        if (lock_id == NULL) // 没有创建
        {
            snprintf(buff, kMaxLen, "%s/%s-", path.c_str(), m_session_str.data());
            {
                MutexLocker locker(&m_mutex);
                if (m_handle == NULL)
                {
                    VLOG(3) << "handle maybe closed";
                    return -1;
                }
                ret = zoo_create(m_handle, buff, NULL, -1, &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_EPHEMERAL | ZOO_SEQUENCE, retbuf, kMaxLen);
            }
            if (ret != ZOK)
            {
                VLOG(3) << "can't create zoo node " << std::string(buff)
                    << ". error code = " << ErrorString(ret);
                deallocate_String_vector(&vectorst); // 释放掉资源
                return ret;
            }
            char *name = strrchr(retbuf, '/');
            if (name == NULL)
                lock_id = NULL;
            else
                lock_id = strdup(name + 1); // session_id-ephernum
        }
        deallocate_String_vector(&vectorst); // 释放掉资源

        if (lock_id != NULL)
        {
            {
                MutexLocker locker(&m_mutex);
                if (m_handle == NULL)
                {
                    VLOG(3) << "handle maybe closed";
                    return -1;
                }
                ret = zoo_get_children(m_handle, path.c_str(), 0, &vec_children);
            }
            if (ret != ZOK)
            {
                VLOG(3) << "get children failed. error_code = " << ErrorString(ret);
                deallocate_String_vector(&vec_children); // 释放掉资源
                return ret;
            }
            // 对孩子节点排序
            SortChildren(&vec_children);
            //  查找它的predecessor
            char *less_than_me = NULL;
            for (idx = 0; idx < vec_children.count; ++idx)
            {
                if (strcmp(vec_children.data[idx], lock_id) == 0)
                    break;
                less_than_me = vec_children.data[idx];
            }
            // 释放vectorst, vec_children
            if (less_than_me != NULL) // 有前驱
            {
                // 设置对前驱的监视
                std::string last_child = path + "/" + less_than_me;

                {
                    MutexLocker locker(&m_mutex);
                    if (m_handle == NULL)
                    {
                        VLOG(3) << "handle maybe closed";
                        return -1;
                    }
                    ret = zoo_wexists(m_handle, last_child.c_str(), LockWatcher, this , NULL);
                }
                if (ret != ZOK) // 如果不能设置watcher, 应该give up
                {
                    VLOG(3) << "watch predecessor failed. error_code = " <<
                        ErrorString(ret) << ", path = " << last_child;
                    return ret; // 有可能session 已经失效
                }
                else
                {
                    VLOG(3) << "watch at predecessor " << last_child;
                    if (blocked_type) // 应该等待, 去等待拿到锁
                        m_wait_lock.Wait();
                    else
                    {
                        deallocate_String_vector(&vec_children);
                        return -1;                          // 没有lock上
                    }
                }
            }
            else // 获得锁
            {
                locked = true;
                VLOG(3) << "get the lock : " << std::string(lock_id);
                lock_full_path = node + "/zoo_attr_lock/" + std::string(lock_id);
                VLOG(3) << "lock full path " << lock_full_path;
                // 设置对锁的关注
                {
                    MutexLocker locker(&m_mutex);
                    if (m_handle == NULL)
                    {
                        VLOG(3) << "handle maybe closed";
                        return -1;
                    }
                    ret = zoo_exists(m_handle, lock_full_path.c_str(), 1, &stat);
                }
                if (ret != ZOK)
                {
                    VLOG(3) << "listen on the lock  failed. node = " << lock_full_path <<
                        "error code = " << ErrorString(ret);
                    return ret;
                }
            }
            deallocate_String_vector(&vec_children);
        }
    }while(locked == false);
    return ZOK;
}


bool DistributeLock::IsLocked(const std::string& node)
{
    MutexLocker locker(&m_mutex);
    struct String_vector vec = {0, NULL};
    bool is_locked = false;
    std::string lock_path = node + std::string("/zoo_attr_lock");
    // 判断lock_path是否有效
    int ret = zoo_get_children(m_handle, lock_path.data(), 1, &vec);
    if (ret == ZOK)
    {
        if (vec.count > 0)
        {
            //  说明有lock文件， 是lock上的
            SortChildren(&vec);
            //  监听锁文件
            struct Stat stat;
            std::string lock_file = lock_path + "/" + std::string(vec.data[0]);
            ret = zoo_exists(m_handle, lock_file.data(), 1, &stat);
            if (ret != ZOK)
            {
                VLOG(3) << "set lock exist watch failed. node = " << lock_path
                        << ", error code = " << ErrorString(ret);
                is_locked = false;
            }
            else
            {
                is_locked = true;
                VLOG(3) << "set lock exist watcher success. node = " << lock_path;
            }
        }
        else
        {
            VLOG(3) << node << " is not locked.";
            is_locked = false;
        }
    }
    else
    {
        VLOG(3) <<  "get node's lock children failed. error code = " << ErrorString(ret);
    }

    deallocate_String_vector(&vec);
    return is_locked;
}

int DistributeLock::SetAttr(const std::string& node, const std::string& attr,
        const std::string& value, int version, ZKEventMask event_mask)
{
    MutexLocker locker(&m_mutex);
    std::string node_path = m_host  + node;
    std::string attr_path = node + "/zoo_attr_" + attr;
    int ret = zoo_exists(m_handle, attr_path.c_str(), 0, NULL);
    if (ret == ZOK) {
        ret = zoo_set(m_handle, attr_path.c_str(), value.data(), value.length(), version);
        if (ret == ZOK) {
            //  设置监听器
            VLOG(3) << "node " << node << " set attr " << attr << " success";
            SetWatcher(attr_path, event_mask);
        }
        else
        {
            VLOG(3) << "set attr val failed. node = " << node_path <<
                ", attr = " << attr << ", error_code = " << ErrorString(ret);
        }
        return ret;
    }
    else if (ret == ZNONODE) {
        ret = zoo_create(m_handle, attr_path.c_str(), value.data(), value.length(),
                &ZOO_OPEN_ACL_UNSAFE, 0, m_buff, kMaxLen);
        if (ret == ZOK || ret == ZNODEEXISTS) {
            SetWatcher(attr_path, event_mask);
        }
        else
        {
            VLOG(3) << "set attr val failed. node = " << node_path <<
                ", attr = " << attr << ", error code = " << ErrorString(ret);
        }
        return ret;
    }
    return ret;
}

int DistributeLock::GetAttr(const std::string& node, const std::string& attr,
        std::string& value, int *version)
{
    MutexLocker locker(&m_mutex);
    std::string node_path = m_host + node;
    value = "";
    std::string path = node + "/zoo_attr_" + attr;
    int ret;
    if (attr == "lock") { // 如果是lock属性,则判断是否有孩子结点
        struct String_vector children_path = {0, NULL};
        ret = zoo_get_children(m_handle, path.c_str(), 0, &children_path);
        if (ret == ZOK && children_path.count > 0)
            value = std::string("1");
        else
            value = std::string("0");
        deallocate_String_vector(&children_path);
        return ret;
    }
    int len = kBuffLen;
    struct Stat stat;
    ret = zoo_get(m_handle, path.data(), 0, m_buff, &len, &stat);
    if (ret == ZOK) {
        if (len > kBuffLen) {
            VLOG(3) <<  "too long data length, exceed limits";
            return -1;
        }
        value = std::string(m_buff, len);
        VLOG(3) << "get attr success. node = " << node_path
                << ", val's length = " << value.length();
    }
    if (version != NULL)
    {
        *version = stat.version;
    }
    return ret;
}

int DistributeLock::Unlink(const std::string& node)
{
    MutexLocker locker(&m_mutex);
    struct String_vector children_path = {0, NULL};
    std::string node_path = m_host + node;
    int ret = zoo_get_children(m_handle, node.c_str(), 0, &children_path);
    if (ret == ZOK) { // 如果操作成功
        for (int i = 0; i < children_path.count; ++i) {
            int unlink_ret = Unlink(std::string(node + "/" + std::string(children_path.data[i])));
            if (unlink_ret != ZOK && unlink_ret != ZNONODE)
            {
                deallocate_String_vector(&children_path);
                return unlink_ret;
            }
        }
        ret = zoo_delete(m_handle, node.c_str(), -1);
        if (ret != ZOK && ret != ZNONODE) {
            VLOG(3) << "unlink node failed. " << node_path << ", error code = " <<
                    ErrorString(ret);
        } else {
            VLOG(3) << "unlink node success. node = " << node_path;
        }
    }
    // zoo_get_children - don't return ZNOTEMPTY, so remove code in else branch
    else
    {
        VLOG(3) << "get node's children failed. node = " << node.c_str() <<
            ", error_code = " << ErrorString(ret);
    }
    deallocate_String_vector(&children_path);
    if (ret == ZNONODE) return ZOK;
    return ret;
}


int DistributeLock::SetWatcher(const std::string& node, ZKEventMask event_mask)
{
    MutexLocker locker(&m_mutex);
    std::string node_path = m_host + node;
    int len = kMaxLen;
    struct Stat stat;
    struct String_vector vec = {0, NULL};
    int ret = 0;
    if (event_mask & EVENT_MASK_ATTR_CHANGED)
    {
        deallocate_String_vector(&vec);
        ret = zoo_get_children(m_handle, node.c_str(), 0, &vec);
        if (ret != ZOK)
        {
            LOG(ERROR) << "get child failed. ret = " << ErrorString(ret);
            return ret;
        }
        for (int i = 0; i < vec.count; ++i)
        {
            std::string attr_node = node + "/" + std::string(vec.data[i]);
            if (IsAttrNode(attr_node)) {
                //  只要是属性节点设置关注事件
                ret = zoo_get(m_handle, attr_node.c_str(), 1, m_buff, &len, &stat);
                if (ret != ZOK)
                {
                    LOG(ERROR) << "zoo get failed. ret = " << ErrorString(ret);
                    return ret;
                }
            }
        }
        VLOG(3) << "set attr-changed watcher success. node = " << node_path;
        deallocate_String_vector(&vec);
    }
    if (event_mask & EVENT_MASK_CHILD_CHANGED) { // 如果不等于0, 需要设置响应ZOO_CHILD_EVENT
        ret = zoo_get_children(m_handle, node.data(), 1, &vec);
        VLOG(3) << "set watcher child add: node = " << node_path;
        deallocate_String_vector(&vec);
    }
    if (event_mask & EVENT_MASK_DATA_CHANGED) {
        ret = zoo_get(m_handle, node.c_str(), 1, m_buff, &len, &stat);
        if (ret != ZOK)
        {
            VLOG(3) << "set data change watch on node failed. error code = " <<
                ErrorString(ret);
            return -1;
        }
        else
        {
            VLOG(3) << "set data change watch success. node = " << node_path;
        }
    }
    if (event_mask & EVENT_MASK_NODE_EVENT) {
        ret = zoo_exists(m_handle, node.c_str(), 1, &stat);
        if (ret != ZOK && ret != ZNONODE) {
            VLOG(3) << "set exist watch on node failed. error code = " << ErrorString(ret);
            return ret;
        }
    }
    if ((event_mask & EVENT_MASK_LOCK_ACQUIRED) || (event_mask & EVENT_MASK_LOCK_RELEASE)) {
        std::string lock_path = node + "/zoo_attr_lock";
        ret = Exists(lock_path);
        if (ret == ZNONODE) {
            ret = CreateNode(lock_path);
            if (ret == ZOK) {
                ret = zoo_get_children(m_handle, lock_path.data(), 1, &vec);
                if (ret != ZOK)
                {
                    LOG(ERROR) << "get children failed. ret = " << ErrorString(ret);
                    return ret;
                }
                VLOG(3) << "set child watch on node success. node = " << lock_path;
                deallocate_String_vector(&vec);
            } else {
                VLOG(3) << "create node failed. node = " << lock_path << ", error code = " <<
                    ErrorString(ret);
            }
        }
        else if (ret == ZOK)
        {
            if (event_mask & EVENT_MASK_LOCK_ACQUIRED)
            {
                ret = zoo_get_children(m_handle, lock_path.data(), 1, &vec);
                if (ret != ZOK)
                {
                    LOG(ERROR) << "get child failed. ret = " << ErrorString(ret);
                    return ret;
                }
                deallocate_String_vector(&vec);
                VLOG(3) << "set lock acquired watch on node successi. node = " << lock_path;
            }
            if (event_mask & EVENT_MASK_LOCK_RELEASE) {
                ret = zoo_get_children(m_handle, lock_path.c_str(), 1, &vec);
                SortChildren(&vec);
                if (vec.count > 0) {
                    std::string path = lock_path + "/" + std::string(vec.data[0]);
                    ret = zoo_exists(m_handle, path.c_str(), 1, &stat);
                    if (ret != ZOK)
                    {
                        LOG(ERROR) << "zoo exist set lock failed. path = " << path;
                        return ret;
                    }
                    VLOG(3) << "set watcher on node success. node =  " << path;
                }
                deallocate_String_vector(&vec);
            }
        }
        else
        {
            return ret;
        }
    }
    return  ZOK;
}

void DistributeLock::OnNodeChanged(const std::string& node, int state)
{
    MutexLocker locker(&m_mutex);
    if (m_handle == NULL) return;
    // 继续设置事件
    SetWatcher(node, EVENT_MASK_DATA_CHANGED);
    if (m_listener != NULL) {
        std::string value;
        if (ZooGet(node, value) == ZOK) {
            m_listener->DataChange(node, value);
        }
    }
    VLOG(3) << "node data change, ndoe = " << node;
    if (IsAttrNode(node)) { // 只对属性节点的改变感兴趣
        std::string parent_node, attr_node;
        GetPathName(node, parent_node);
        GetBaseName(node, attr_node);
        if (IsAttrNode(attr_node)) {
            GetPureAttrName(&attr_node);
        }

        if (m_listener != NULL)
        {
            VLOG(3) << "report attr change, node = " << attr_node;
            m_listener->AttrChange(parent_node, attr_node);
        }
    }
}

void DistributeLock::OnNodeDeleted(const std::string& node, int state)
{
    MutexLocker locker(&m_mutex);
    if (m_handle == NULL) return;
    size_t pos = node.find("zoo_attr_lock/"); // 必须是zoo_attr_lock的孩子节点
    if (pos != std::string::npos) {
        std::string preces_node = node.substr(0, pos - 1);
        VLOG(3) <<  "node release lock. node = " << preces_node;
        if (m_listener != NULL) {
            m_listener->LockReleased(preces_node);
        }
    }
    else
    {
        if (m_listener != NULL)
        {
            VLOG(3) << "report node delete. node = " << node;
            m_listener->NodeDel(node);
        }
    }
    VLOG(3) << "node deleted. node = " << node;
}

void DistributeLock::OnChildNode(const std::string& node, int state)
{
    MutexLocker locker(&m_mutex);
    if (m_handle == NULL) return;
    struct String_vector  vec = {0, NULL};
    struct Stat stat;
    // 首先设置孩子结点监听
    int ret = zoo_get_children(m_handle, node.data(), 1, &vec);
    int len = kMaxLen;
    if (ret == ZOK)
    {
        VLOG(3) << "set children watch on node " << node << " success. vec_count = " << vec.count;
    }
    else
    {
        VLOG(3) << "set children watch on node " << node << " failed. error code = " <<
            ErrorString(ret);
    }
    if (m_listener != NULL)
    {
        m_listener->ChildEvent(node);
    }

    if (IsAttrNode(node)) // 只处理lock属性结点
    {
        //  当孩子结点删也会触发该事件
        if (vec.count > 0) // 有孩子结点, 定是加锁, 可直接上报结点加入
        {
            // 设置对锁的监听 (如果这个结点下被lock两次，则会上报两次)
            std::string lock_path = node + "/" + std::string(vec.data[0]);
            ret = zoo_exists(m_handle, lock_path.c_str(), 1, &stat);
            if (ret != ZOK)
            {
                LOG(ERROR) << "zoo exist set lock failed. ret = " << ErrorString(ret);
                return;
            }
            VLOG(3) << "set lock exist watcher on node " << lock_path;
            // 上报获得锁事件(监听这个lock)
            std::string path;
            GetPathName(node, path);
            // mark
            SetProcessed(path);
            if (m_listener != NULL)
            {
                VLOG(3) << "report lock acquired. path = " << path;
                m_listener->LockAcquired(path);
            }
        }
        else
            VLOG(3) << "node = " << node << ", maybe lose lock";
    }
    else // node 本身不是属性结点
    {
        for (int i = 0; i < vec.count; ++i)
        {
            len = kMaxLen;
            std::string path = node + "/" + std::string(vec.data[i]);
            DELT_FLAG flag = GetProcessedFlag(path);
            VLOG(3) << "deal child add. node = " << path << " val_len = " << len;
            if (flag == kNotDelt) // 如果没有打上标记
            {
                if (!IsAttrNode(path)) // 新加进来的节点，要监听其孩子节点
                {
                    SetWatcher(path, EVENT_MASK_CHILD_CHANGED);
                    VLOG(3) << "set child watch on node " << path;
                    bool is_locked = IsLocked(path); // 有设置监听(判断有没有上锁)
                    if (is_locked )
                    {
                        // set flag on node (置为处理过的)
                        SetProcessed(path);
                        if (m_listener != NULL)
                        {
                            VLOG(3) << "report lock acquired. node = " << path;
                            m_listener->LockAcquired(path);  // 向上上报
                        }
                    }
                }
                else // 新加进来的属性节点 要处理lock结点的孩子结点
                {
                    if (IsLockAttr(path)) // 如果当前是锁属性结点, 判断是否有孩子
                    {
                        // 设置孩子监听事件
                        SetWatcher(path, EVENT_MASK_CHILD_CHANGED);

                        std::string rep_path = "";
                        GetPathName(path, rep_path);
                        DELT_FLAG tmp_flag = GetProcessedFlag(rep_path);
                        if (tmp_flag == kNotDelt)
                        {
                            // 如果没有处理
                            bool is_locked = IsLocked(rep_path);
                            if (!is_locked) continue;
                            // set flag on node (置为处理过的)
                            SetProcessed(rep_path);
                            //  找该session对应的lock-node
                            if (m_listener != NULL)
                            {
                                VLOG(3) << "report lock acquire, node = " << rep_path;
                                m_listener->LockAcquired(rep_path);
                            }
                        }
                    }
                    else
                    {
                        // 忽略其他属性结点的孩子结点事件
                    }
                }
            }
            else
            {
                VLOG(3) << " node " << path << " has been delt. ";
            }
        }
    }
    //  释放资源
    deallocate_String_vector(&vec);
}

void DistributeLock::OnNodeCreate(const std::string& path, int state)
{
    // reserved
    VLOG(3) <<  "create node " << path;
    {
        MutexLocker locker(&m_mutex);
        if (m_handle == NULL)
            return;
    }
    if (m_listener != NULL)
    {
        VLOG(3) << "report node create. node = " << path;
        m_listener->NodeCreate(path);
    }
}

void DistributeLock::OnSessionExpired()
{
    VLOG(3) << "session expired";
    {
        MutexLocker locker(&m_mutex);
        if (m_handle == NULL)
            return;
    }
    if (m_listener != NULL)
    {
        VLOG(3) << "report session expired. session_id = " << m_session_str;
        m_listener->SessionExpired();
    }
}

void DistributeLock::OnSessionReConnected()
{
    VLOG(3) << "session reconnected";
    bool should_notify = false;
    {
        MutexLocker locker(&m_mutex); // must use lock to make sure m_listener has value.

        if (m_handle == NULL) return;
        if (m_listener != NULL)
        {
            VLOG(3) << "report session reconnected. session_id = " << m_session_str;
            should_notify = true;
        }
    }
    if (should_notify) {
        m_listener->SessionReConnected();
    }
}

int DistributeLock::GetBaseName(const std::string& src, std::string &dst)
{
    int pos = src.rfind('/');
    if (pos < 0) {
        return -1;
    }
    dst     = src.substr(pos + 1, src.length() - pos - 1);
    return 0;
}

int DistributeLock::GetPathName(const std::string& src, std::string &dst)
{
    int pos = src.rfind('/');
    if (pos < 0) {
        return -1;
    }
    dst     = src.substr(0, pos);
    return 0;
}


bool DistributeLock::IsAttrNode(const std::string& node)
{
    size_t pos = node.find("zoo_attr_");
    if (pos == std::string::npos) {
        return false;
    }
    return true;
}

void DistributeLock::GetPureAttrName(std::string* node)
{
    std::string str = std::string("zoo_attr_");
    size_t pos = node->find(str);
    *node = node->substr(pos+str.length(), node->length() - str.length() - pos);
}

int DistributeLock::Exists(const std::string& node)
{
    struct Stat stat;
    int ret = zoo_exists(m_handle, node.c_str(), 0, &stat);
    return ret;
}

int DistributeLock::UnLock(const std::string& node)
{
    //  判断该节点是否拥有锁
    std::string lock_path = node + "/zoo_attr_lock";
    struct String_vector vec = {0, NULL};
    int ret = zoo_get_children(m_handle, lock_path.c_str(), 0, &vec);
    if (ret == ZNONODE) {
        VLOG(3) <<  "the node has not been locked.";
    }
    else if (ret == ZOK  && vec.count != 0) {
        int idx;
        // 看看lock的sessionid跟自己的id符不符合
        // 对锁进行排序
        SortChildren(&vec);
        const char* prefix = m_session_str.c_str();
        //  找该session对应的lock-node
        for (idx = 0; idx < vec.count; ++idx) {
            char * child = vec.data[idx];
            if (strncmp(prefix, child, strlen(prefix)) == 0) {
                break;
            }
        }
        if (idx != 0) {
            VLOG(3) << "you don't own the lock";
            ret = ZNOAUTH;            // 返回没有权限
        }
        else
        {
            //  释放掉锁节点
            lock_path = lock_path + "/" + std::string(vec.data[0]);
            ret = zoo_delete(m_handle, lock_path.c_str(), -1);
            if (ret == ZOK) {
                VLOG(3) << "release lock node success. node = " << lock_path;
            }
            else
            {
                VLOG(3) << "release lock node failed. node = " << lock_path << ", error code = " <<
                        ErrorString(ret);
            }
        }
    }
    else
    {
        VLOG(3) << "unlock node = " << lock_path << " failed. error code = " <<
                ErrorString(ret);
    }
    deallocate_String_vector(&vec);
    return ret;
}

int DistributeLock::ListNode(const std::string& dir, std::vector<std::string>& node)
{
    node.clear();
    struct String_vector children_path = {0, NULL};
    int ret = zoo_get_children(m_handle, dir.c_str(), 0, &children_path);
    if (ret == ZOK) {
        for (int i = 0; i < children_path.count; ++i) {
            std::string child = std::string(children_path.data[i]);
            if (!IsAttrNode(child)) {
                node.push_back(child);
            }
        }
    } else {
        VLOG(3) << "get children failed. node = " << dir << ", error code = " <<
        ErrorString(ret);
    }
    deallocate_String_vector(&children_path);
    return ret;
}

int DistributeLock::ListAttr(const std::string& dir, std::vector<struct Attr>& attr)
{
    attr.clear();
    struct String_vector children_path = {0, NULL};
    int ret = zoo_get_children(m_handle, dir.data(), 0, &children_path);
    if (ret == ZOK) {
        for (int i = 0; i < children_path.count; ++i) {
            std::string child = std::string(children_path.data[i]);
            if (IsAttrNode(child)) {
                Attr tmp_attr;
                GetPureAttrName(&child);
                tmp_attr.name = child;
                ret = GetAttr(dir, child, tmp_attr.val);
                if (ret != ZOK)
                {
                    VLOG(3) << "node get attr failed. node = " << dir <<", attr = " << child <<
                        "error code = " << tmp_attr.val;
                }
                attr.push_back(tmp_attr);
            }
        }
    } else {
        VLOG(3) << "get children failed. node = " << dir << ", error code = " <<
            ErrorString(ret);
    }

    deallocate_String_vector(&children_path);
    return ret;
}

int DistributeLock::IsLockAttr(const std::string& node)
{
    std::string dst;
    if (GetBaseName(node, dst) != 0) {
        return -1;
    }
    if (dst == "zoo_attr_lock") {
        return 1;
    }
    return 0;
}

/// @brief 设置结点的值
///
/// @param node - 节点名称
/// @param value - 设置的值
/// @param version - 版本号
///
/// @retval 0 - 成功 其它 - 失败
int DistributeLock::ZooSet(const std::string& node, const std::string& value, int version)
{
    MutexLocker locker(&m_mutex);
    int ret = zoo_set(m_handle, node.c_str(), value.data(), value.length(), version);
    return ret;
}

/// @brief 获得结点的值
///
/// @param node - 结点名称
/// @param value - 获得的值
/// @param version - 获得的版本号
///
/// @retval 0 - 成功 其他 - 失败
int DistributeLock::ZooGet(const std::string& node, std::string& value, int* version)
{
    MutexLocker locker(&m_mutex);
    int len = kBuffLen;
    struct Stat stat;
    int ret = zoo_get(m_handle, node.c_str(), 0, m_buff, &len, &stat);
    if (ret == ZOK) {
        if (len > kBuffLen) {
            VLOG(3) << "too long data length, exceed limits";
            return -1;
        }
        if (len < 0) return -1;
        value = std::string(m_buff, len);
        if (version != NULL)
        {
            *version = stat.version;
        }
    }
    return ret;
}

/// @brief 设置标记，标记为session id
/// @param node - 处理的结点
void DistributeLock::SetProcessed(const std::string& node)
{
    SetAttr(node, "delt", m_session_str);
}

/// @brief 判断是否处理过
/// @param node - 待判断的结点
DELT_FLAG DistributeLock::GetProcessedFlag(const std::string& node)
{
    std::string value = "";
    int ret = GetAttr(node, "delt", value);
    if (ret == ZOK)
    {
        if (value == m_session_str)
            return kDelt;
        else
            return kNotDelt;
    }
    else if (ret == ZNONODE)
    {
        VLOG(3) << "haven't create attr node";
        return kNotDelt;
    }
    else
    {
        LOG(ERROR) << "get delt flag failed. error_code = " << ErrorString(ret);
        return kError;
    }
}

} // namespace distribute_lock
} // namespace toft
