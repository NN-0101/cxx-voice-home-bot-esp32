//
// Created by 0101 on 2026/3/24.
//

#ifndef CXX_VOICE_HOME_BOT_ESP32_WEBSOCKETCLIENT_H
#define CXX_VOICE_HOME_BOT_ESP32_WEBSOCKETCLIENT_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <WebSocketsClient.h>

/**
 * WebSocket 客户端类
 * 提供完整的 WebSocket 功能：连接、心跳、发送消息、接收消息
 */
class WebSocketClient
{
public:
    /**
    * 消息类型枚举
    * 定义了 WebSocket 可能接收到的各种消息类型
    */
    enum class MessageType
    {
        TEXT, // 文本消息（UTF-8 编码的字符串）
        BINARY, // 二进制消息（原始数据，如图片、音频等）
        PING, // Ping 心跳请求（服务器发送的保活消息）
        PONG, // Pong 心跳响应（客户端自动回复，但也可通知上层）
        ERROR, // 连接或通信错误
        DISCONNECTED, // 连接已断开（主动或被动）
        CONNECTED, // 连接已建立成功
    };

    /**
     * 构造函数
     */
    WebSocketClient();
    /**
     * 析构函数
     */
    ~WebSocketClient();

    /**
        * 消息接收回调函数类型
        * 当收到 WebSocket 消息时调用
        *
        * @param type 消息类型（文本/二进制/心跳等）
        * @param payload 消息数据指针（注意：根据 type 不同，可能为文本或二进制）
        * @param length 消息数据长度（字节）
        *
        * 使用示例：
        *   client.onMessage([](MessageType type, uint8_t* data, size_t len) {
        *       if (type == MessageType::TEXT) {
        *           String text = String((char*)data, len);
        *           Serial.printf("收到文本: %s\n", text.c_str());
        *       } else if (type == MessageType::BINARY) {
        *           Serial.printf("收到二进制数据: %d 字节\n", len);
        *       }
        *   });
        */
    typedef std::function<void(MessageType type, uint8_t* payload, size_t length)> MessageCallback;

    /**
     * 连接状态回调函数类型
     * 当 WebSocket 连接状态发生变化时调用
     *
     * @param connected 连接状态：true = 已连接，false = 已断开
     *
     * 使用示例：
     *   client.onConnection([](bool connected) {
     *       if (connected) {
     *           Serial.println("✅ WebSocket 已连接");
     *       } else {
     *           Serial.println("❌ WebSocket 已断开");
     *       }
     *   });
     */
    typedef std::function<void(bool connected)> ConnectionCallback;

    /**
     * 错误回调函数类型
     * 当 WebSocket 发生错误时调用
     *
     * @param error 错误描述信息（字符串）
     *
     * 使用示例：
     *   client.onError([](const String& error) {
     *       Serial.printf("⚠️ WebSocket 错误: %s\n", error.c_str());
     *   });
     */
    typedef std::function<void(const String& error)> ErrorCallback;

    /**
     * 通过完整 URL 初始化 WebSocket 客户端
     * 支持格式: ws://host:port/path 或 wss://host:port/path
     *
     * @param serverUrl WebSocket 服务器完整 URL
     * @param useSSL_ 是否使用 SSL/TLS 加密
     * @return true: 初始化成功，false: 初始化失败
     */
    bool begin(const String& serverUrl, bool useSSL_ = false);

    /**
     * 初始化 WebSocket 客户端（指定主机、端口和路径）
     * 配置 WebSocket 连接参数并建立底层连接
     *
     * @param host 服务器主机地址（如 "192.168.1.100" 或 "example.com"）
     * @param port 服务器端口号（WebSocket 默认: 80，WSS 默认: 443）
     * @param path WebSocket 路径（如 "/ws"、"/voice"，默认为 "/"）
     * @param userSSL_ 是否使用 SSL/TLS 加密（true: wss://, false: ws://）
     * @return true: 初始化成功，false: 初始化失败
     */
    bool begin(const String& host, uint16_t port, const String& path = "/", bool userSSL_ = false);

    /**
     * WebSocket 客户端主循环处理函数
     * 必须在主程序的 loop() 中定期调用，以处理：
     *   - WebSocket 底层事件（接收消息、连接状态变化等）
     *   - 自动重连逻辑（断线后尝试重连）
     *   - 心跳保活机制（定期发送心跳保持连接）
     *
     * 调用频率：建议在每次 Arduino loop() 中调用，或至少每 100ms 调用一次
     */
    void loop();

    /**
     * 发送文本消息
     * @param message 消息内容
     * @return 是否发送成功
     */
    bool sendText(const String& message);

    /**
     * 发送JSON消息
     * @param json JSON数据
     * @return 是否发送成功
     */
    bool sendJSON(const JsonDocument& json);

    /**
     * 发送二进制数据
     * @param data 数据指针
     * @param size 数据大小
     * @return 是否发送成功
     */
    bool sendBinary(const uint8_t* data, size_t size);

    /**
    * 发送心跳消息
    * 定期发送心跳保持连接活跃，防止 NAT 超时
    *
    * @return true: 发送成功，false: 发送失败
    */
    bool sendHeartbeat();

    /**
     * 断开连接
     */
    void disconnect();

    /**
     * 是否连接
     * @return 是否连接
     */
    bool isConnected() const;

    /**
     * 获取设备MAC地址
     * @return MAC地址字符串
     */
    static String getDeviceMac();

    /**
     * 获取设备ID（MAC地址去除冒号）
     * @return 设备id
     */
    static String getDeviceId();

