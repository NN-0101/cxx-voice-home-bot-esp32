//
// Created by 0101 on 2026/3/23.
//

#include "PTZControl.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <map>

PTZControl::PTZControl() : horizontalPin(SERVO_HORIZONTAL), verticalPin(SERVO_VERTICAL), currentHorizontal(90),
                           currentVertical(90), horizontalChannel(0), verticalChannel(1)
{
}

bool PTZControl::init(int hPin, int vPin)
{
    horizontalPin = hPin;
    verticalPin = vPin;

    Serial.println("初始化舵机（PWM模式）。。。");

    // 配置PWM通道
    horizontalChannel = 0;
    verticalChannel = 1;

    // 设置引脚为PWM输出
    // 配置水平舵机 PWM
    ledcSetup(horizontalChannel, freq, resolution);
    // 绑定引脚到 PWM 通道
    ledcAttachPin(horizontalPin, horizontalChannel);

    // 配置垂直舵机 PWM
    ledcSetup(verticalPin, freq, resolution);
    // 绑定引脚到 PWM 通道
    ledcAttachPin(verticalPin, verticalChannel);

    // 等待稳定
    delay(100);

    Serial.println("舵机初始化完成");
    return true;
}

void PTZControl::setHorizontal(int angle)
{
    // 角度限制
    angle = constrainAngle(angle);
    // 角度转PWM限制
    int pluse = angleToPulse(angle);
    // 输出PWM信号
    ledcWrite(horizontalChannel, pluse);
    // 记录当前角度
    currentHorizontal = angle;
    Serial.printf("水平舵机: %d°\n", angle);
}

void PTZControl::setVertical(int angle)
{
    angle = constrainAngle(angle);
    int pluse = angleToPulse(angle);
    ledcWrite(verticalChannel, pluse);
    currentVertical = angle;
    Serial.printf("垂直舵机: %d°\n", angle);
}

void PTZControl::center()
{
    Serial.println("舵机归中");
    setHorizontal(90);
    setVertical(90);
    delay(300);
}

void PTZControl::searchMovement()
{
    Serial.println("开始找人动作");

    Serial.println("1.左转");
    setHorizontal(60);
    delay(500);

    Serial.println("2.右转");
    setHorizontal(120);
    delay(500);

    Serial.println("3.回中");
    setHorizontal(100);
    delay(300);

    Serial.println("4.抬头");
    setVertical(110);
    delay(500);

    Serial.println("4.低头");
    setVertical(70);
    delay(500);

    Serial.println("5.回中");
    setVertical(50);
    delay(300);

    Serial.println("=== 找人动作完成 ===");
}

int PTZControl::angleToPulse(int angle)
{
    // 限制角度范围
    angle = constrain(angle, 0, 180);

    // 直接计算占空比值
    // 公式：占空比 = (脉冲宽度 × 分辨率) / 周期
    // 脉冲宽度 = minPulse + (angle × (maxPulse - minPulse) / 180)
    int pulseWidth = minPulse + (angle * (maxPulse - minPulse) / 180);
    int dutyCycle = (pulseWidth * ((1 << resolution) - 1)) / (1000000 / freq);

    return constrain(dutyCycle, 0, (1 << resolution) - 1);
}

int PTZControl::constrainAngle(int angle)
{
    return constrain(angle, 30, 150);
}
