//
// Created by 0101 on 2026/3/24.
//

#include "WebSocketClient.h"

WebSocketClient::WebSocketClient()
    : serverPort(0)
      , useSSL(false)
      , connected(false)
      , initialized(false)
      , autoReconnectEnabled(false)
      , autoReconnectInterval(5000)
      , lastReconnectAttempt(0)
      , heartbeatInterval(30000) // 默认 30 秒心跳
      , lastHeartbeatTime(0)
      , messageCallback(nullptr)
      , connectionCallback(nullptr)
      , errorCallback(nullptr)
{
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

bool WebSocketClient::begin(const String& serverUrl, bool useSSL_)
{
    String url = serverUrl;
    String host;
    uint16_t port;
    String path = "/";

    // 验证并移除协议前缀
    if (useSSL_)
    {
        if (url.startsWith("wss://"))
        {
            url = url.substring(6);  // 移除 "wss://"
        }
        else
        {
            if (errorCallback)
            {
                errorCallback("URL 必须以 wss:// 开头");
            }
            return false;
        }
    }
    else
    {
        if (url.startsWith("ws://"))
        {
            url = url.substring(5);  // 移除 "ws://"
        }
        else
        {
            if (errorCallback)
            {
                errorCallback("URL 必须以 ws:// 开头");
            }
            return false;
        }
    }

    // 解析主机、端口和路径
    // 注意：indexOf 返回 int，可能为 -1
    int colonIndex = url.indexOf(':');   // 冒号位置（端口分隔符）
    int slashIndex = url.indexOf('/');    // 斜杠位置（路径分隔符）

    // 转换为 bool 值，避免无符号类型问题
    bool hasColon = (colonIndex != -1);
    bool hasSlash = (slashIndex != -1);

    // URL 包含端口号，格式: host:port/path 或 host:port
    if (hasColon && (!hasSlash || colonIndex < slashIndex))
    {
        // 提取主机名（冒号之前的部分）
        host = url.substring(0, colonIndex);

        // 提取端口号（冒号和斜杠之间，或冒号和末尾之间）
        // 显式转换 url.length() 为 int，避免类型不匹配警告
        int endPortIndex = hasSlash ? slashIndex : (int)url.length();
        port = url.substring(colonIndex + 1, endPortIndex).toInt();

        // 提取路径（斜杠之后的部分）
        if (hasSlash)
        {
            path = url.substring(slashIndex);
        }
    }
    // URL 不包含端口号，格式: host/path 或 host
    else
    {
        // 提取主机名（斜杠之前的部分，或整个字符串）
        int hostEndIndex = hasSlash ? slashIndex : (int)url.length();
        host = url.substring(0, hostEndIndex);

        // 使用默认端口
        port = useSSL_ ? 443 : 80;

        // 提取路径（斜杠之后的部分）
        if (hasSlash)
        {
            path = url.substring(slashIndex);
        }
    }

    // 调用重载的 begin 函数
    return begin(host, port, path, useSSL_);
}

bool WebSocketClient::begin(const String& host, uint16_t port, const String& path, bool userSSL_)
{
    this->serverHost = host;
    this->serverPort = port;
    this->serverPath = path;
    this->useSSL = userSSL_;

    // 获取设备信息
    deviceMac = getDeviceMac();
    deviceId = getDeviceId();

    Serial.println("========================================");
    Serial.println("🌐 WebSocket 客户端初始化");
    Serial.printf("   服务器: %s:%d%s\n", serverHost.c_str(), serverPort, serverPath.c_str());
    Serial.printf("   协议: %s\n", useSSL ? "WSS (SSL)" : "WS");
    Serial.printf("   设备 MAC: %s\n", deviceMac.c_str());
    Serial.printf("   设备 ID: %s\n", deviceId.c_str());
    Serial.println("========================================");

    // 设置底层 WebSocket 库的事件处理函数
    // 当连接、断开、收到消息等事件发生时，会调用 handleWebSocketEvent
    webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        // 使用 Lambda 表达式捕获 this 指针，将事件转发给成员函数处理
        handleWebSocketEvent(type, payload, length);
    });

    // 调用底层库的 begin 方法，建立 WebSocket 连接
    // 参数说明：
    //   - serverHost.c_str(): 服务器地址（C 字符串）
    //   - serverPort: 端口号
    //   - serverPath.c_str(): WebSocket 路径
    //   - useSSL ? "wss" : "ws": 协议类型（"wss" 或 "ws"）
    webSocket.begin(
        serverHost.c_str(),           // 主机地址
        serverPort,                    // 端口
        serverPath.c_str(),           // 路径
        useSSL ? "wss" : "ws"         // 协议（wss 表示 SSL 加密）
    );

    // 配置断开后自动重连的时间间隔（毫秒），当连接意外断开时，底层库会每隔 5 秒尝试重连一次
    webSocket.setReconnectInterval(5000);

    // 设置初始化标志
    initialized = true;
    Serial.println("✅ WebSocket 客户端初始化完成");

    return true;
}