    /**
     * 发送设备注册信息
     * @return 是否发送成功
     */
    bool sendRegister();

    /**
     * 发送设备状态
     * @param status 状态信息
     * @return 是否发送成功
     */
    bool sendStatus(int status);

    /**
     * 设置消息接收回调
     * @param callback 回调函数
     */
    void onMessage(MessageCallback callback);

    /**
     * 设置连接状态回调
     * @param callback 回调函数
     */
    void onConnection(ConnectionCallback callback);

    /**
     * 设置连接状态回调
     * @param callback 回调函数
     */
    void onError(ErrorCallback callback);

    /**
     * 设置自动重连
     * @param enabled 是否启用
     * @param interval 重连间隔（毫秒）
     */
    void setAutoReconnect(bool enabled, unsigned long interval = 5000);

    /**
     * 设置心跳间隔
     * @param interval 心跳间隔（毫秒），0表示禁用自己心跳
     */
    void setHeartbeatInterval(unsigned long interval);

private:
    /**
     * WebSocket 底层客户端实例
     * 来自 WebSockets 库，提供基础的 WebSocket 协议实现
     * 负责处理连接、发送、接收、心跳等底层操作
     */
    WebSocketsClient webSocket;

    /**
     * 服务器主机地址
     * 例如: "ws.example.com" 或 "192.168.1.100"
     */
    String serverHost;

    /**
     * 服务器端口号
     * WebSocket 默认端口: 80 (ws://) 或 443 (wss://)
     */
    uint16_t serverPort;

    /**
     * WebSocket 路径
     * 例如: "/ws", "/voice", "/api/ws"
     * 默认: "/"
     */
    String serverPath;

    /**
     * 是否使用 SSL/TLS 加密
     * true: 使用 wss:// 协议（需要证书）
     * false: 使用 ws:// 协议（明文传输）
     */
    bool useSSL;

    /**
     * 连接状态标志
     * true: 已连接到服务器
     * false: 未连接或已断开
     */
    bool connected;

    /**
     * 初始化状态标志
     * true: 已调用 begin() 完成初始化
     * false: 未初始化或初始化失败
     */
    bool initialized;

    // ==================== 设备信息 ====================

    /**
     * 设备 MAC 地址
     * 格式: "AA:BB:CC:DD:EE:FF"
     * 用于设备唯一标识和服务器认证
     */
    String deviceMac;

    /**
     * 设备 ID（MAC 地址去除冒号）
     * 用于 API 调用和日志记录，更简洁
     */
    String deviceId;

    /**
     * 是否启用自动重连功能
     * true: 连接断开后自动尝试重连
     * false: 连接断开后不自动重连，需手动调用 begin()
     */
    bool autoReconnectEnabled;

    /**
     * 自动重连间隔（毫秒）
     * 两次重连尝试之间的等待时间
     * 默认: 5000 毫秒（5秒）
     */
    unsigned long autoReconnectInterval;

    /**
     * 上次重连尝试时间戳（毫秒）
     * 用于控制重连频率，避免频繁重连
     */
    unsigned long lastReconnectAttempt;

    /**
     * 心跳发送间隔（毫秒）
     * 0: 禁用自动心跳，使用库默认的心跳机制
     * >0: 定期发送心跳消息保持连接活跃
     */
    unsigned long heartbeatInterval;

    /**
     * 上次发送心跳的时间戳（毫秒）
     * 用于判断是否需要发送下一次心跳
     */
    unsigned long lastHeartbeatTime;

    /**
     * 消息接收回调函数
     * 当收到服务器消息时调用
     * 参数: 消息类型、数据指针、数据长度
     */
    MessageCallback messageCallback;

    /**
     * 连接状态回调函数
     * 当连接建立或断开时调用
     * 参数: true=已连接, false=已断开
     */
    ConnectionCallback connectionCallback;

    /**
     * 错误回调函数
     * 当发生错误时调用
     * 参数: 错误描述字符串
     */
    ErrorCallback errorCallback;

    /**
     * 处理 WebSocket 底层事件
     * 这是 WebSockets 库的事件回调函数，负责处理所有 WebSocket 相关的底层事件
     * 并将它们转换为上层可用的回调通知
     *
     * @param type 事件类型（来自 WebSockets 库）
     * @param payload 事件相关的数据负载（消息内容、错误信息等）
     * @param length 负载数据长度（字节）
     *
     * 事件类型说明：
     *   WStype_DISCONNECTED: 连接断开
     *   WStype_CONNECTED:    连接成功建立
     *   WStype_TEXT:         收到文本消息
     *   WStype_BIN:          收到二进制消息
     *   WStype_PING:         收到 Ping 心跳请求
     *   WStype_PONG:         收到 Pong 心跳响应
     *   WStype_ERROR:        发生错误
     */
    void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);

    /**
     * 发送心跳（如果需要）
     *
     * 根据配置的心跳间隔，定期发送心跳消息以保持连接活跃。
     * 此函数应在主循环中定期调用（通常在 loop() 中），
     * 它会检查是否需要发送心跳，并在需要时自动发送。
     *
     * 心跳的作用：
     *   1. 保持 WebSocket 连接活跃，防止 NAT 超时断开
     *   2. 检测连接是否仍然有效（如果长时间未收到响应，可判定为断开）
     *   3. 维持服务器端的连接状态
     *
     * 调用方式：
     *   在 WebSocketClient::loop() 中自动调用，无需手动调用
     */
    void sendHeartbeatIfNeeded();
};


#endif //CXX_VOICE_HOME_BOT_ESP32_WEBSOCKETCLIENT_H
