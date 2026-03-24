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
      , vadThreshold(500)
      , silenceTimeout(2000)
      , lastSoundTime(0)
      , isSpeaking(false)
      , currentPeak(0)
      , currentVolume(0)
      , captureTaskHandle(nullptr)
      , i2sMutex(nullptr)
      , audioQueue(nullptr)
      , audioCallback(nullptr)
      , wakeWordCallback(nullptr)
      , speechStateCallback(nullptr)
{
    // 默认唤醒词
    wakeWords.push_back("0101");
    wakeWords.push_back("洞幺洞幺");
}

MicrophoneCapture::~MicrophoneCapture()
{
    // 1. 停止音频采集
    // 作用：停止采集任务，设置 capturing = false，删除采集任务线程
    // 确保在释放硬件资源前，采集循环已经退出
    stop();

    // 2. 卸载 I2S 驱动（如果已初始化）
    // 条件判断：只有成功初始化过的设备才需要卸载
    // i2s_driver_uninstall() 会：
    //   - 释放 I2S 硬件占用的 DMA 缓冲区
    //   - 释放 I2S 中断资源
    //   - 关闭 I2S 外设时钟
    if (initialized)
    {
        i2s_driver_uninstall(i2sPort);
    }

    // 3. 删除互斥锁（如果已创建）
    // 互斥锁用于保护 I2S 资源的并发访问
    // vSemaphoreDelete() 会释放互斥锁占用的内存和内核资源
    if (i2sMutex != nullptr)
    {
        vSemaphoreDelete(i2sMutex);
        // 注意：不需要设置 i2sMutex = nullptr，因为对象即将销毁
    }

    // 4. 删除音频队列（如果已创建）
    // 音频队列用于在采集任务和主任务间传递数据
    // vQueueDelete() 会释放队列占用的内存
    if (audioQueue != nullptr)
    {
        vQueueDelete(audioQueue);
        // 注意：不需要设置 audioQueue = nullptr，因为对象即将销毁
    }

    // 对象销毁后，所有成员变量占用的内存也会自动释放
}

bool MicrophoneCapture::begin(int sckPin_, int wsPin_, int dinPin_, int sampleRate_)
{
    this->sckPin = sckPin_;
    this->wsPin = wsPin_;
    this->dinPin = dinPin_;
    this->sampleRate = sampleRate_;
    this->frameSize = bufferSize;

    Serial.println("🎤 初始化麦克风...");

    // 创建互斥锁
    i2sMutex = xSemaphoreCreateMutex();
    if (i2sMutex == nullptr)
    {
        Serial.println("❌ 创建 I2S 互斥锁失败");
        return false;
    }

    // 初始化 I2S
    if (!initI2S())
    {
        Serial.println("❌ I2S 初始化失败");
        return false;
    }

    initialized = true;
    Serial.println("✅ 麦克风初始化完成");
    Serial.printf("   采样率: %d Hz\n", sampleRate_);
    Serial.printf("   位深度: %d bit\n", bitsPerSample);
    Serial.printf("   唤醒词: ");
    for (const auto& word : wakeWords)
    {
        Serial.printf("%s ", word.c_str());
    }
    Serial.println();

    return true;
}

bool MicrophoneCapture::initI2S()
{
    // 配置 I2S 引脚
    i2s_pin_config_t i2sPins = {
        .bck_io_num = sckPin,
        .ws_io_num = wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = dinPin
    };

    // 配置 I2S 参数
    i2s_config_t i2sConfig;
    i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2sConfig.sample_rate = (uint32_t)sampleRate;
    i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sConfig.dma_buf_count = 8;
    i2sConfig.dma_buf_len = bufferSize / 2;
    i2sConfig.use_apll = false;
    i2sConfig.tx_desc_auto_clear = true;
    i2sConfig.fixed_mclk = 0;

    // 安装 I2S 驱动
    esp_err_t err = i2s_driver_install(i2sPort, &i2sConfig, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("❌ I2S 驱动安装失败: %d\n", err);
        return false;
    }

    // 设置 I2S 引脚
    err = i2s_set_pin(i2sPort, &i2sPins);
    if (err != ESP_OK)
    {
        Serial.printf("❌ I2S 引脚设置失败: %d\n", err);
        i2s_driver_uninstall(i2sPort);
        return false;
    }

    // 清空 DMA 缓冲区
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
        Serial.println("⚠️ 麦克风已在采集中");
        return true;
    }

    capturing = true;

    // 创建采集任务
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        captureTask, // 任务函数指针
        "MicCapture", // 任务名称
        8192, // 任务栈深度（字节）
        this, // 传递给任务的参数
        5, // 任务优先级
        &captureTaskHandle, // 任务句柄（输出）
        0 // 绑定的 CPU 核心
    );

    if (taskCreated != pdPASS)
    {
        Serial.println("❌ 创建采集任务失败");
        capturing = false;
        return false;
    }

    Serial.println("🎤 麦克风开始采集，等待唤醒词...");
    return true;
}

