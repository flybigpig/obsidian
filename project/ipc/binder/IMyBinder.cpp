// IMyBinder.cpp
// Binder 接口实现
#include "IMyBinder.h"

namespace android {

// BnMyBinder onTransact 实现
status_t IMyBinder::BnMyBinder::onTransact(uint32_t code, const Parcel& data,
                                           Parcel* reply, uint32_t flags) {
    switch (code) {
        case GET_SERVICE_NAME: {
            String16 name;
            status_t status = handleGetServiceName(name);
            if (reply) {
                reply->writeInt32(status);
                if (status == OK) {
                    reply->writeString16(name);
                }
            }
            return OK;
        }
        case ADD: {
            int32_t a = data.readInt32();
            int32_t b = data.readInt32();
            int32_t result = 0;
            status_t status = handleAdd(a, b, &result);
            if (reply) {
                reply->writeInt32(status);
                if (status == OK) {
                    reply->writeInt32(result);
                }
            }
            return OK;
        }
        case SEND_MESSAGE: {
            String16 message = data.readString16();
            status_t status = handleSendMessage(message);
            if (reply) {
                reply->writeInt32(status);
            }
            return OK;
        }
        case IS_CONNECTED: {
            bool connected = false;
            status_t status = handleIsConnected(&connected);
            if (reply) {
                reply->writeInt32(status);
                if (status == OK) {
                    reply->writeInt32(connected ? 1 : 0);
                }
            }
            return OK;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// BpMyBinder 实现
BpMyBinder::BpMyBinder(const sp<IBinder>& impl)
    : BpInterface<IMyBinder>(impl) {}

status_t BpMyBinder::getServiceName(String16& name) {
    Parcel data, reply;
    data.writeInterfaceToken(DESCRIPTOR);
    
    status_t status = remote()->transact(GET_SERVICE_NAME, data, &reply);
    if (status == OK) {
        status = reply.readInt32();
        if (status == OK) {
            name = reply.readString16();
        }
    }
    return status;
}

status_t BpMyBinder::add(int32_t a, int32_t b, int32_t* result) {
    Parcel data, reply;
    data.writeInterfaceToken(DESCRIPTOR);
    data.writeInt32(a);
    data.writeInt32(b);
    
    status_t status = remote()->transact(ADD, data, &reply);
    if (status == OK) {
        status = reply.readInt32();
        if (status == OK) {
            *result = reply.readInt32();
        }
    }
    return status;
}

status_t BpMyBinder::sendMessage(const String16& message) {
    Parcel data, reply;
    data.writeInterfaceToken(DESCRIPTOR);
    data.writeString16(message);
    
    return remote()->transact(SEND_MESSAGE, data, &reply);
}

status_t BpMyBinder::isConnected(bool* connected) {
    Parcel data, reply;
    data.writeInterfaceToken(DESCRIPTOR);
    
    status_t status = remote()->transact(IS_CONNECTED, data, &reply);
    if (status == OK) {
        status = reply.readInt32();
        if (status == OK) {
            *connected = reply.readInt32() != 0;
        }
    }
    return status;
}

// 注册 BnMyBinder 到 service manager
IMPLEMENT_META_INTERFACE(IMyBinder, "com.example.binder.IMyBinder");

} // namespace android
