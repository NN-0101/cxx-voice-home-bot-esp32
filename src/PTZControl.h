//
// Created by 0101 on 2026/3/23.
//

#ifndef CXX_VOICE_HOME_BOT_ESP32_PTZCONTROL_H
#define CXX_VOICE_HOME_BOT_ESP32_PTZCONTROL_H

// 二维云台舵机引脚定义
#define SERVO_HORIZONTAL 19 // 水平舵机 GPIO19
#define SERVO_VERTICAL 18   // 垂直舵机 GPIO18


/**
 * 云台控制
 * 此处使用的是二维云台 金属二自由度双轴云台智能小车机器人FPV航模摄像头 舵机云台支架,
 * 这里使用的是PWM来控制
 * PWM 是 Pulse Width Modulation（脉冲宽度调制）的缩写，是一种通过快速开关信号来控制输出功率的技术。
 */
class PTZControl
{
    /**
     * 水平舵机引脚号（GPIO）
     */
    int horizontalPin;

    /**
     * 垂直舵机引脚号（GPIO）
     */
    int verticalPin;

    /**
     * 当前水平舵机角度
     */
    int currentHorizontal;

    /**
     * 当前垂直舵机角度
     */
    int currentVertical;

    /**
     * 水平舵机PWM通道号
     */
    int horizontalChannel;

    /**
     * 垂直舵机PWM通道号
     */
    int verticalChannel;

    /**
     * 50HZ PWM频率（标准舵机频率）
     */
    const int freq = 50;

    /**
     * 10 位分辨率（0-1023，共1024级）
     */
    const int resolution = 10;

    /**
     * 最小脉冲宽度 0.5ms（对应0度）
     */
    const int minPulse = 500;

    /**
     * 最大脉冲宽度2.4ms（对应180度）
     */
    const int maxPulse = 2400;

public:
    /**
     * 构造函数
     */
    PTZControl();

    /**
     * 初始化舵机控制系统
     * @param hPin 水平引脚号
     * @param vPin 垂直引脚号
     * @return 是否成功
     */
    bool begin(int hPin = SERVO_HORIZONTAL, int vPin = SERVO_VERTICAL);

    /**
     * 设置水平舵机角度（通常控制左右看） 角度值（0-180度） 将角度转换为脉冲宽度，输出 PWM
     * @param angle 角度
     */
    void setHorizontal(int angle);

    /**
     * 设置垂直舵机角度（通常控制上下看）角度值（0-180度）
     * @param angle 角度
     */
    void setVertical(int angle);

    /**
     * 复位舵机位置，将两个舵机都设置到中心位置（90度）
     */
    void center();

    /**
     * 执行搜索动作（可能是左右/上下摆动）模拟眼睛扫视或搜索目标的动作
     */
    void searchMovement();

private:

    /**
     * 角度转脉冲 将角度（0-180）转换为 PWM 脉冲宽度（500-2400 微秒）
     * @param angle 角度
     * @return 脉冲
     */
    int angleToPulse(int angle);

    /**
     * 限制角度在有效范围内（0-180度）
     * @param angle 角度
     * @return 角度
     */
    int constrainAngle(int angle);
};


#endif //CXX_VOICE_HOME_BOT_ESP32_PTZCONTROL_H