void WebSocketClient::loop()
{
    // 如果未初始化，直接返回，避免操作未初始化的对象
    if (!initialized) return;

    // 调用底层库的 loop() 方法，处理：
    //   - 接收网络数据
    //   - 解析 WebSocket 帧
    //   - 触发事件回调（onEvent）
    //   - 维护 TCP 连接状态
    //
    // 注意：这个方法必须频繁调用，否则可能导致：
    //   - 数据接收延迟
    //   - 心跳超时
    //   - 连接被服务器关闭
    webSocket.loop();

    // 自动重连逻辑
    // 条件检查：
    //   - autoReconnectEnabled: 用户启用了自动重连功能
    //   - !connected: 当前未连接
    //   - WiFi.isConnected(): WiFi 已连接（网络可用）
    if (autoReconnectEnabled && !connected && WiFi.isConnected())
    {
        // 获取当前时间戳
        unsigned long now = millis();

        // 检查是否到达重连时间间隔
        // 避免频繁重连，给服务器足够的恢复时间
        if (now - lastReconnectAttempt >= autoReconnectInterval)
        {
            // 更新上次重连尝试时间
            lastReconnectAttempt = now;

            // 输出调试信息
            Serial.println("🔄 WebSocket: 尝试自动重连...");

            // 重新发起 WebSocket 连接
            // 使用之前保存的连接参数（serverHost, serverPort, serverPath, useSSL）
            webSocket.begin(
                serverHost.c_str(),           // 服务器地址
                serverPort,                   // 端口号
                serverPath.c_str(),           // 路径
                useSSL ? "wss" : "ws"         // 协议类型
            );
            // 注意：连接成功或失败会通过 onEvent 回调通知
            // 连接成功后，connected 标志会在 handleWebSocketEvent 中被设置为 true
        }
    }

    // ==================== 4. 发送心跳保活消息 ====================
    // 定期发送心跳消息，保持连接活跃
    // 防止网络设备（如路由器 NAT）因长时间无数据而断开连接
    sendHeartbeatIfNeeded();
}

bool WebSocketClient::sendText(const String& message)
{
    if (!connected) {
        if (errorCallback) errorCallback("未连接，无法发送消息");
        return false;
    }

    // 使用 c_str() 将 String 转换为 const char*
    webSocket.sendTXT(message.c_str());
    Serial.printf("📤 WebSocket 发送: %s\n", message.c_str());
    return true;
}

bool WebSocketClient::sendJSON(const JsonDocument& json)
{
    String message;
    serializeJson(json, message);
    return sendText(message);
}

bool WebSocketClient::sendBinary(const uint8_t* data, size_t size)
{
    if (!connected) {
        if (errorCallback) errorCallback("未连接，无法发送二进制数据");
        return false;
    }

    webSocket.sendBIN(data, size);
    return true;
}

bool WebSocketClient::sendHeartbeat()
{
    if (!connected) return false;

    // 创建静态 JSON 文档（栈上分配，速度快，无内存碎片）
    // 根据内容大小分配足够的内存（约 100 字节足够）
    StaticJsonDocument<128> doc;
    doc["type"] = "heartbeat";
    doc["deviceId"] = deviceId;
    doc["timestamp"] = millis();

    String message;
    serializeJson(doc, message);

    webSocket.sendTXT(message);
    // 调试输出
    Serial.printf("💓 WebSocket 发送心跳: %s\n", message.c_str());
    return true;
}

