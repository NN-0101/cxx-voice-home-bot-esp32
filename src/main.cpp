#include <Arduino.h>

#include "DisplayScreen.h"
#include "PTZControl.h"
#include "VoicePlayback.h"
#include "MicrophoneCapture.h"  // 添加麦克风头文件

DisplayScreen displayScreen;
PTZControl ptzControl;
VoicePlayback voicePlayback;
MicrophoneCapture mic;  // 添加麦克风对象

// 添加状态变量
bool isAwakened = false;  // 设备是否已唤醒

/**
 * 唤醒词检测回调
 */
void onWakeWordDetected()
{
    Serial.println("\n🎙️ 检测到唤醒词！");
    isAwakened = true;

    // 播放提示音或语音（可选）
    // voicePlayback.playBeep();

    // 可选：在显示屏上显示
    // displayScreen.showText("已唤醒");
}

/**
 * 语音状态变化回调
 */
void onSpeechStateChanged(bool isSpeaking)
{
    if (isSpeaking)
    {
        Serial.println("🎤 开始录音...");
        // 可选：显示屏显示录音状态
        // displayScreen.showText("录音中...");
    }
    else
    {
        Serial.println("⏹️ 停止录音");
        // 可选：显示屏显示待机状态
        // displayScreen.showText("待机");
    }
}

/**
 * 音频数据回调（可选，如果需要处理音频数据）
 */
void onAudioDataReceived(uint8_t* data, size_t size, uint32_t timestamp)
{
    // 这里可以添加语音识别功能
    // 例如：将音频数据发送到服务器识别指令
    // 当前只是打印日志，不处理
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 2000)
    {
        Serial.printf("📡 收到音频数据: %d 字节\n", size);
        lastPrint = millis();
    }
}

/**
 * 初始化函数
 */
void setup() {
    // 初始化串口通信
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================");
    Serial.println("智能云台系统 - 语音唤醒版");
    Serial.println("==================================\n");

    // 1. 初始化音频播放
    if (!voicePlayback.begin())
    {
        Serial.println("❌ 初始化 I2S 音频系统失败！！！");
    } else {
        Serial.println("✅ 音频系统初始化成功");
    }

    // 2. 初始化显示屏
    if (!displayScreen.begin())
    {
        Serial.println("❌ 显示屏初始化失败！！！");
    } else {
        Serial.println("✅ 显示屏初始化成功");
    }
    delay(1000);

    // 3. 初始化舵机
    if (ptzControl.begin())
    {
        Serial.println("✅ 舵机初始化成功");
        ptzControl.searchMovement();
        voicePlayback.playHello();
    } else {
        Serial.println("❌ 舵机初始化失败");
    }

    // 4. 初始化麦克风（语音唤醒）
    Serial.println("\n🎤 初始化语音唤醒系统...");

    // 配置唤醒词
    std::vector<String> wakeWords;
    wakeWords.push_back("0101");
    wakeWords.push_back("洞幺洞幺");
    mic.setWakeWords(wakeWords);

    // 配置参数
    mic.setVADThreshold(200);      // 语音检测阈值
    mic.setSilenceTimeout(2000);   // 静音2秒后自动结束

    // 启用功能
    mic.enableWakeWord(true);      // 启用唤醒词检测
    mic.enableVAD(true);           // 启用语音活动检测

    // 注册回调函数
    mic.onWakeWord(onWakeWordDetected);
    mic.onSpeechState(onSpeechStateChanged);
    mic.onAudioData(onAudioDataReceived);

    // 初始化麦克风（使用测试成功的引脚：14, 13, 35）
    if (!mic.begin(14, 13, 35, 16000))
    {
        Serial.println("❌ 麦克风初始化失败！！！");
    } else {
        Serial.println("✅ 麦克风初始化成功");

        // 开始采集
        if (!mic.start())
        {
            Serial.println("❌ 启动采集失败！！！");
        } else {
            Serial.println("✅ 语音采集已启动");
        }
    }

    Serial.println("\n==================================");
    Serial.println("🎤 系统已就绪，等待唤醒词...");
    Serial.println("   唤醒词: \"0101\" 或 \"洞幺洞幺\"");
    Serial.println("==================================\n");
}

void loop() {
    // 这里保持原有逻辑，不做修改
    // 如果需要添加其他功能，可以在这里添加

    // 可选：定期打印状态（每10秒一次）
    static uint32_t lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 10000)
    {
        Serial.printf("状态: 采集=%s, 唤醒=%s, 音量=%d%%\n",
                      mic.isCapturing() ? "是" : "否",
                      isAwakened ? "是" : "否",
                      mic.getCurrentVolume());
        lastStatusPrint = millis();
    }

    delay(100);
}