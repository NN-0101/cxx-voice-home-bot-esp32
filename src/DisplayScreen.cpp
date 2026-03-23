//
// Created by 0101 on 2026/3/23.
//

#include "DisplayScreen.h"

/**
 * 构造方法
 */
DisplayScreen::DisplayScreen()
    : display(SCREEN_WIDTH, SCREEN_HEIGHT,SCREEN_MOSI,SCREEN_SCLK,SCREEN_DC,SCREEN_RES,SCREEN_CS)
      , leftEyeX(20)
      , rightEyeX(78)
      , eyeY(16)
      , eyeWidth(28)
      , eyeHeight(32)
{
}

bool DisplayScreen::begin()
{
    Serial.println("初始化显示屏，显示眼睛");
    // 复位显示屏
    // 将复位引脚设置为输出模式，这样才能通过代码控制这个引脚输出高电平或低电平
    pinMode(SCREEN_RES, OUTPUT);
    // 将复位引脚输出低电平（OV），这会让OLED屏进入复位状态，内部电路重置，类似于按下电子设备的“复位按钮”
    digitalWrite(SCREEN_RES, LOW);
    // 等待100毫秒，确保复位信号持续时间足够长，让屏幕电路完全复位，太短有可能导致复位不成功
    delay(100);
    // 将复位引脚输出高电平（通常是3.3V或5V），释放复位状态，屏幕开始正常启动，相当于松开了“复位按钮”
    digitalWrite(SCREEN_RES, HIGH);
    // 再次等待100毫秒，让屏幕有足够时间完成内部初始化，稳定工作
    delay(100);

    // 为什么要这样做？
    // 硬件复位是必须的，因为：
    //      OLED 屏幕在上电后需要明确的复位时序才能正确初始化
    //      确保屏幕内部寄存器恢复到默认状态
    //      避免之前可能残留的错误配置影响当前运行
    //      提高系统启动的可靠性

    // 初始化 OLED 显示屏并检查是否成功 SSD1306_SWITCHCAPVCC表示这顶屏幕的电源模式，使用内部电荷泵来生成所需电压，
    // 对于大多数128x64 OLED显示屏来说，这是标准配置。另一种选项是 SSD1306_EXTERNALVCC 使用外部电源
    if (!display.begin(SSD1306_SWITCHCAPVCC))
    {
        Serial.println("OLED初始化失败");
        return false;
    }
    Serial.println("OLED初始化成功");
    // 清除内存缓冲区
    display.clearDisplay();
    // 开始显示眼睛
    showEyes();
    return true;
}


void DisplayScreen::showEyes()
{
    // 绘制左边眼睛
    drawNormalEye(leftEyeX, eyeY);
    // 绘制右边眼睛
    drawNormalEye(rightEyeX, eyeY);
    // 将之前在缓冲区中绘制的所有图形、文字等内容，一次性传输并显示到物理屏幕上。
    display.display();
}

void DisplayScreen::drawNormalEye(int x, int y)
{
    // 绘制眼眶（外轮廓）,画出一个圆角矩形的眼眶轮廓
    // drawRoundRect表示绘制圆角矩形，x, y表示矩形的左上坐标；eyeWidth, eyeHeight表示矩形的宽度和高度；10表示圆角的半径（像素）；SSD1306_WHITE表示白色，这里绘制的是边框（不填充内部）
    display.drawRoundRect(x, y, eyeWidth, eyeHeight, 10,SSD1306_WHITE);

    // 计算瞳孔中心位置 计算出眼眶的中心点，作为瞳孔的位置
    int pupilX = x + eyeWidth / 2; // 瞳孔X坐标 = 眼眶X + 宽度的一半
    int pupilY = y + eyeHeight / 2; // 瞳孔Y坐标 = 眼眶Y + 高度的一半

    // 绘制瞳孔 画一个较大的白色实心圆作为眼睛的"眼白"基础
    // fillCircle表示绘制实心圆；pupilX，pupilY表示圆心坐标（眼睛中心）；8表示半径8像素；SSD1306_WHITE表示白色填充
    display.fillCircle(pupilX, pupilY, 8,SSD1306_WHITE);

    // 绘制虹膜/瞳孔 在白色圆中心画一个黑色圆，形成瞳孔效果,由于黑色覆盖了白色的一部分，看起来就像瞳孔
    // 6表示半径 6 像素（比白色圆小 2 像素）；SSD1306_BLACK表示黑色填充
    display.fillCircle(pupilX, pupilY, 6,SSD1306_BLACK);

    // 绘制高光（反光点）
    // pupilX - 3, pupilY - 3表示圆心在瞳孔中心左上方偏移3像素；2表示半径2像素的小圆；SSD1306_WHITE表示白色填充
    display.fillCircle(pupilX - 3, pupilY - 3, 2,SSD1306_WHITE);
}
