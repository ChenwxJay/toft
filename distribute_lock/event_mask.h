#ifndef TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H
#define TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H

namespace toft {
namespace distribute_lock {

enum ZKEventMask
{
    EVENT_MASK_NONE                 = 0x0000, ///< �޸���Ȥ�¼�
    EVENT_MASK_ATTR_CHANGED         = 0x0001, ///< ���Ըı��¼�
    EVENT_MASK_CHILD_CHANGED        = 0x0002, ///< ���ӽڵ����Եĸı�
    EVENT_MASK_LOCK_RELEASE         = 0x0004, ///< ��ʧЧ
    EVENT_MASK_DATA_CHANGED         = 0x0008, ///< ֵ�ı��¼�
    EVENT_MASK_LOCK_ACQUIRED        = 0x0010, ///< �����¼�
    EVENT_MASK_SESSION_EXPIRED      = 0x0020, ///< sessionʧЧ�¼�
    EVENT_MASK_SESSION_RECONNECTED  = 0x0040, ///< session close and connected
    EVENT_MASK_NODE_EVENT           = 0x0080, ///< �ڵ���ɾ���¼�
};

} // distribute_lock
} // toft

#endif // TOFT_DISTRIBUTE_LOCK_EVENT_MASK_H