void WebSocketClient::disconnect()
{
    if (connected) {
        webSocket.disconnect();
        connected = false;
        Serial.println("🔌 WebSocket: 已断开连接");
    }
}

bool WebSocketClient::isConnected() const
{
    return connected;
}

String WebSocketClient::getDeviceMac()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return {macStr};
}

String WebSocketClient::getDeviceId()
{
    String mac = getDeviceMac();
    mac.replace(":", "");
    return mac;
}

bool WebSocketClient::sendRegister()
{
    if (!connected) {
        if (errorCallback) errorCallback("未连接，无法发送注册信息");
        return false;
    }

    // 创建静态 JSON 文档（栈上分配，速度快，无内存碎片）
    // 根据内容大小分配足够的内存（约 100 字节足够）
    StaticJsonDocument<128> doc;
    doc["type"] = "register";
    doc["deviceId"] = deviceId;
    doc["mac"] = deviceMac;
    doc["timestamp"] = millis();

    JsonObject deviceInfo = doc["deviceInfo"].to<JsonObject>();
    deviceInfo["chipModel"] = ESP.getChipModel();
    deviceInfo["chipCores"] = ESP.getChipCores();
    deviceInfo["flashSize"] = ESP.getFlashChipSize() / (1024 * 1024);
    deviceInfo["freeHeap"] = ESP.getFreeHeap();
    deviceInfo["sketchSize"] = ESP.getSketchSize();
    deviceInfo["freeSketchSpace"] = ESP.getFreeSketchSpace();

    Serial.println("📝 WebSocket: 发送设备注册信息");
    return sendJSON(doc);
}

bool WebSocketClient::sendStatus(const int status)
{
    if (!connected) return false;

    StaticJsonDocument<128> doc;
    doc["type"] = status;
    doc["deviceId"] = deviceId;
    doc["timestamp"] = millis();

    return sendJSON(doc);
}

void WebSocketClient::onMessage(MessageCallback callback)
{
    // 使用 std::move 将参数移动到成员变量
    // 避免不必要的拷贝
    messageCallback = std::move(callback);
}

void WebSocketClient::onConnection(ConnectionCallback callback)
{

    connectionCallback = std::move(callback);
}

void WebSocketClient::onError(ErrorCallback callback)
{
    errorCallback = std::move(callback);
}

void WebSocketClient::setAutoReconnect(bool enabled, unsigned long interval)
{
    autoReconnectEnabled = enabled;
    autoReconnectInterval = interval;
    if (enabled) {
        lastReconnectAttempt = millis();
    }
}

void WebSocketClient::setHeartbeatInterval(unsigned long interval)
{
    heartbeatInterval = interval;
}

