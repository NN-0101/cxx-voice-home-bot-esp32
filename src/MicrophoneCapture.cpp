//
// Created by 0101 on 2026/3/24.
//

#include "MicrophoneCapture.h"

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
      , vadEnabled(true)
      , vadThreshold(true)
      , currentPeak(0)
      , currentVolume(0)
      , captureTaskHandle(nullptr)
      , i2sMutex(nullptr)
      , audioQueue(nullptr)
      , audioCallback(nullptr)
{
}

MicrophoneCapture::~MicrophoneCapture()
{
    stop();
    if (initialized)
    {
        // 释放 I2S 硬件占用的资源（DMA 缓冲区、中断等）
        i2s_driver_uninstall(i2sPort);
    }
    if (i2sMutex != nullptr)
    {
        //  删除信号量，释放内核资源
        vSemaphoreDelete(i2sMutex);
    }
    if (audioQueue != nullptr)
    {
        // 删除队列，释放队列占用的内存
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
    Serial.println("🎤初始化麦克风");

    // 创建互斥锁
    i2sMutex = xSemaphoreCreateMutex();
    if (i2sMutex == nullptr)
    {
        Serial.println("❌ 创建 I2S 互斥锁失败");
        return false;
    }

    // 创建音频数据队列
    audioQueue = xQueueCreate(10, sizeof(AudioFrame));
    if (audioQueue == nullptr)
    {
        Serial.println("❌ 创建音频队列失败");
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
    Serial.printf("   采样率: %d Hz\n", sampleRate);
    Serial.printf("   位深度: %d bit\n", bitsPerSample);
    Serial.printf("   声道数: %d\n", channels);
    Serial.printf("   帧大小: %d bytes\n", frameSize);

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
    // captureTask: 任务入口函数，在 captureTask() 中实现了音频采集循环
    // "MicCapture": 任务名称，便于调试时识别
    // 4096: 栈大小（4KB），足够存放局部变量和函数调用栈
    // this: 将当前对象指针传递给任务，让任务能访问对象的成员变量和方法
    // 5: 优先级（范围 0-24），5 是中等优先级，确保音频采集能及时处理
    // &captureTaskHandle: 保存任务句柄，用于后续删除任务
    // 0: 指定在 ESP32 的 0 号核心上运行，避免与 WiFi/蓝牙任务冲突
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
            captureTask,
            "MicCapture",
            4096,
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
    // 检查是否正在采集 如果已经停止，直接返回（幂等性设计）避免重复执行清理操作
    if (!capturing) return;

    // 停止采集标志 通知采集任务循环应该退出, captureTask() 中，while (mic->capturing) 循环会检测到这个变化并退出
    capturing = false;

    // 删除采集任务
    if (captureTaskHandle != nullptr)
    {
        vTaskDelete(captureTaskHandle);
        captureTaskHandle = nullptr;
    }
    // 清空 DMA 缓冲区
    i2s_zero_dma_buffer(i2sPort);
    Serial.println("⏹️ 麦克风停止采集");
}

bool MicrophoneCapture::isCapturing()
{
    return capturing;
}

void MicrophoneCapture::onAudioData(AudioDataCallback callback)
{
    audioCallback = callback;
}

void MicrophoneCapture::setVADThreshold(int threshold)
{
    vadThreshold = threshold;
}

void MicrophoneCapture::enableVAD(bool enabled)
{
    vadEnabled = enabled;
}

int MicrophoneCapture::getCurrentVolume()
{
    return currentVolume;
}

int16_t MicrophoneCapture::getPeakAmplitude()
{
    return currentPeak;
}

bool MicrophoneCapture::initI2S()
{
    // 配置 I2S 引脚（麦克风作为输入）
    i2s_pin_config_t i2sPins = {
        .bck_io_num = sckPin,               // 位时钟
        .ws_io_num = wsPin,                 // 帧时钟
        .data_out_num = I2S_PIN_NO_CHANGE,  // 不使用数据输出
        .data_in_num = dinPin               // 数据输入
    };

    // 配置 I2S 参数（接收模式）- 使用显式赋值方式
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
    if (err != ESP_OK) {
        Serial.printf("❌ I2S 驱动安装失败: %d\n", err);
        return false;
    }

    // 设置 I2S 引脚
    err = i2s_set_pin(i2sPort, &i2sPins);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S 引脚设置失败: %d\n", err);
        i2s_driver_uninstall(i2sPort);
        return false;
    }

    // 清空 DMA 缓冲区
    i2s_zero_dma_buffer(i2sPort);

    return true;
}

bool MicrophoneCapture::readAudioData(uint8_t* buffer, int bufferSize_, size_t* bytesRead)
{
    // 获取互斥锁，如果获取失败则返回 false
    if (xSemaphoreTake(i2sMutex, portMAX_DELAY) != pdTRUE)
    {
        return false;
    }

    // 成功获取锁，执行 I2S 读取
    esp_err_t err = i2s_read(i2sPort, buffer, bufferSize_, bytesRead, portMAX_DELAY);

    // 释放互斥锁
    xSemaphoreGive(i2sMutex);

    // 返回读取是否成功
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

void MicrophoneCapture::captureTask(void* parameter)
{
    // 任务参数获取和内存分配 将 void* 参数转换为具体的类指针 这样任务可以访问类的所有成员变量和方法
    // 动态分配音频数据缓冲区（通常 1024 字节）,使用 malloc 而非静态数组，节省栈空间,每个任务独立拥有自己的缓冲区，避免数据竞争
    MicrophoneCapture* mic = (MicrophoneCapture*)parameter;
    uint8_t* buffer = (uint8_t*)malloc(mic->bufferSize);
    size_t bytesRead = 0;

    // 内存分配失败处理
    if (buffer == nullptr)
    {
        Serial.println("❌ 分配音频缓冲区失败");
        // 删除当前任务（vTaskDelete(NULL) 删除自身）,防止后续代码使用空指针
        vTaskDelete(NULL);
        return;
    }

    uint32_t lastPrintTime = 0;

    // 主采集循环，持续运行直到 capturing 标志变为 false，通过 stop() 函数可以安全退出循环
    while (mic -> capturing)
    {
        // 读取音频数据，调用 readAudioData() 从 I2S 读取音频，检查读取是否成功且有数据，如果失败或没有数据，跳过本次循环
        if (mic -> readAudioData(buffer,mic->bufferSize,&bytesRead) && bytesRead > 0)
        {
            // VAD（语音活动检测）
            if (mic -> vadEnabled)
            {
                int16_t* samples = (int16_t*)buffer;
                // 16位样本 = 2字节
                int sampleCount = bytesRead / 2;
                int16_t energy = mic -> calculateEnergy(samples,sampleCount);

                // 静音检测：能量低于阈值则跳过发送
                if (energy < mic -> vadThreshold)
                {
                    // 每3秒打印一次静音状态
                    if (millis() - lastPrintTime > 3000)
                    {
                        Serial.printf("🔇 静音中 (能量: %d)\n", energy);
                        lastPrintTime = millis();
                    }
                    continue;
                }
            }


            // 调用回调函数
            if (mic->audioCallback != nullptr)
            {
                mic -> audioCallback(buffer,bytesRead,millis());
            }

            // 可选 打印音量信息
            if (mic -> currentVolume > 10 && millis() - lastPrintTime > 500)
            {
                Serial.printf("🎤 音量: %d%% | 峰值: %d\n",
                                              mic->currentVolume, mic->currentPeak);
                lastPrintTime = millis();
            }
        }
        // 任务调度，主动让出 CPU：延迟 1 个 tick（通常 1-10 毫秒），避免任务独占 CPU，让其他任务运行
        vTaskDelay(1);
    }
    // 任务清理,循环结束后释放缓冲区内存,删除任务自身（vTaskDelete(NULL)）,防止内存泄漏
    free(buffer);
    vTaskDelete(NULL);
}
