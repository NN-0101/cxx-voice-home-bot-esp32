//
// Created by 0101 on 2026/3/24.
//
#ifndef CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H
#define CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// I2S 麦克风引脚定义
#define I2S_SCK  14   // 位时钟 (BCLK)
#define I2S_WS   13   // 帧时钟 (LRC/WS)
#define I2S_SD   12   // 数据输入 (DIN)

struct AudioFrame {
    uint8_t* data;      // 音频数据指针
    size_t size;        // 数据大小
    uint32_t timestamp; // 时间戳
};

/**
 * 麦克风采集
 * 此处用的是 INMP441全向麦克风模块 MEMS 高精度 低功耗 I2S接口
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
     * @param data 音频数据指针
     * @param size 数据大小（字节）
     * @param timestamp 时间戳（毫秒）
     */
    typedef std::function<void(uint8_t* data, size_t size, uint32_t timestamp)> AudioDataCallback;

    /**
     * 初始化麦克风
     * @param sckPin_ 位时钟引脚（BCLK）
     * @param wsPin_ 帧时钟引脚（LRC/WS）
     * @param dinPin_ 数据输入引脚（DIN）
     * @param sampleRate_ 采样率（默认16000 Hz）
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
     * @return 是否正在采集
     */
    bool isCapturing();

    /**
     * 设置音频数据回调
     * @param callback 回调函数
     */
    void onAudioData(AudioDataCallback callback);

    /**
     * 设置 VAD（语音活动检测）阈值
     * @param threshold 阈值（0-10000，默认50）
     */
    void setVADThreshold(int threshold);

    /**
     * 启用/禁用VAD（静音检测）
     * @param enabled 是否启用
     */
    void enableVAD(bool enabled);

    /**
     * 获取当前音量级别（0-100）
     * @return 音量级别
     */
    int getCurrentVolume();

    /**
     * 获取音频峰值
     * @return 峰值幅度
     */
    int16_t getPeakAmplitude();

    /**
     * 获取采样率
     * @return 采样率
     */
    int getSampleRate() { return sampleRate; }

    /**
     * 获取没帧数据大小
     * @return 帧大小（字节）
     */
    int getFrameSize() { return frameSize; }

private:
    // I2S 配置
    int sckPin; // I2S 位时钟引脚（BCLK/SCK），用于同步数据位传输
    int wsPin; // I2S 字选择引脚（LRC/WS），用于标识左右声道切换
    int dinPin; // I2S 数据输入引脚（DIN/SD），从麦克风接收音频数据
    int sampleRate; // 采样率（Hz），如16000表示每秒采集16000个样本
    int bitsPerSample; // 每个样本的位深度，通常为16位或24位
    int channels; // 声道数，1=单声道，2=立体声
    int frameSize; // 每帧数据大小（字节），通常 = (bitsPerSample/8) * channels * 样本数/帧
    int bufferSize; // 内部缓冲区大小（字节），用于临时存储音频数据

    // I2S 句柄
    i2s_port_t i2sPort; // I2S 端口号，ESP32支持I2S_NUM_0和I2S_NUM_1两个端口

    // 采集状态
    bool capturing; // 是否正在采集音频数据
    bool initialized; // 麦克风是否已成功初始化
    bool vadEnabled; // 是否启用VAD（语音活动检测），用于自动检测说话状态
    int vadThreshold; // VAD能量阈值，高于此值判定为有声音，低于则为静音

    // 音频数据
    int16_t currentPeak; // 当前音频峰值幅度（绝对值），用于音量表显示
    int currentVolume; // 当前音量级别（0-100），基于能量计算后的归一化值

    // 任务相关
    TaskHandle_t captureTaskHandle; // FreeRTOS任务句柄，用于管理音频采集任务
    SemaphoreHandle_t i2sMutex; // I2S互斥锁，保护I2S资源的并发访问
    QueueHandle_t audioQueue; // 音频数据队列，用于在采集任务和主任务间传递数据

    // 回调函数
    AudioDataCallback audioCallback; // 音频数据回调函数，当采集到新数据时调用，用于向上层传递数据

    /**
     * 初始化麦克风
     * @return 是否成功
     */
    bool initI2S();

    /**
     * 从I2S 读取音频数据
     * @param buffer 数据缓冲区
     * @param bufferSize_ 缓冲区大小
     * @param bytesRead 实际读取的字节数
     * @return 是否读取成功
     */
    bool readAudioData(uint8_t* buffer, int bufferSize_, size_t* bytesRead);

    /**
     * 计算音频能量
     * @param samples 16位音频样本数组
     * @param count 样本数量
     * @return 平均能量值
     */
    int16_t calculateEnergy(int16_t* samples,int count);

    /**
     * 采集任务（在独立县城中运行）
     */
    static void captureTask(void* parameter);
};


#endif //CXX_VOICE_HOME_BOT_ESP32_MICROPHONECAPTURE_H
