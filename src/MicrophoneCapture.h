//
// Created by 0101 on 2026/3/24.
//

#ifndef CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H
#define CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <vector>

// I2S 麦克风引脚定义
#define I2S_SCK  14   // 位时钟 (BCLK)
#define I2S_WS   13   // 帧时钟 (LRC/WS)
#define I2S_SD   12   // 数据输入 (DIN)


/**
 * 麦克风采集类
 * 支持语音唤醒功能 此处使用的是 此处用的是 INMP441全向麦克风模块 MEMS 高精度 低功耗 I2S接口
 */
class MicrophoneCapture
{
public:
    /**
     * 构造函数
     */
    MicrophoneCapture();

    /**
     * 析构函数
     */
    ~MicrophoneCapture();

    /**
     * 音频数据回调函数类型
     */
    typedef std::function<void(uint8_t* data, size_t size, uint32_t timestamp)> AudioDataCallback;

    /**
     * 唤醒回调函数类型
     */
    typedef std::function<void()> WakeWordCallback;

    /**
     * 语音状态回调函数类型
     * @param isSpeaking 是否正在说话
     */
    typedef std::function<void(bool isSpeaking)> SpeechStateCallback;

    /**
     * 初始化麦克风
     * @param sckPin_ 位时钟引脚
     * @param wsPin_ 帧时钟引脚
     * @param dinPin_ 数据输入引脚
     * @param sampleRate_ 采样率
     * @return 是否初始化成功
     */
    bool begin(int sckPin_ = I2S_SCK, int wsPin_ = I2S_WS, int dinPin_ = I2S_SD, int sampleRate_ = 16000);

    /**
     * 开始采集
     * @return 是否成功开始
     */
    bool start();

    /**
     * 停止采集
     */
    void stop();

    /**
     * 是否正在采集
     */
    bool isCapturing();

    /**
     * 设置唤醒词
     * @param wakeWords_ 唤醒词列表（如 {"洞幺洞幺", "0101"}）
     */
    void setWakeWords(const std::vector<String>& wakeWords_);

    /**
     * 添加唤醒词
     * @param wakeWord 唤醒词
     */
    void addWakeWord(const String& wakeWord);

    /**
     * 启用/禁用唤醒功能
     * @param enabled 是否启用
     */
    void enableWakeWord(bool enabled);

    /**
     * 启用/禁用 VAD（语音活动检测）
     * @param enabled 是否启用
     */
    void enableVAD(bool enabled);

    /**
     * 设置 VAD 阈值
     * @param threshold 阈值（0-10000）
     */
    void setVADThreshold(int threshold);

    /**
     * 设置静音超时时间（毫秒）
     * @param timeoutMs 超时时间
     */
    void setSilenceTimeout(uint32_t timeoutMs);

    /**
     * 获取当前音量级别（0-100）
     */
    int getCurrentVolume();

    /**
     * 获取音频峰值
     */
    int16_t getPeakAmplitude();

    /**
     * 获取采样率
     */
    int getSampleRate() { return sampleRate; }

    /**
     * 设置音频数据回调
     */
    void onAudioData(AudioDataCallback callback);

    /**
     * 设置唤醒回调
     */
    void onWakeWord(WakeWordCallback callback);

    /**
     * 设置语音状态回调
     */
    void onSpeechState(SpeechStateCallback callback);

private:
    // I2S 配置
    int sckPin; // I2S 位时钟引脚（BCLK/SCK），用于同步数据传输时钟
    int wsPin; // I2S 字选择引脚（LRC/WS），标识左右声道（高电平左声道，低电平右声道）
    int dinPin; // I2S 数据输入引脚（DIN/SD），从麦克风接收音频数据
    int sampleRate; // 音频采样率（Hz），如 16000 表示每秒采集 16000 个样本
    int bitsPerSample; // 每个样本的位深度，通常为 16 位或 24 位
    int channels; // 声道数，1=单声道（左声道），2=立体声（左右声道）
    int frameSize; // 每帧数据大小（字节），通常 = (bitsPerSample/8) * channels * 样本数/帧
    int bufferSize; // 内部缓冲区大小（字节），用于临时存储读取的音频数据
    i2s_port_t i2sPort; // I2S 硬件端口号，ESP32 支持 I2S_NUM_0 和 I2S_NUM_1

