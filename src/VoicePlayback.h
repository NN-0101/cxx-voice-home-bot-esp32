//
// Created by 0101 on 2026/3/23.
//

#ifndef CXX_VOICE_HOME_BOT_ESP32_VOICEPLAYBACK_H
#define CXX_VOICE_HOME_BOT_ESP32_VOICEPLAYBACK_H

#include <Arduino.h>
#include <FS.h>

// MAX98357 I2S 引脚定义
#define I2S_BCLK 26  // Bit Clock - 位时钟引脚
#define I2S_LRC  25  // Left/Right Clock - 左右声道时钟引脚（也叫 WS）
#define I2S_DIN  27  // Data Input - 数据输入引脚


/**
 * 语音播放，播放WAV音频文件
 * 此处用的是 MAX98357 I2S音频放大器模块 ESP32 STM32数字功放板小智AI带喇叭
 */
class VoicePlayback
{
    /**
     * 是否正在播放音乐
     */
    bool isPlaying;

    /**
     * 是否初始化
     */
    bool isInitialized;

    /**
     * 缓冲区大小（字节）
     */
    const int bufferSize = 1024;

    /**
     * WAV 文件头结构
     */
    struct WAVHeader
    {
        /**
         * RIFF 文件标识
         */
        char chunkID[4];

        /**
         * 文件大小
         */
        uint32_t chunkSize;

        /**
         * WAVE 格式标识
         */
        char format[4];

        /**
         * fmt 格式快标识
         */
        char subchunk1ID[4];

        /**
         * 格式块大小（通常16）
         */
        uint32_t subchunk1Size;

        /**
         * 音频格式（1=PCM）
         */
        uint16_t audioFormat;

        /**
         * 声道数（1=单声道，2=立体声）
         */
        uint16_t numChannels;

        /**
         * 采样率（如16000,44100Hz）
         */
        uint32_t sampleRate;

        /**
         * 字节率 = sampleRate × numChannels × bitsPerSample/8
         */
        uint32_t byteRate;

        /**
         * 块对齐 = numChannels × bitsPerSample/8
         */
        uint16_t blockAlign;

        /**
         * 位深度（如 16位）
         */
        uint16_t bitsPerSample;

        /**
         * "data" - 数据块标识
         */
        char subchunk2ID[4];

        /**
         * 音频数据大小
         */
        uint32_t subchunk2Size;
    };

public:
    /**
     * 构造函数
     */
    VoicePlayback();

    /**
     * 析构函数
     */
    ~VoicePlayback();

    /**
     * 初始化音频系统 初始化 I2S 总线 挂载 SPIFFS 文件系统 设置初始状态
     * @return
     */
    bool begin();

    /**
     * 播放“你好”提示音
     */
    void playHello();

    /**
     * 停止播放
     */
    void stop();

    /**
     * 查询是否在播放
     * @return 是否在播放
     */
    bool isPlayingSound();

private:

    /**
     * 初始化 SPIFFS 文件系统
     * @return 是否成功
     */
    bool initSPIFFS();

    /**
     * 播放指定 WAV 文件
     * @param fileName 文件名称
     * @return 是否播放成功
     */
    bool playWAV(const char* fileName);
};


#endif //CXX_VOICE_HOME_BOT_ESP32_VOICEPLAYBACK_H
