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
#define I2S_SD   35   // 数据输入 (DIN)

class MicrophoneCapture
{
public:
    MicrophoneCapture();
    ~MicrophoneCapture();

    typedef std::function<void(uint8_t* data, size_t size, uint32_t timestamp)> AudioDataCallback;
    typedef std::function<void()> WakeWordCallback;
    typedef std::function<void(bool isSpeaking)> SpeechStateCallback;

    bool begin(int sckPin_ = I2S_SCK, int wsPin_ = I2S_WS, int dinPin_ = I2S_SD, int sampleRate_ = 16000);
    bool start();
    void stop();
    bool isCapturing();

    void setWakeWords(const std::vector<String>& wakeWords_);
    void addWakeWord(const String& wakeWord);
    void enableWakeWord(bool enabled);
    void enableVAD(bool enabled);
    void setVADThreshold(int threshold);
    void setSilenceTimeout(uint32_t timeoutMs);

    int getCurrentVolume();
    int16_t getPeakAmplitude();
    int getSampleRate() { return sampleRate; }

    void onAudioData(AudioDataCallback callback);
    void onWakeWord(WakeWordCallback callback);
    void onSpeechState(SpeechStateCallback callback);

private:
    int sckPin;
    int wsPin;
    int dinPin;
    int sampleRate;
    int bitsPerSample = 16;
    int channels = 1;
    int frameSize;
    int bufferSize = 1024;
    i2s_port_t i2sPort = I2S_NUM_0;

    bool capturing;
    bool initialized;
    bool wakeWordEnabled;
    bool vadEnabled;
    int vadThreshold = 200;
    uint32_t silenceTimeout;
    uint32_t lastSoundTime;
    bool isSpeaking;

    std::vector<String> wakeWords;
    static const int WAKE_BUFFER_SIZE = 16000;

    int16_t currentPeak;
    int currentVolume;
    bool isAwakened;  // 添加：设备是否已被唤醒
    int lastPeak;     // 添加：峰值记录

    TaskHandle_t captureTaskHandle;
    SemaphoreHandle_t i2sMutex;
    QueueHandle_t audioQueue;

    AudioDataCallback audioCallback;
    WakeWordCallback wakeWordCallback;
    SpeechStateCallback speechStateCallback;

    bool initI2S();
    bool readAudioData(uint8_t* buffer, size_t bufferSize_, size_t* bytesRead);
    static void captureTask(void* parameter);
};

#endif