//
// Created by 0101 on 2026/3/24.
//

#include "MicrophoneCapture.h"
#include <cmath>

MicrophoneCapture::MicrophoneCapture()
    : sckPin(I2S_SCK)
    , wsPin(I2S_WS)
    , dinPin(I2S_SD)
    , sampleRate(16000)
    , bitsPerSample(16)
    , channels(1)
    , frameSize(1024)
    , bufferSize(1024)
    , i2sPort(I2S_NUM_0)
    , capturing(false)
    , initialized(false)
    , wakeWordEnabled(true)
    , vadEnabled(true)
    , vadThreshold(200)
    , silenceTimeout(2000)
    , lastSoundTime(0)
    , isSpeaking(false)
    , isAwakened(false)  // 添加唤醒状态
    , currentPeak(0)
    , currentVolume(0)
    , lastPeak(0)
    , captureTaskHandle(nullptr)
    , i2sMutex(nullptr)
    , audioQueue(nullptr)
    , audioCallback(nullptr)
    , wakeWordCallback(nullptr)
    , speechStateCallback(nullptr)
{
    wakeWords.push_back("0101");
    wakeWords.push_back("洞幺洞幺");
}

MicrophoneCapture::~MicrophoneCapture()
{
    stop();
    if (initialized)
    {
        i2s_driver_uninstall(i2sPort);
    }
    if (i2sMutex != nullptr)
    {
        vSemaphoreDelete(i2sMutex);
    }
    if (audioQueue != nullptr)
    {
        vQueueDelete(audioQueue);
    }
}

bool MicrophoneCapture::begin(int sckPin_, int wsPin_, int dinPin_, int sampleRate_)
{
    this->sckPin = sckPin_;
    this->wsPin = wsPin_;
    this->dinPin = dinPin_;
    this->sampleRate = sampleRate_;
    this->frameSize = bufferSize;

    Serial.println("\n==================================");
    Serial.println("麦克风测试程序");
    Serial.println("==================================");

    i2sMutex = xSemaphoreCreateMutex();
    if (i2sMutex == nullptr)
    {
        Serial.println("❌ 创建互斥锁失败");
        return false;
    }

    if (!initI2S())
    {
        Serial.println("❌ I2S 初始化失败");
        return false;
    }

    initialized = true;

    Serial.println("✅ I2S 驱动安装成功");
    Serial.println("✅ I2S 引脚设置成功");
    Serial.println("\n🎤 麦克风初始化完成！");
    Serial.println("开始监听麦克风...");
    Serial.println("请对着麦克风说话或发出声音\n");
    Serial.println("==================================\n");

    return true;
}

bool MicrophoneCapture::initI2S()
{
    i2s_pin_config_t i2sPins = {
        .bck_io_num = sckPin,
        .ws_io_num = wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = dinPin
    };

    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = (uint32_t)sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = bufferSize,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_driver_uninstall(i2sPort);

    esp_err_t err = i2s_driver_install(i2sPort, &i2sConfig, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("❌ I2S 驱动安装失败: %d\n", err);
        return false;
    }

    err = i2s_set_pin(i2sPort, &i2sPins);
    if (err != ESP_OK)
    {
        Serial.printf("❌ I2S 引脚设置失败: %d\n", err);
        i2s_driver_uninstall(i2sPort);
        return false;
    }

    i2s_zero_dma_buffer(i2sPort);
    return true;
}

bool MicrophoneCapture::start()
{
    if (!initialized)
    {
        Serial.println("❌ 麦克风未初始化");
        return false;
    }

    if (capturing)
    {
        return true;
    }

    capturing = true;
    isAwakened = false;  // 初始状态未唤醒
    isSpeaking = false;

    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        captureTask,
        "MicCapture",
        8192,
        this,
        5,
        &captureTaskHandle,
        0
    );

    if (taskCreated != pdPASS)
    {
        Serial.println("❌ 创建采集任务失败");
        capturing = false;
        return false;
    }

    return true;
}

void MicrophoneCapture::stop()
{
    if (!capturing) return;

    capturing = false;

    if (captureTaskHandle != nullptr)
    {
        vTaskDelete(captureTaskHandle);
        captureTaskHandle = nullptr;
    }

    i2s_zero_dma_buffer(i2sPort);
}

bool MicrophoneCapture::isCapturing()
{
    return capturing;
}

void MicrophoneCapture::setWakeWords(const std::vector<String>& wakeWords_)
{
    this->wakeWords = wakeWords_;
}

void MicrophoneCapture::addWakeWord(const String& wakeWord)
{
    wakeWords.push_back(wakeWord);
}

void MicrophoneCapture::enableWakeWord(bool enabled)
{
    wakeWordEnabled = enabled;
    if (!enabled) {
        isAwakened = false;  // 禁用唤醒词时重置唤醒状态
    }
}

void MicrophoneCapture::enableVAD(bool enabled)
{
    vadEnabled = enabled;
}

void MicrophoneCapture::setVADThreshold(int threshold)
{
    vadThreshold = threshold;
}

void MicrophoneCapture::setSilenceTimeout(uint32_t timeoutMs)
{
    silenceTimeout = timeoutMs;
}

int MicrophoneCapture::getCurrentVolume()
{
    return currentVolume;
}

int16_t MicrophoneCapture::getPeakAmplitude()
{
    return currentPeak;
}

void MicrophoneCapture::onAudioData(AudioDataCallback callback)
{
    audioCallback = callback;
}

void MicrophoneCapture::onWakeWord(WakeWordCallback callback)
{
    wakeWordCallback = callback;
}

