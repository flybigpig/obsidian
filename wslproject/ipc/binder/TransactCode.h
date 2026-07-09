// TransactCode.h
// 事务码定义
#ifndef TRANSACT_CODE_H
#define TRANSACT_CODE_H

namespace android {

// IMyBinder 事务码
enum {
    GET_SERVICE_NAME = android::IBinder::FIRST_CALL_TRANSACTION,
    ADD,
    SEND_MESSAGE,
    IS_CONNECTED,
};

} // namespace android

#endif // TRANSACT_CODE_H