    // 采集状态
    bool capturing; // 是否正在采集音频数据（控制采集循环的运行）
    bool initialized; // 麦克风硬件是否已成功初始化
    bool wakeWordEnabled; // 是否启用唤醒词检测功能
    bool vadEnabled; // 是否启用 VAD（语音活动检测），用于自动检测说话状态
    int vadThreshold; // VAD 能量阈值，高于此值判定为有声音，低于则为静音（范围 0-10000）
    uint32_t silenceTimeout; // 静音超时时间（毫秒），超过此时间无声音则自动结束语音会话
    uint32_t lastSoundTime; // 最后一次检测到声音的时间戳（毫秒），用于判断静音超时
    bool isSpeaking; // 当前是否正在说话（用于语音状态跟踪）

    // 唤醒词相关
    std::vector<String> wakeWords; // 唤醒词列表，如 {"0101", "洞幺洞幺", "小爱同学"}，支持多个唤醒词
    std::vector<int16_t> audioBuffer; // 用于唤醒词检测的音频数据缓冲区，存储最近一段时间的音频样本
    static const int WAKE_BUFFER_SIZE = 16000; // 唤醒检测缓冲区大小（样本数），16000 样本 ≈ 1 秒音频（16kHz采样率）

    // 音频数据统计
    int16_t currentPeak; // 当前音频峰值幅度（绝对值），范围 0-32767，用于音量表显示
    int currentVolume; // 当前音量级别（0-100），基于峰值映射后的归一化音量值

    // 任务和同步相关
    TaskHandle_t captureTaskHandle; // FreeRTOS 任务句柄，用于管理音频采集任务的创建和删除
    SemaphoreHandle_t i2sMutex; // I2S 互斥锁（Mutex），保护 I2S 硬件资源的并发访问，防止多任务冲突
    QueueHandle_t audioQueue; // 音频数据队列（当前未使用），用于在采集任务和主任务间传递数据

    // 回调函数
    AudioDataCallback audioCallback; // 音频数据回调函数，采集到有效音频时调用，用于上层处理（如发送到服务器）
    WakeWordCallback wakeWordCallback; // 唤醒词检测回调函数，检测到唤醒词时调用，通知上层设备已被唤醒
    SpeechStateCallback speechStateCallback; // 语音状态回调函数，说话开始/结束时调用，用于 UI 显示或状态管理

    /**
     * 初始化 I2S
     */
    bool initI2S();

    /**
    * 从 I2S 麦克风读取音频数据（线程安全版本）
    * @param buffer 存储音频数据的缓冲区指针
    * @param bufferSize_ 缓冲区大小（字节）
    * @param bytesRead 输出参数，返回实际读取的字节数
    * @return true: 读取成功，false: 读取失败
    */
    bool readAudioData(uint8_t* buffer, size_t bufferSize_, size_t* bytesRead);

    /**
     * 计算音频能量和音量统计信息
     * 分析音频样本数组，计算平均能量、峰值幅度和归一化音量
     *
     * @param samples 16位有符号整数音频样本数组（范围：-32768 到 32767）
     * @param count 样本数量
     * @return 平均能量值（范围：0-32767），用于 VAD（语音活动检测）
     */
    int16_t calculateEnergy(int16_t* samples, int count);

    /**
    * 检测唤醒词（简化版）
    * 通过检测持续的高能量音频来判断是否有人喊唤醒词
    *
    * 注意：这是一个简化实现，实际应用中应该集成更复杂的唤醒词检测模型
    * 如：语音识别引擎、神经网络模型等
    *
    * @param samples 音频样本数组（16位有符号整数）
    * @param count 样本数量
    * @return true: 检测到唤醒词，false: 未检测到
    */
    bool detectWakeWord(int16_t* samples, int count);

    /**
    * 简单语音活动检测（VAD - Voice Activity Detection）
    * 判断当前音频段是否包含人声
    *
    * @param samples 音频样本数组（16位有符号整数）
    * @param count 样本数量
    * @return true: 检测到语音活动（有声音），false: 静音
    */
    bool isVoiceActivity(int16_t* samples, int count);

    /**
    * 音频采集任务（在独立 FreeRTOS 线程中运行）
    * 负责从 I2S 麦克风持续读取音频数据，进行唤醒词检测和语音活动检测
    *
    * @param parameter 指向 MicrophoneCapture 对象的指针
    */
    static void captureTask(void* parameter);
};

#endif //CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H