void MicrophoneCapture::onSpeechState(SpeechStateCallback callback)
{
    speechStateCallback = callback;
}

bool MicrophoneCapture::readAudioData(uint8_t* buffer, size_t bufferSize_, size_t* bytesRead)
{
    esp_err_t err = i2s_read(i2sPort, buffer, bufferSize_, bytesRead, portMAX_DELAY);
    return (err == ESP_OK);
}

void MicrophoneCapture::captureTask(void* parameter)
{
    MicrophoneCapture* mic = (MicrophoneCapture*)parameter;

    uint8_t* buffer = (uint8_t*)malloc(mic->bufferSize);
    if (buffer == NULL) {
        Serial.println("❌ 内存分配失败");
        vTaskDelete(NULL);
        return;
    }

    size_t bytesRead = 0;
    int lastPeak = 0;
    uint32_t lastWaitPrint = 0;
    uint32_t lastDebugPrint = 0;
    uint32_t lastWakeTime = 0;  // 添加：上次唤醒时间

    while (mic->capturing)
    {
        esp_err_t err = i2s_read(mic->i2sPort, buffer, mic->bufferSize, &bytesRead, portMAX_DELAY);

        if (err == ESP_OK && bytesRead > 0) {
            int16_t* samples = (int16_t*)buffer;
            int sampleCount = bytesRead / 2;

            // 计算音频峰值和平均幅度
            int16_t maxAmplitude = 0;
            int16_t minAmplitude = 0;
            long sumAmplitude = 0;

            for (int i = 0; i < sampleCount; i++) {
                int16_t sample = samples[i];
                if (sample > maxAmplitude) maxAmplitude = sample;
                if (sample < minAmplitude) minAmplitude = sample;
                sumAmplitude += abs(sample);
            }

            int16_t peak = maxAmplitude - minAmplitude;
            int16_t avg = sumAmplitude / sampleCount;

            mic->currentPeak = peak;
            int db = map(peak, 0, 30000, 0, 100);
            db = constrain(db, 0, 100);
            mic->currentVolume = db;

            // ========== 未唤醒状态：检测唤醒词 ==========
            if (!mic->isAwakened && mic->wakeWordEnabled)
            {
                // 简单版本：峰值超过阈值直接触发
                if (peak > 2800) {  // 可根据需要调整阈值
                    uint32_t now = millis();

                    // 避免连续触发（3秒内只触发一次）
                    if (now - lastWakeTime > 3000) {
                        lastWakeTime = now;

                        Serial.println("\n🎙️ ========== 检测到唤醒词！ ==========");
                        Serial.printf("   峰值: %d | 音量: %d%%\n", peak, db);

                        mic->isAwakened = true;
                        mic->lastSoundTime = millis();

                        // 触发唤醒回调
                        if (mic->wakeWordCallback) {
                            mic->wakeWordCallback();
                        }

                        // 触发语音状态回调
                        if (mic->speechStateCallback) {
                            mic->speechStateCallback(true);
                        }
                    }
                }

                // 未唤醒时的输出：只有较大声音才提示
                if (peak > 800 && millis() - lastDebugPrint > 1000) {
                    Serial.printf("[%6lu ms] 检测到声音: 峰值 %d | 音量 %d%%\n",
                                  millis(), peak, db);
                    lastDebugPrint = millis();
                }
            }

            // ========== 唤醒状态：处理语音指令 ==========
            if (mic->isAwakened)
            {
                // 检测是否有声音
                bool hasSound = (peak > mic->vadThreshold);

                if (hasSound)
                {
                    mic->lastSoundTime = millis();

                    if (!mic->isSpeaking)
                    {
                        mic->isSpeaking = true;
                        Serial.println("🎤 开始录音...");

                        if (mic->speechStateCallback) {
                            mic->speechStateCallback(true);
                        }
                    }

                    // 说话时打印音量条
                    if (millis() - lastWaitPrint > 100)
                    {
                        int barLength = map(peak, 0, 30000, 0, 50);
                        barLength = constrain(barLength, 0, 50);

                        unsigned long timeMs = millis();

                        Serial.printf("[%6lu ms] ", timeMs);

                        Serial.print("[");
                        for (int i = 0; i < barLength; i++) {
                            if (i < 30) Serial.print("=");
                            else Serial.print("#");
                        }
                        for (int i = barLength; i < 50; i++) {
                            Serial.print(" ");
                        }
                        Serial.print("] ");

                        Serial.printf("峰值: %5d | 音量: %3d%%\n", peak, db);

                        if (peak > lastPeak) {
                            lastPeak = peak;
                            Serial.printf("    🔥 峰值刷新: %d\n", lastPeak);
                        }

                        lastWaitPrint = millis();
                    }

                    // 发送音频数据回调
                    if (mic->audioCallback != nullptr) {
                        mic->audioCallback(buffer, bytesRead, millis());
                    }
                }
                else
                {
                    // 没有声音，检查是否超时
                    if (mic->isSpeaking &&
                        (millis() - mic->lastSoundTime > mic->silenceTimeout))
                    {
                        mic->isSpeaking = false;
                        mic->isAwakened = false;
                        Serial.println("⏹️ ========== 语音输入结束 ==========\n");

                        if (mic->speechStateCallback) {
                            mic->speechStateCallback(false);
                        }

                        lastPeak = 0;
                    }
                }
            }

            // 完全静音时的提示
            if (peak <= 200 && !mic->isAwakened && millis() - lastWaitPrint > 3000) {
                Serial.println("🎤 等待声音输入...");
                lastWaitPrint = millis();
            }
        }

        vTaskDelay(1);
    }

    free(buffer);
    vTaskDelete(NULL);
}