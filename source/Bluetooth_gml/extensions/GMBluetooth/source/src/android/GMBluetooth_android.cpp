#include "GMBluetooth_backend.h"

#if defined(__ANDROID__)

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gmbluetooth
{
    class AndroidBackend;

    namespace
    {
        JavaVM* g_java_vm = nullptr;

        struct ScopedEnv
        {
            JNIEnv* env = nullptr;
            bool attached = false;

            ScopedEnv()
            {
                if (!g_java_vm)
                    return;

                if (g_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
                    return;

                if (g_java_vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
                    attached = true;
                else
                    env = nullptr;
            }

            ~ScopedEnv()
            {
                if (attached && g_java_vm)
                    g_java_vm->DetachCurrentThread();
            }
        };

        std::string from_jstring(JNIEnv* env, jstring text)
        {
            if (!env || !text)
                return {};

            const char* chars = env->GetStringUTFChars(text, nullptr);
            if (!chars)
                return {};
            std::string out(chars);
            env->ReleaseStringUTFChars(text, chars);
            return out;
        }

        jstring to_jstring(JNIEnv* env, const std::string& text)
        {
            return env ? env->NewStringUTF(text.c_str()) : nullptr;
        }

        Error error_from_int(jint value)
        {
            if (value < 0 || value > static_cast<jint>(Error::OperationFailed))
                return Error::Unknown;
            return static_cast<Error>(value);
        }

        PermissionStatus permission_from_int(jint value)
        {
            if (value == static_cast<jint>(PermissionStatus::Granted))
                return PermissionStatus::Granted;
            if (value == static_cast<jint>(PermissionStatus::Denied))
                return PermissionStatus::Denied;
            return PermissionStatus::Unknown;
        }

        std::atomic<AndroidBackend*> g_backend_instance{nullptr};

        struct JavaBridge
        {
            jclass cls = nullptr;

            jmethodID initialize = nullptr;
            jmethodID shutdown = nullptr;
            jmethodID permission_status = nullptr;
            jmethodID permission_request = nullptr;
            jmethodID le_scan_start = nullptr;
            jmethodID le_scan_stop = nullptr;
            jmethodID le_scan_running = nullptr;
            jmethodID classic_scan_start = nullptr;
            jmethodID classic_scan_stop = nullptr;
            jmethodID classic_scan_running = nullptr;
            jmethodID classic_connect = nullptr;
            jmethodID classic_disconnect = nullptr;
            jmethodID classic_is_connected = nullptr;
            jmethodID classic_send_bytes = nullptr;
            jmethodID classic_server_start = nullptr;
            jmethodID classic_server_stop = nullptr;
            jmethodID classic_server_running = nullptr;
        };

        JavaBridge g_bridge;
        std::mutex g_bridge_mutex;

        bool clear_exception(JNIEnv* env, std::string& message)
        {
            if (!env || !env->ExceptionCheck())
                return false;

            env->ExceptionClear();
            message = "Android Java exception";
            return true;
        }

        jclass load_helper_class(JNIEnv* env, std::string& message)
        {
            jclass activity_thread = env->FindClass("android/app/ActivityThread");
            if (!activity_thread)
            {
                clear_exception(env, message);
                message = "Unable to resolve android.app.ActivityThread";
                return nullptr;
            }

            jmethodID current_application = env->GetStaticMethodID(
                activity_thread,
                "currentApplication",
                "()Landroid/app/Application;");
            jobject application = current_application
                ? env->CallStaticObjectMethod(activity_thread, current_application)
                : nullptr;
            env->DeleteLocalRef(activity_thread);

            if (!application || clear_exception(env, message))
            {
                message = "Android Application is not available";
                return nullptr;
            }

            jclass context_class = env->GetObjectClass(application);
            jmethodID get_package_name = env->GetMethodID(
                context_class,
                "getPackageName",
                "()Ljava/lang/String;");
            jstring package_string = static_cast<jstring>(
                env->CallObjectMethod(application, get_package_name));
            std::string package_name = from_jstring(env, package_string);
            if (package_string) env->DeleteLocalRef(package_string);

            jmethodID get_class_loader = env->GetMethodID(
                context_class,
                "getClassLoader",
                "()Ljava/lang/ClassLoader;");
            jobject loader = env->CallObjectMethod(application, get_class_loader);
            env->DeleteLocalRef(context_class);
            env->DeleteLocalRef(application);

            if (!loader || package_name.empty() || clear_exception(env, message))
            {
                message = "Unable to obtain Android application ClassLoader";
                return nullptr;
            }

            jclass loader_class = env->FindClass("java/lang/ClassLoader");
            jmethodID load_class = env->GetMethodID(
                loader_class,
                "loadClass",
                "(Ljava/lang/String;)Ljava/lang/Class;");

            const std::string full_name = package_name + ".GMBluetooth";
            jstring class_name = env->NewStringUTF(full_name.c_str());
            jobject loaded = env->CallObjectMethod(loader, load_class, class_name);

            env->DeleteLocalRef(class_name);
            env->DeleteLocalRef(loader_class);
            env->DeleteLocalRef(loader);

            if (!loaded || clear_exception(env, message))
            {
                message = "Unable to load " + full_name;
                return nullptr;
            }

            return static_cast<jclass>(loaded);
        }

        // Native callback forward declarations for RegisterNatives.
        void JNICALL jni_device_found(
            JNIEnv*, jclass, jint, jstring, jstring, jstring, jint, jboolean, jboolean);
        void JNICALL jni_scan_stopped(JNIEnv*, jclass, jint, jint, jstring);
        void JNICALL jni_classic_connected(JNIEnv*, jclass, jlong, jint, jstring);
        jlong JNICALL jni_classic_server_accepted(JNIEnv*, jclass, jstring, jstring, jstring);
        void JNICALL jni_classic_data(JNIEnv*, jclass, jlong, jbyteArray);
        void JNICALL jni_classic_disconnected(JNIEnv*, jclass, jlong, jint, jstring);

        bool ensure_bridge(JNIEnv* env, std::string& message)
        {
            std::scoped_lock lock(g_bridge_mutex);
            if (g_bridge.cls)
                return true;

            jclass local_class = load_helper_class(env, message);
            if (!local_class)
                return false;

            const JNINativeMethod native_methods[] = {
                {const_cast<char*>("nativeDeviceFound"),
                 const_cast<char*>("(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;IZZ)V"),
                 reinterpret_cast<void*>(jni_device_found)},
                {const_cast<char*>("nativeScanStopped"),
                 const_cast<char*>("(IILjava/lang/String;)V"),
                 reinterpret_cast<void*>(jni_scan_stopped)},
                {const_cast<char*>("nativeClassicConnected"),
                 const_cast<char*>("(JILjava/lang/String;)V"),
                 reinterpret_cast<void*>(jni_classic_connected)},
                {const_cast<char*>("nativeClassicServerAccepted"),
                 const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J"),
                 reinterpret_cast<void*>(jni_classic_server_accepted)},
                {const_cast<char*>("nativeClassicData"),
                 const_cast<char*>("(J[B)V"),
                 reinterpret_cast<void*>(jni_classic_data)},
                {const_cast<char*>("nativeClassicDisconnected"),
                 const_cast<char*>("(JILjava/lang/String;)V"),
                 reinterpret_cast<void*>(jni_classic_disconnected)},
            };

            if (env->RegisterNatives(
                    local_class,
                    native_methods,
                    sizeof(native_methods) / sizeof(native_methods[0])) != JNI_OK)
            {
                clear_exception(env, message);
                env->DeleteLocalRef(local_class);
                message = "RegisterNatives failed for GMBluetooth";
                return false;
            }

            g_bridge.cls = static_cast<jclass>(env->NewGlobalRef(local_class));
            env->DeleteLocalRef(local_class);
            if (!g_bridge.cls)
            {
                message = "Unable to create global GMBluetooth class reference";
                return false;
            }

            auto method = [env](const char* name, const char* sig) -> jmethodID
            {
                return env->GetStaticMethodID(g_bridge.cls, name, sig);
            };

            g_bridge.initialize = method("bt_init", "()D");
            g_bridge.shutdown = method("bt_end", "()D");
            g_bridge.permission_status = method("bluetooth_permission_get_status", "()D");
            g_bridge.permission_request = method("bluetooth_permission_request", "()D");
            g_bridge.le_scan_start = method("bt_le_scan_start", "()D");
            g_bridge.le_scan_stop = method("bt_le_scan_stop", "()D");
            g_bridge.le_scan_running = method("bt_le_scan_is_active", "()D");
            g_bridge.classic_scan_start = method("bt_classic_scan_start", "()D");
            g_bridge.classic_scan_stop = method("bt_classic_scan_stop", "()D");
            g_bridge.classic_scan_running = method("bt_classic_scan_is_active", "()D");
            g_bridge.classic_connect = method("bt_classic_socket_open", "(Ljava/lang/String;Ljava/lang/String;D)D");
            g_bridge.classic_disconnect = method("bt_classic_socket_close", "(D)D");
            g_bridge.classic_is_connected = method("bt_classic_socket_close", "(D)D");
            g_bridge.classic_send_bytes = method("bt_classic_socket_send", "(DD[BDD)D");
            g_bridge.classic_server_start = method("bt_classic_server_start", "(Ljava/lang/String;Ljava/lang/String;)D");
            g_bridge.classic_server_stop = method("bt_classic_server_stop", "()D");
            g_bridge.classic_server_running = method("bt_classic_server_stop", "()D");

            if (clear_exception(env, message) ||
                !g_bridge.initialize || !g_bridge.shutdown ||
                !g_bridge.permission_status || !g_bridge.permission_request ||
                !g_bridge.le_scan_start || !g_bridge.le_scan_stop || !g_bridge.le_scan_running ||
                !g_bridge.classic_scan_start || !g_bridge.classic_scan_stop || !g_bridge.classic_scan_running ||
                !g_bridge.classic_connect || !g_bridge.classic_disconnect || !g_bridge.classic_is_connected ||
                !g_bridge.classic_send_bytes ||
                !g_bridge.classic_server_start || !g_bridge.classic_server_stop || !g_bridge.classic_server_running)
            {
                message = "GMBluetooth method lookup failed";
                return false;
            }

            return true;
        }
    }

    class AndroidBackend final : public Backend
    {
    public:
        explicit AndroidBackend(CoreHooks hooks)
            : hooks_(std::move(hooks))
        {
            g_backend_instance.store(this, std::memory_order_release);
        }

        ~AndroidBackend() override
        {
            if (initialized_)
                shutdown();
            AndroidBackend* expected = this;
            g_backend_instance.compare_exchange_strong(
                expected, nullptr, std::memory_order_acq_rel);
        }

        Error initialize(std::string& message) override
        {
            ScopedEnv scoped;
            if (!scoped.env)
            {
                message = "Android JNI environment is unavailable";
                return Error::OperationFailed;
            }
            if (!ensure_bridge(scoped.env, message))
                return Error::OperationFailed;

            const jdouble ok = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.initialize);
            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            if (ok != 0)
            {
                message = "Android BluetoothAdapter is unavailable";
                return Error::NotSupported;
            }

            initialized_ = true;
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
            ScopedEnv scoped;
            if (scoped.env && g_bridge.cls && g_bridge.shutdown)
            {
                scoped.env->CallStaticDoubleMethod(g_bridge.cls, g_bridge.shutdown);
                std::string ignored;
                clear_exception(scoped.env, ignored);
            }

            {
                std::scoped_lock lock(receive_mutex_);
                received_.clear();
            }
            {
                std::scoped_lock lock(connection_mutex_);
                connection_devices_.clear();
            }
            initialized_ = false;
        }

        bool supports_ble() const override { return true; }
        bool supports_classic() const override { return true; }
        bool supports_classic_server() const override { return true; }

        PermissionStatus permission_status() const override
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls)
                return PermissionStatus::Unknown;
            const jdouble value = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.permission_status);
            std::string ignored;
            if (clear_exception(scoped.env, ignored))
                return PermissionStatus::Unknown;
            return permission_from_int(static_cast<jint>(value));
        }

        Error permission_request(std::string& message) override
        {
            return call_error_no_args(g_bridge.permission_request, message);
        }

        Error le_scan_start(bool active, std::string& message) override
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls)
                return Error::NotInitialized;
            const jdouble value = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.le_scan_start);
            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            return error_from_int(static_cast<jint>(value));
        }

        Error le_scan_stop(std::string& message) override
        {
            return call_error_no_args(g_bridge.le_scan_stop, message);
        }

        bool le_scan_is_running() const override
        {
            return call_bool_no_args(g_bridge.le_scan_running);
        }

        Error classic_scan_start(std::string& message) override
        {
            return call_error_no_args(g_bridge.classic_scan_start, message);
        }

        Error classic_scan_stop(std::string& message) override
        {
            return call_error_no_args(g_bridge.classic_scan_stop, message);
        }

        bool classic_scan_is_running() const override
        {
            return call_bool_no_args(g_bridge.classic_scan_running);
        }

        Error classic_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            const std::string& service_uuid,
            std::string& message) override
        {
            if (!device.address_available || device.address.empty())
            {
                message = "Bluetooth Classic device has no usable address";
                return Error::InvalidArgument;
            }

            const std::uint64_t device_handle = hooks_.upsert_device
                ? hooks_.upsert_device(device)
                : 0;
            {
                std::scoped_lock lock(connection_mutex_);
                connection_devices_[connection] = device_handle;
            }

            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls)
                return Error::NotInitialized;

            jstring address = to_jstring(scoped.env, device.address);
            jstring uuid = to_jstring(scoped.env, service_uuid);
            const jdouble value = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.classic_connect,
                address,
                uuid,
                0.0);
            if (address) scoped.env->DeleteLocalRef(address);
            if (uuid) scoped.env->DeleteLocalRef(uuid);

            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            return error_from_int(static_cast<jint>(value));
        }

        Error classic_disconnect(
            std::uint64_t connection,
            std::string& message) override
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls)
                return Error::NotInitialized;
            const jdouble value = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.classic_disconnect,
                static_cast<jdouble>(connection));
            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            return error_from_int(static_cast<jint>(value));
        }

        bool classic_connection_is_connected(std::uint64_t connection) const override
        {
            return false;
        }

        std::int32_t classic_receive_available(std::uint64_t connection) const override
        {
            std::scoped_lock lock(receive_mutex_);
            const auto it = received_.find(connection);
            return it == received_.end()
                ? 0
                : static_cast<std::int32_t>(it->second.size());
        }

        Error classic_send_bytes(
            std::uint64_t connection,
            const std::uint8_t* data,
            std::size_t size,
            std::string& message) override
        {
            message.clear();
            return Error::Ok;
        }

        std::size_t classic_receive_bytes(
            std::uint64_t connection,
            std::uint8_t* out,
            std::size_t max_size) override
        {
            if (!out || max_size == 0)
                return 0;

            std::scoped_lock lock(receive_mutex_);
            auto it = received_.find(connection);
            if (it == received_.end())
                return 0;

            const std::size_t count = std::min(max_size, it->second.size());
            for (std::size_t i = 0; i < count; ++i)
            {
                out[i] = it->second.front();
                it->second.pop_front();
            }
            if (it->second.empty())
                received_.erase(it);
            return count;
        }

        Error classic_server_start(
            const std::string& name,
            const std::string& service_uuid,
            std::string& message) override
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls)
                return Error::NotInitialized;

            jstring jname = to_jstring(scoped.env, name);
            jstring juuid = to_jstring(scoped.env, service_uuid);
            const jdouble value = scoped.env->CallStaticDoubleMethod(
                g_bridge.cls,
                g_bridge.classic_server_start,
                jname,
                juuid);
            if (jname) scoped.env->DeleteLocalRef(jname);
            if (juuid) scoped.env->DeleteLocalRef(juuid);

            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            return error_from_int(static_cast<jint>(value));
        }

        Error classic_server_stop(std::string& message) override
        {
            return call_error_no_args(g_bridge.classic_server_stop, message);
        }

        bool classic_server_is_running() const override
        {
            return false;
        }

        bool pump_events()
        {
            return false;
        }

        void on_device_found(
            Transport transport,
            std::string id,
            std::string name,
            std::string address,
            std::int32_t rssi,
            bool has_rssi,
            bool connectable)
        {
            DiscoveredDevice device;
            device.transport = transport;
            device.id = std::move(id);
            device.name = std::move(name);
            device.address = std::move(address);
            device.address_available = !device.address.empty();
            device.rssi = rssi;
            device.rssi_available = has_rssi;
            device.connectable = connectable;
            if (hooks_.upsert_device)
                hooks_.upsert_device(device);
        }

    private:
        Error call_error_no_args(jmethodID method, std::string& message) const
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls || !method)
                return Error::NotInitialized;
            const jdouble value = scoped.env->CallStaticDoubleMethod(g_bridge.cls, method);
            if (clear_exception(scoped.env, message))
                return Error::OperationFailed;
            return error_from_int(static_cast<jint>(value));
        }

        bool call_bool_no_args(jmethodID method) const
        {
            ScopedEnv scoped;
            if (!scoped.env || !g_bridge.cls || !method)
                return false;
            const jdouble value = scoped.env->CallStaticDoubleMethod(g_bridge.cls, method);
            std::string ignored;
            return !clear_exception(scoped.env, ignored) && value != 0;
        }

        CoreHooks hooks_;
        bool initialized_ = false;

        mutable std::mutex connection_mutex_;
        std::unordered_map<std::uint64_t, std::uint64_t> connection_devices_;

        mutable std::mutex receive_mutex_;
        std::unordered_map<std::uint64_t, std::deque<std::uint8_t>> received_;
    };

    namespace
    {
        void JNICALL jni_device_found(
            JNIEnv* env,
            jclass,
            jint transport,
            jstring id,
            jstring name,
            jstring address,
            jint rssi,
            jboolean has_rssi,
            jboolean connectable)
        {
            auto* backend = g_backend_instance.load(std::memory_order_acquire);
            if (!backend) return;
            backend->on_device_found(
                static_cast<Transport>(transport),
                from_jstring(env, id),
                from_jstring(env, name),
                from_jstring(env, address),
                static_cast<std::int32_t>(rssi),
                has_rssi == JNI_TRUE,
                connectable == JNI_TRUE);
        }

        void JNICALL jni_scan_stopped(
            JNIEnv* env,
            jclass,
            jint transport,
            jint error,
            jstring message)
        {
        }

        void JNICALL jni_classic_connected(
            JNIEnv* env,
            jclass,
            jlong connection,
            jint error,
            jstring message)
        {
        }

        jlong JNICALL jni_classic_server_accepted(
            JNIEnv* env,
            jclass,
            jstring id,
            jstring name,
            jstring address)
        {
            return 0;
        }

        void JNICALL jni_classic_data(
            JNIEnv* env,
            jclass,
            jlong connection,
            jbyteArray bytes)
        {
        }

        void JNICALL jni_classic_disconnected(
            JNIEnv* env,
            jclass,
            jlong connection,
            jint error,
            jstring message)
        {
        }
    }

void set_android_java_vm(JavaVM* vm)
{
}

std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
{
    return std::make_unique<AndroidBackend>(std::move(hooks));
}
}

#endif
