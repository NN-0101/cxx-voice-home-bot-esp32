//
// Created by 0101 on 2026/3/23.
//

#include "VoicePlayback.h"
#include <SPIFFS.h>
#include <driver/i2s.h>


VoicePlayback::VoicePlayback() : isPlaying(false), isInitialized(false)
{
}

VoicePlayback::~VoicePlayback()
{
    stop();
}

bool VoicePlayback::begin()
{
    Serial.println("🎵 初始化 I2S 音频系统...");

    // 1. 初始化文件系统
    if (!initSPIFFS()) {
        return false;
    }

    // 2. 配置 I2S 参数
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    // 3. 配置 I2S 引脚
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    // 4. 安装 I2S 驱动
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S 驱动安装失败: %d\n", err);
        return false;
    }

    // 5. 设置 I2S 引脚
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("❌ I2S 引脚设置失败: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);  // 清理已安装的驱动
        return false;
    }

    isInitialized = true;
    Serial.println("✅ 音频系统初始化完成");
    return true;
}

void VoicePlayback::playHello()
{
    if (isPlaying) {
        Serial.println("⚠️ 正在播放中，请稍后再试");
        return;
    }

    Serial.println("\n🎵 播放: 你好呀 🎵");

    // 生成随机数 1-5
    int randomNum = random(1, 1);
    // 构建文件名
    char fileName[32];
    sprintf(fileName, "/hello%d.wav", randomNum);
    if (!playWAV(fileName)) {
        Serial.println("❌ 播放失败");
    } else {
        Serial.println("✅ 播放完成\n");
    }
}

void VoicePlayback::stop()
{
    if (!isPlaying) return;

    isPlaying = false;
    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("⏹️ 播放已停止");
}

bool VoicePlayback::isPlayingSound()
{
    return isPlaying;
}

bool VoicePlayback::initSPIFFS()
{
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ SPIFFS 挂载失败");
        return false;
    }

    Serial.println("✅ SPIFFS 挂载成功");

    // 列出所有文件（调试用）
    // File root = SPIFFS.open("/");
    // if (!root) {
    //     Serial.println("⚠️ 无法打开根目录");
    //     return true;
    // }
    //
    // File file = root.openNextFile();
    // int fileCount = 0;
    //
    // while (file) {
    //     Serial.printf("   📁 %s (%d 字节)\n", file.name(), file.size());
    //     file = root.openNextFile();
    //     fileCount++;
    // }
    // root.close();
    //
    // if (fileCount == 0) {
    //     Serial.println("   ⚠️ 警告: SPIFFS 中没有音频文件");
    // } else {
    //     Serial.printf("   📊 共 %d 个文件\n", fileCount);
    // }

    return true;
}