void MicrophoneCapture::stop()
{
    // 1. 检查采集状态
    // 如果已经停止，直接返回（幂等性设计，确保多次调用不会出错）
    if (!capturing) return;

    // 2. 设置采集标志为 false
    // 这个标志会被采集任务中的 while(mic->capturing) 循环检测到
    // 使采集任务能够正常退出循环，避免强制删除导致资源泄漏
    capturing = false;

    // 3. 删除采集任务
    // 检查任务句柄是否有效（避免删除不存在的任务）
    if (captureTaskHandle != nullptr)
    {
        // vTaskDelete() 会从 FreeRTOS 调度器中移除任务
        // 释放任务控制块（TCB）和任务栈占用的内存
        vTaskDelete(captureTaskHandle);

        // 将句柄设置为 nullptr，防止悬空指针（dangling pointer）
        // 避免后续误用已删除的任务句柄
        captureTaskHandle = nullptr;
    }

    // 4. 清空 I2S DMA 缓冲区
    // i2s_zero_dma_buffer() 会：
    //   - 清空 DMA 接收缓冲区中的所有残留数据
    //   - 重置 DMA 内部指针到起始位置
    //   - 确保下次启动时不会读到旧的音频数据
    //   - 避免缓冲区满导致的 DMA 中断异常
    i2s_zero_dma_buffer(i2sPort);
    Serial.println("⏹️ 麦克风停止采集");
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
    if (enabled)
    {
        Serial.println("🔊 唤醒词功能已启用");
    }
    else
    {
        Serial.println("🔇 唤醒词功能已禁用");
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
    // 1. 获取互斥锁（保护 I2S 资源）
    // xSemaphoreTake() 尝试获取互斥锁
    //   - 参数1: i2sMutex - 互斥锁句柄
    //   - 参数2: portMAX_DELAY - 无限等待，直到获取到锁
    // 返回值：pdTRUE = 成功获取锁，pdFALSE = 获取失败（超时或错误）
    // 如果获取锁失败，立即返回 false，表示读取失败
    if (xSemaphoreTake(i2sMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    // 2. 执行 I2S 读取操作（此时已持有互斥锁）
    // i2s_read() 从 I2S 硬件读取音频数据
    //   - i2sPort: I2S 端口号（I2S_NUM_0 或 I2S_NUM_1）
    //   - buffer: 数据存储缓冲区
    //   - bufferSize_: 缓冲区大小（字节）
    //   - bytesRead: 实际读取的字节数（输出）
    //   - portMAX_DELAY: 无限等待，直到有数据可用
    // 返回值：ESP_OK = 成功，其他值 = 错误码
    esp_err_t err = i2s_read(i2sPort, buffer, bufferSize_, bytesRead, portMAX_DELAY);

    // 3. 释放互斥锁
    // 无论读取成功还是失败，都必须释放锁
    // 让其他等待的任务有机会访问 I2S 资源
    // 防止死锁（deadlock）
    xSemaphoreGive(i2sMutex);

    // 4. 返回读取结果
    // 如果 err == ESP_OK，返回 true；否则返回 false
    return (err == ESP_OK);
}

int16_t MicrophoneCapture::calculateEnergy(int16_t* samples, int count)
{
    if (samples == nullptr || count <= 0)
    {
        return 0;
    }

    // 初始化统计变量
    long sum = 0;
    int16_t maxPeak = 0;

    // 遍历音频样本
    for (int i = 0; i < count; i++)
    {
        int16_t sample = abs(samples[i]);   // 取绝对值
        sum += sample;                      // 累加能量
        if (sample > maxPeak)
        {
            maxPeak = sample;               // 更新峰值
        }
    }

    // 保存原始峰值
    currentPeak = maxPeak;

    // 计算音量级别（0-100）
    // 根据实际测试，INMP441 在安静环境峰值为 100-500
    // 正常说话峰值为 1000-8000，大声说话可达 15000+
    const int MAX_EXPECTED_PEAK = 15000;  // 可根据实际环境调整

    int volume = map(maxPeak, 0, MAX_EXPECTED_PEAK, 0, 100);
    currentVolume = constrain(volume, 0, 100);

    // 可选：添加调试输出
    // static uint32_t lastPrint = 0;
    // if (millis() - lastPrint > 1000)
    // {
    //     Serial.printf("峰值: %d, 音量: %d%%, 平均能量: %d\n",
    //                   maxPeak, currentVolume, sum / count);
    //     lastPrint = millis();
    // }

    // 返回平均能量（用于 VAD）
    return sum / count;
}

bool MicrophoneCapture::detectWakeWord(int16_t* samples, int count)
{
    // 1. 检查唤醒词功能是否启用
    // 如果用户禁用了唤醒词功能，直接返回 false
    if (!wakeWordEnabled) return false;

    // 2. 检查唤醒词列表是否为空
    // 没有设置任何唤醒词时，无法进行检测
    if (wakeWords.empty()) return false;

    // 3. 计算当前音频段的平均能量
    // calculateEnergy() 会返回平均能量值，同时更新 currentPeak 和 currentVolume
    int16_t energy = calculateEnergy(samples, count);

    // 4. 唤醒词检测逻辑：基于能量的持续时间检测
    // 原理：有人喊唤醒词时，通常会持续发出较大的声音
    // 通过统计能量超过阈值的时间长度来判断

    // 静态变量：记录连续高能量的次数（函数调用间保持状态）
    static int highEnergyCount = 0;

    // 计算需要多少帧连续高能量才算检测到唤醒词
    // sampleRate / 100 表示约 1 秒的音频帧数
    // 例如：sampleRate = 16000，则 requiredHighEnergyCount = 160
    // 即需要连续 160 次检测到高能量（约 1 秒）
    const int requiredHighEnergyCount = sampleRate / 100;

    // 5. 判断当前能量是否足够高
    // 使用 vadThreshold * 3 作为唤醒词的能量阈值
    // 正常说话能量 > vadThreshold（判定为有声音）
    // 唤醒词需要 3 倍能量，确保只有大声喊才会触发
    if (energy > vadThreshold * 3)
    {
        // 能量足够高，增加计数器
        highEnergyCount++;

        // 检查是否达到所需的高能量次数
        if (highEnergyCount >= requiredHighEnergyCount)
        {
            // 检测成功！重置计数器，防止连续触发
            highEnergyCount = 0;

            // 输出调试信息
            Serial.printf("🎙️ 检测到唤醒词！能量: %d\n", energy);

            // 返回 true 表示检测到唤醒词
            return true;
        }
    }
    else
    {
        // 能量不够高，重置计数器
        // 如果中间有能量不足，说明声音中断，不是有效的唤醒词
        highEnergyCount = 0;
    }

    // 未检测到唤醒词
    return false;
}

bool MicrophoneCapture::isVoiceActivity(int16_t* samples, int count)
{
    // 1. 计算当前音频段的平均能量
    // calculateEnergy() 会：
    //   - 遍历所有样本，计算平均能量值
    //   - 同时更新 currentPeak（峰值）和 currentVolume（音量）
    //   - 返回值范围：0-32767
    int16_t energy = calculateEnergy(samples, count);

    // 2. 判断是否有语音活动
    // 将平均能量与预设的 VAD 阈值进行比较
    //   - 如果能量 > 阈值：判定为有声音（正在说话）
    //   - 如果能量 ≤ 阈值：判定为静音（没有说话）
    return energy > vadThreshold;
}

void MicrophoneCapture::captureTask(void* parameter)
{
    // ==================== 1. 初始化 ====================

    // 获取对象指针，用于访问类的成员变量和方法
    MicrophoneCapture* mic = (MicrophoneCapture*)parameter;

    // 分配音频数据缓冲区（动态分配，避免占用栈空间）
    uint8_t* buffer = (uint8_t*)malloc(mic->bufferSize);
    size_t bytesRead = 0;  // 实际读取的字节数

    // 用于唤醒词检测的环形缓冲区（缓存最近 1 秒的音频）
    std::vector<int16_t> wakeBuffer;
    wakeBuffer.reserve(mic->WAKE_BUFFER_SIZE);  // 预分配内存，提高性能

    // 检查缓冲区分配是否成功
    if (buffer == nullptr)
    {
        Serial.println("❌ 分配音频缓冲区失败");
        vTaskDelete(NULL);  // 删除当前任务
        return;
    }

    // 状态变量
    bool isAwakened = false;      // 设备是否已被唤醒
    uint32_t lastPrintTime = 0;   // 上次打印调试信息的时间

    // ==================== 2. 主采集循环 ====================
    while (mic->capturing)  // 检查采集标志，为 false 时退出循环
    {
        // ----- 2.1 读取音频数据 -----
        if (mic->readAudioData(buffer, mic->bufferSize, &bytesRead) && bytesRead > 0)
        {
            // 转换为 16 位有符号整数样本
            int16_t* samples = (int16_t*)buffer;
            int sampleCount = bytesRead / 2;  // 16位 = 2字节

            // ==================== 3. 唤醒词检测（未唤醒状态） ====================
            if (!isAwakened && mic->wakeWordEnabled)
            {
                // ----- 3.1 更新唤醒缓冲区（环形缓冲区）-----
                // 将新读取的样本添加到缓冲区末尾
                for (int i = 0; i < sampleCount; i++)
                {
                    wakeBuffer.push_back(samples[i]);

                    // 保持缓冲区大小不超过 WAKE_BUFFER_SIZE
                    // 超过时删除最早的样本（FIFO 行为）
                    if (wakeBuffer.size() > mic->WAKE_BUFFER_SIZE)
                    {
                        wakeBuffer.erase(wakeBuffer.begin());
                    }
                }

                // ----- 3.2 检测唤醒词 -----
                // 当缓冲区有足够数据时（至少半秒）才进行检测
                if (wakeBuffer.size() >= mic->WAKE_BUFFER_SIZE / 2)
                {
                    if (mic->detectWakeWord(wakeBuffer.data(), wakeBuffer.size()))
                    {
                        // 检测到唤醒词，设备进入唤醒状态
                        isAwakened = true;
                        mic->lastSoundTime = millis();  // 记录最后声音时间

                        // 触发唤醒回调
                        if (mic->wakeWordCallback)
                        {
                            mic->wakeWordCallback();
                        }

                        Serial.println("🎙️ 设备已唤醒，开始听指令...");

                        // 触发语音状态回调（开始说话）
                        if (mic->speechStateCallback)
                        {
                            mic->speechStateCallback(true);
                        }
                    }
                }
            }

            // ==================== 4. 唤醒后处理 ====================
            if (isAwakened)
            {
                // ----- 4.1 语音活动检测（VAD）-----
                if (mic->vadEnabled)
                {
                    bool isTalking = mic->isVoiceActivity(samples, sampleCount);

                    if (isTalking)
                    {
                        // 检测到声音，更新最后声音时间
                        mic->lastSoundTime = millis();

                        // 如果之前是静音状态，现在开始说话
                        if (!mic->isSpeaking)
                        {
                            mic->isSpeaking = true;
                            Serial.println("🎤 检测到语音输入...");

                            // 触发语音状态回调（开始说话）
                            if (mic->speechStateCallback)
                            {
                                mic->speechStateCallback(true);
                            }
                        }
                    }
                    else
                    {
                        // 当前是静音，检查是否超时
                        if (mic->isSpeaking &&
                            (millis() - mic->lastSoundTime > mic->silenceTimeout))
                        {
                            // 静音超时，结束语音会话，设备回到待机状态
                            mic->isSpeaking = false;
                            isAwakened = false;  // 重要：回到待唤醒状态
                            Serial.println("⏹️ 语音输入结束，设备进入待机");

                            // 触发语音状态回调（结束说话）
                            if (mic->speechStateCallback)
                            {
                                mic->speechStateCallback(false);
                            }
                        }
                    }
                }

                // ----- 4.2 发送音频数据（仅在说话时发送）-----
                // 只有在唤醒状态且正在说话时，才将音频数据传递给上层
                if (mic->audioCallback != nullptr && mic->isSpeaking)
                {
                    mic->audioCallback(buffer, bytesRead, millis());
                }
            }

            // ==================== 5. 调试信息输出 ====================
            // 每秒打印一次音量信息（音量 > 5% 时）
            if (mic->currentVolume > 5 && millis() - lastPrintTime > 1000)
            {
                if (isAwakened)
                {
                    // 唤醒状态：显示音量
                    Serial.printf("🎤 音量: %d%% | 峰值: %d (已唤醒)\n",
                                  mic->currentVolume, mic->currentPeak);
                }
                else if (mic->currentVolume > 20)
                {
                    // 待唤醒状态：只显示较大音量（避免刷屏）
                    Serial.printf("🔊 音量: %d%% | 峰值: %d (待唤醒)\n",
                                  mic->currentVolume, mic->currentPeak);
                }
                lastPrintTime = millis();
            }
        }

        // ----- 6. 任务调度 -----
        // 让出 CPU，避免独占处理器
        // 延迟 1 个 tick（通常 1-10 毫秒），平衡实时性和系统负载
        vTaskDelay(1);
    }

    // ==================== 7. 任务退出和清理 ====================
    // 循环结束（capturing 变为 false），释放资源并删除任务
    free(buffer);          // 释放音频缓冲区
    vTaskDelete(NULL);     // 删除当前任务（自删除）
}