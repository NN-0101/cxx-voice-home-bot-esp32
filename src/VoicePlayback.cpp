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
    if(!isInitialized) {
        Serial.println("❌ 音频系统未初始化");
        return false;
    }

    if(isPlaying) {
        Serial.println("⚠️ 正在播放中");
        return false;
    }

    Serial.printf("🔊 打开文件: %s\n", fileName);

    File audioFile = SPIFFS.open(fileName, "r");
    if(!audioFile) {
        Serial.printf("❌ 无法打开文件: %s\n", fileName);
        return false;
    }

    Serial.printf("✅ 文件打开成功，大小: %d bytes\n", audioFile.size());

    // 读取标准WAV头
    WAVHeader header;
    size_t bytesRead = audioFile.read((uint8_t*)&header, sizeof(header));
    Serial.printf("读取WAV头: %d / %d bytes\n", bytesRead, sizeof(header));

    if(bytesRead != sizeof(header)) {
        Serial.println("❌ 读取WAV头失败");
        audioFile.close();
        return false;
    }

    // 验证WAV格式
    if(strncmp(header.chunkID, "RIFF", 4) != 0) {
        Serial.println("❌ 无效的RIFF标识");
        audioFile.close();
        return false;
    }

    if(strncmp(header.format, "WAVE", 4) != 0) {
        Serial.println("❌ 无效的WAVE标识");
        audioFile.close();
        return false;
    }

    if(strncmp(header.subchunk1ID, "fmt ", 4) != 0) {
        Serial.println("❌ 无效的fmt标识");
        audioFile.close();
        return false;
    }

    if(header.audioFormat != 1) {
        Serial.printf("❌ 不支持的音频格式: %d\n", header.audioFormat);
        audioFile.close();
        return false;
    }

    Serial.printf("✅ WAV格式验证通过\n");
    Serial.printf("   采样率: %d Hz\n", header.sampleRate);
    Serial.printf("   声道数: %d\n", header.numChannels);
    Serial.printf("   位深度: %d bit\n", header.bitsPerSample);

    // 查找data块
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    // 从fmt块之后开始查找
    uint32_t currentPos = 12 + 8 + header.subchunk1Size;  // RIFF(12) + fmt(8) + fmt数据大小

    Serial.printf("从位置 %d 开始查找data块\n", currentPos);

    while(currentPos < audioFile.size()) {
        audioFile.seek(currentPos);

        char chunkID[5] = {0};
        uint32_t chunkSize;

        if(audioFile.read((uint8_t*)chunkID, 4) != 4) {
            Serial.println("读取块ID失败");
            break;
        }

        if(audioFile.read((uint8_t*)&chunkSize, 4) != 4) {
            Serial.println("读取块大小失败");
            break;
        }

        Serial.printf("找到块: %c%c%c%c, 大小: %d\n", chunkID[0], chunkID[1], chunkID[2], chunkID[3], chunkSize);

        if(strncmp(chunkID, "data", 4) == 0) {
            dataOffset = currentPos + 8;
            dataSize = chunkSize;
            Serial.printf("✅ 找到data块！偏移: %d, 大小: %d\n", dataOffset, dataSize);
            break;
        }

        // 跳过当前块
        currentPos += 8 + chunkSize;

        // 防止无限循环
        if(currentPos >= audioFile.size()) {
            break;
        }
    }

    if(dataSize == 0) {
        Serial.println("❌ 未找到data块");
        audioFile.close();
        return false;
    }

    // 跳转到数据开始位置
    audioFile.seek(dataOffset);

    // 配置I2S - 使用WAV文件的采样率
    i2s_set_clk(I2S_NUM_0, header.sampleRate,
                header.bitsPerSample == 16 ? I2S_BITS_PER_SAMPLE_16BIT : I2S_BITS_PER_SAMPLE_8BIT,
                header.numChannels == 2 ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

    isPlaying = true;

    // 播放音频数据
    uint8_t* buffer = (uint8_t*)malloc(bufferSize);
    if(!buffer) {
        Serial.println("❌ 内存分配失败");
        audioFile.close();
        isPlaying = false;
        return false;
    }

    size_t totalBytes = 0;
    size_t bytesWritten;
    uint32_t remaining = dataSize;

    Serial.println("🎵 开始播放...");

    while(remaining > 0 && isPlaying) {
        size_t toRead = (remaining < bufferSize) ? remaining : bufferSize;
        size_t readBytes = audioFile.read(buffer, toRead);
        if(readBytes > 0) {
            i2s_write(I2S_NUM_0, buffer, readBytes, &bytesWritten, portMAX_DELAY);
            totalBytes += bytesWritten;
            remaining -= readBytes;

            // 显示播放进度
            if(totalBytes % (header.sampleRate * header.blockAlign) == 0) {
                int seconds = totalBytes / header.byteRate;
                Serial.printf("   播放进度: %d 秒\n", seconds);
            }
        } else {
            break;
        }
    }

    free(buffer);
    audioFile.close();

    delay(100);
    i2s_zero_dma_buffer(I2S_NUM_0);

    Serial.printf("✅ 播放完成，共 %d bytes (%.1f 秒)\n", totalBytes, (float)totalBytes / header.byteRate);
    isPlaying = false;
    return true;
}