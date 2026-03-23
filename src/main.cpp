#include <Arduino.h>

#include "DisplayScreen.h"
#include "PTZControl.h"
#include "VoicePlayback.h"

DisplayScreen displayScreen;
PTZControl ptzControl;
VoicePlayback voicePlayback;

/**
 * 初始化函数，在程序开始时自动执行一次
 */
void setup() {
    // 初始化串口通信，设置通信速率为115200波特
    // Serial - Arduino 的串口通信对象；.begin() - 初始化函数，启动串口通信；115200 - 波特率（bits per second），即每秒传输 115200 个数据位
    Serial.begin(115200);
    // 等待1000毫秒，给硬件足够的时间完成初始化
    delay(1000);

    // 初始化音频
    if (!voicePlayback.begin())
    {
        Serial.println("初始化 I2S 音频系统失败！！！");
    }

    // 初始化显示屏
    if (!displayScreen.begin())
    {
        Serial.println("显示屏初始化失败！！！");
    }
    delay(1000);

    // 初始化舵机
    if (ptzControl.begin())
    {
        ptzControl.searchMovement();

        // 播放hello语音
        voicePlayback.playHello();
    }

}

void loop() {
// write your code here
}