void WebSocketClient::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    // 根据事件类型进行不同的处理
    switch (type)
    {
    // ==================== 连接断开事件 ====================
    case WStype_DISCONNECTED:
        // 更新连接状态标志为 false
        connected = false;

        // 输出断开连接日志
        Serial.println("🔌 WebSocket: 已断开连接");

        // 触发连接状态回调，通知上层应用连接已断开
        if (connectionCallback) {
            connectionCallback(false);
        }
        break;

    // ==================== 连接成功事件 ====================
    case WStype_CONNECTED:
        // 更新连接状态标志为 true
        connected = true;

        // 记录心跳时间（连接成功也算作一次有效通信）
        lastHeartbeatTime = millis();

        // 输出连接成功日志
        Serial.println("✅ WebSocket: 连接成功！");

        // 触发连接状态回调，通知上层应用连接已建立
        if (connectionCallback) {
            connectionCallback(true);
        }

        // 自动发送设备注册信息
        // 向服务器报告设备身份，便于服务器识别和管理
        sendRegister();
        break;

    // ==================== 文本消息事件 ====================
    case WStype_TEXT:
    {
        // 将二进制负载转换为字符串（假设消息是 UTF-8 编码的文本）
        String message = String((char*)payload);

        // 输出收到的消息内容（调试用）
        Serial.printf("📩 WebSocket 收到消息: %s\n", message.c_str());

        // ---- 特殊消息处理：心跳响应 ----
        // 解析 JSON 消息，检查是否为心跳响应
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, message);

        if (!error) {
            // 获取消息类型字段，如果不存在则返回空字符串
            String msgType = doc["type"] | "";

            // 心跳响应不需要触发上层回调，只做记录即可
            // 这样可以避免上层应用收到大量无意义的心跳确认消息
            if (msgType == "heartbeat_ack") {
                // 心跳响应，静默处理，不触发消息回调
                // 此处可以添加心跳统计等逻辑
            }
        }

        // ---- 通用文本消息处理 ----
        // 将文本消息传递给上层应用的回调函数
        // 注意：心跳响应也会传递，如需过滤可在回调中根据消息类型判断
        if (messageCallback) {
            messageCallback(MessageType::TEXT, payload, length);
        }
        break;
    }

    // ==================== 二进制消息事件 ====================
    case WStype_BIN:
        // 输出二进制数据接收日志
        Serial.printf("📩 WebSocket 收到二进制数据: %d bytes\n", length);

        // 将二进制消息传递给上层应用的回调函数
        // 常见用途：接收音频数据、图片、文件等
        if (messageCallback) {
            messageCallback(MessageType::BINARY, payload, length);
        }
        break;

    // ==================== Ping 心跳请求事件 ====================
    case WStype_PING:
        // 输出心跳请求日志
        Serial.println("💓 WebSocket: 收到 PING");

        // 注意：WebSockets 库会自动回复 PONG 响应
        // 这里只需要记录日志，不需要手动发送响应
        // 如需监控连接质量，可以在此处添加统计代码
        break;

    // ==================== Pong 心跳响应事件 ====================
    case WStype_PONG:
        // 收到服务器的心跳响应，更新时间戳
        // 这个时间戳可用于判断连接是否活跃
        // 当长时间未收到 PONG 时，可以认为连接可能已断开
        lastHeartbeatTime = millis();

        // 不输出日志，避免频繁打印导致刷屏
        // 如需调试可取消下面的注释
        // Serial.println("💓 WebSocket: 收到 PONG");
        break;

    // ==================== 错误事件 ====================
    case WStype_ERROR:
        // 输出错误日志
        Serial.println("❌ WebSocket: 发生错误");

        // 触发错误回调，通知上层应用处理错误
        // 上层可以根据错误类型决定是否重连或显示错误信息
        if (errorCallback) {
            errorCallback("WebSocket 错误");
        }
        break;

    // ==================== 未知事件 ====================
    default:
        // 忽略未处理的事件类型
        // 这通常是 WebSockets 库的扩展事件或未使用的事件
        break;
    }
}

void WebSocketClient::sendHeartbeatIfNeeded()
{
    // ==================== 1. 检查是否需要发送心跳 ====================
    // 条件1: 必须已连接（未连接时无需发送心跳）
    // 条件2: 心跳间隔必须大于0（0表示禁用自动心跳）
    if (!connected || heartbeatInterval == 0) {
        return;  // 不满足条件，直接返回
    }

    // ==================== 2. 获取当前时间 ====================
    unsigned long now = millis();

    // ==================== 3. 判断是否到达发送时间 ====================
    // 计算距离上次发送心跳的时间差
    // 如果时间差 >= 心跳间隔，则需要发送新的心跳
    if (now - lastHeartbeatTime >= heartbeatInterval) {

        // ==================== 4. 发送心跳消息 ====================
        // 调用 sendHeartbeat() 发送心跳
        // 心跳消息格式: {"type":"heartbeat","deviceId":"xxx","timestamp":xxx}
        sendHeartbeat();

        // ==================== 5. 更新上次心跳时间 ====================
        // 记录本次发送时间，用于下次判断
        lastHeartbeatTime = now;
    }
}
