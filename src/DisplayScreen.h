//
// Created by 0101 on 2026/3/23.
//

#ifndef CXX_VOICE_HOME_BOT_ESP32_DISPLAYSCREEN_H
#define CXX_VOICE_HOME_BOT_ESP32_DISPLAYSCREEN_H

// 显示屏引脚定义
#define SCREEN_MOSI 22  // 板子上写的是DI
#define SCREEN_SCLK 21  // 板子上写的是DO
#define SCREEN_DC 15
#define SCREEN_CS 5
#define SCREEN_RES 2  // 复位引脚

// 显示屏长宽
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#include "Adafruit_SSD1306.h"


/**
 * 显示屏内容设置
 * 此处使用的显示屏是 GND0.96OLED显示屏 SSD1306 1315 128X64 SPI串口6Pin 3.3V 5V-SSD1353
 */
class DisplayScreen
{
    /**
     * 来自 Adafruit 的 SSD1306 OLED 显示屏驱动库。这个类封装了控制 SSD1306 驱动芯片的所有功能，包括初始化、绘图、刷新显示等操作
     */
    Adafruit_SSD1306 display;

    // 左右眼睛X坐标
    int leftEyeX, rightEyeX;

    // 眼睛Y坐标
    int eyeY;

    // 眼睛的长宽（大小）
    int eyeWidth, eyeHeight;

public:
    DisplayScreen();

    /**
     * 初始化
     */
    bool begin();

    /**
     *  显示眼睛
     */
    void showEyes();

    /**
     * 绘制普通没有表情的眼睛
     * @param x 矩形左上坐标x
     * @param y 矩形左上坐标y
     */
    void drawNormalEye(int x, int y);
};


#endif //CXX_VOICE_HOME_BOT_ESP32_DISPLAYSCREEN_H