bool VoicePlayback::playWAV(const char* fileName)
{
    // 检查初始化状态
    if (!isInitialized) {
        Serial.println("❌ 音频系统未初始化");
        return false;
    }

    // 打开文件
    File audioFile = SPIFFS.open(fileName, "r");
    if (!audioFile) {
        Serial.printf("❌ 无法打开文件: %s\n", fileName);
        return false;
    }

    Serial.printf("📂 打开文件: %s (%d 字节)\n", fileName, audioFile.size());

    // 读取并验证 WAV 头
    WAVHeader header;
    if (audioFile.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
        Serial.println("❌ 读取 WAV 头失败");
        audioFile.close();
        return false;
    }

    // 验证 RIFF 标识
    if (memcmp(header.chunkID, "RIFF", 4) != 0) {
        Serial.println("❌ 无效的 RIFF 标识");
        audioFile.close();
        return false;
    }

    // 验证 WAVE 标识
    if (memcmp(header.format, "WAVE", 4) != 0) {
        Serial.println("❌ 无效的 WAVE 标识");
        audioFile.close();
        return false;
    }

    // 验证 fmt 标识
    if (memcmp(header.subchunk1ID, "fmt ", 4) != 0) {
        Serial.println("❌ 无效的 fmt 标识");
        audioFile.close();
        return false;
    }

    // 验证音频格式（必须是 PCM）
    if (header.audioFormat != 1) {
        Serial.printf("❌ 不支持的音频格式: %d (仅支持 PCM)\n", header.audioFormat);
        audioFile.close();
        return false;
    }

    // 打印音频信息
    Serial.println("✅ WAV 格式验证通过");
    Serial.printf("   📊 采样率: %d Hz\n", header.sampleRate);
    Serial.printf("   🔊 声道数: %d\n", header.numChannels);
    Serial.printf("   📀 位深度: %d bit\n", header.bitsPerSample);
    Serial.printf("   💾 数据大小: %d 字节 (%.2f 秒)\n",
                  header.subchunk2Size,
                  (float)header.subchunk2Size / header.byteRate);

    // 查找 data 块（某些 WAV 文件可能有额外的块）
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
    uint32_t currentPos = sizeof(header);  // 已读取完整头，当前位置在头之后

    // 如果 subchunk2ID 不是 "data"，说明有额外块
    if (memcmp(header.subchunk2ID, "data", 4) != 0) {
        Serial.println("⚠️ 检测到额外块，正在查找 data 块...");

        // 从当前位置开始查找
        while (currentPos < audioFile.size()) {
            char chunkID[5] = {0};
            uint32_t chunkSize;

            if (audioFile.read((uint8_t*)chunkID, 4) != 4) break;
            if (audioFile.read((uint8_t*)&chunkSize, 4) != 4) break;

            if (memcmp(chunkID, "data", 4) == 0) {
                dataOffset = audioFile.position();
                dataSize = chunkSize;
                Serial.printf("✅ 找到 data 块，偏移: %d, 大小: %d\n", dataOffset, dataSize);
                break;
            }

            // 跳过当前块
            audioFile.seek(currentPos + 8 + chunkSize);
            currentPos = audioFile.position();
        }
    } else {
        // 标准格式，直接使用头中的信息
        dataOffset = audioFile.position();
        dataSize = header.subchunk2Size;
        Serial.printf("✅ data 块偏移: %d, 大小: %d\n", dataOffset, dataSize);
    }

    if (dataSize == 0) {
        Serial.println("❌ 未找到 data 块");
        audioFile.close();
        return false;
    }

    // 配置 I2S 时钟（使用 WAV 文件的采样率）
    i2s_set_clk(I2S_NUM_0, header.sampleRate,
                (header.bitsPerSample == 16) ? I2S_BITS_PER_SAMPLE_16BIT : I2S_BITS_PER_SAMPLE_8BIT,
                (header.numChannels == 2) ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

    // 分配缓冲区
    uint8_t* buffer = (uint8_t*)malloc(bufferSize);
    if (!buffer) {
        Serial.println("❌ 内存分配失败");
        audioFile.close();
        return false;
    }

    // 播放音频
    isPlaying = true;
    size_t totalBytes = 0;
    size_t bytesWritten;
    uint32_t remaining = dataSize;

    Serial.println("🎵 开始播放...");
    unsigned long startTime = millis();

    while (remaining > 0) {
        size_t toRead = (remaining < bufferSize) ? remaining : bufferSize;
        size_t readBytes = audioFile.read(buffer, toRead);

        if (readBytes > 0) {
            i2s_write(I2S_NUM_0, buffer, readBytes, &bytesWritten, portMAX_DELAY);
            totalBytes += bytesWritten;
            remaining -= readBytes;

            // 每 0.5 秒显示一次进度
            if (millis() - startTime > 500) {
                int progress = (totalBytes * 100) / dataSize;
                Serial.printf("   播放进度: %d%%\n", progress);
                startTime = millis();
            }
        } else {
            break;
        }
    }

    // 清理
    free(buffer);
    audioFile.close();

    // 确保 DMA 缓冲区清空
    i2s_zero_dma_buffer(I2S_NUM_0);
    delay(50);

    isPlaying = false;
    Serial.printf("✅ 播放完成，共 %d 字节 (%.2f 秒)\n",
                  totalBytes, (float)totalBytes / header.byteRate);

    return true;
}