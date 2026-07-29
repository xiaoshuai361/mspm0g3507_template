#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdio.h>

#include "bsp_adc.h"
#include "bluetooth_command_test.h"
#include "delay.h"
#include "dl1a_test.h"
#include "Grayscale_Sensor.h"
#include "key5d_test.h"
#include "line_trace.h"
#include "line_trace_test.h"
#include "menu_test.h"
#include "motor.h"
#include "Encoder.h"
#include "oled.h"
#include "uart.h"

volatile uint32_t g_key5d_self_test_failures; /**< 五向按键自检失败项数量。 */
volatile uint32_t g_key5d_self_test_complete; /**< 五向按键自检已执行标志。 */
volatile uint32_t g_menu_self_test_failures; /**< 菜单模块自检失败项数量。 */
volatile uint32_t g_menu_self_test_complete; /**< 菜单模块自检已执行标志。 */
volatile uint32_t g_dl1a_self_test_failures; /**< DL1A 测距模块自检失败项数量。 */
volatile uint32_t g_dl1a_self_test_complete; /**< DL1A 测距模块自检已执行标志。 */
volatile uint32_t g_line_trace_self_test_failures; /**< 灰度循迹解码自检失败项数量。 */
volatile uint32_t g_line_trace_self_test_complete; /**< 灰度循迹解码自检已执行标志。 */
volatile uint32_t g_bt_command_self_test_failures; /**< 蓝牙车辆命令解析自检失败项数量。 */
volatile uint32_t g_bt_command_self_test_complete; /**< 蓝牙车辆命令解析自检已执行标志。 */

static uint32_t lastBatteryTick; /**< 上一次电池电压采样时间戳。 */

/**
 * @brief 输出上电软件自检结果。
 * @param 无。
 * @note 用于确认 Key5D、Menu、DL1A、循迹和蓝牙命令解析逻辑是否异常。
 * @retval 无。
 */
static void App_LogSelfTests(void)
{
    char message[96];

    /* 汇总各模块自检结果，方便模板工程上电后直接从 UART0 判断状态。 */
    (void)snprintf(message, sizeof(message),
                   "Self-test Key5D=%lu Menu=%lu DL1A=%lu Line=%lu BT=%lu\r\n",
                   (unsigned long)g_key5d_self_test_failures,
                   (unsigned long)g_menu_self_test_failures,
                   (unsigned long)g_dl1a_self_test_failures,
                   (unsigned long)g_line_trace_self_test_failures,
                   (unsigned long)g_bt_command_self_test_failures);
    uart0_send_string(message);
}

/*
 * ================================================================
 *  Task 1 —— 题2：单圈循迹 + 精确停在A
 *
 *  指标：单圈 ≤20s，停车偏差 ≤2cm
 *  操作：菜单选中 Task 1 后按右键启动
 *
 *  算法：
 *    直道（方法一）：P=1.2, 死区±3, 低通α=0.7, 自校准偏置, 限幅±150, 缓变30/frame
 *    弯道（待测）：IMU gyro_z 检测，P=3.0, 无死区, 低通α=0.3, 限幅±300, 变速60/frame
 *    横切线：≥3路黑 + 任意连续3路黑 → 1帧确认 → 停车
 *    丢线：  保持最后偏差方向+逐步加大找线，60帧仍丢线→停车
 *    自校准：前200帧(跳过起步20帧)，|err|<5时采样，trimBias = -avg×1.5
 * ================================================================
 */
void App_Task1Run(void)
{
    enum { T1_IDLE, T1_TRACKING, T1_STOPPED };

    const uint32_t now = BSP_Delay_GetTick();
    uint8_t  raw;
    char     line[17];

    /* 持久状态 */
    static uint8_t  state = T1_IDLE;
    static uint8_t  lastActive;
    static uint32_t lastTick;
    static uint32_t startTick;
    static uint16_t crossLockout;       /* 横切线锁定期(帧) */
    static uint8_t  crossConfirm;       /* 横切线确认计数 */
    static uint32_t detectCount;        /* 累计检测次数 */
    static int16_t  sLeft = 600;        /* 整形后左PWM */
    static int16_t  sRight = 600;       /* 整形后右PWM */
    static float    prevErr;            /* 偏差低通滤波历史 */
    static int16_t  trimBias;           /* 自校准偏置(PWM) */
    static int32_t  calibSum;           /* 校准偏差累积 */
    static uint16_t calibCnt;           /* 校准采样帧数 */
    static uint16_t calFrame;           /* 启动后帧计数 */

    /* 临时变量 */
    uint8_t  activeCount;               /* 检测到黑线的通道数 */
    uint8_t  onCurve = 0U;              /* 弯道标志 */
    int16_t  errorTenths;               /* 加权偏差(0.1路间距) */

    /* ---- 菜单重入检测 ---- */
    if (lastActive != 1U) {
        lastActive = 1U;  state = T1_IDLE;  lastTick = 0U;
        sLeft=600; sRight=600; detectCount=0U; crossLockout=0U; crossConfirm=0U;
        prevErr=0.0f; trimBias=0; calibSum=0; calibCnt=0; calFrame=0;
        Set_Speed(0, 0);
        LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
    }

    /* ---- 按键检测(不限速) ---- */
    {
        Key5D_Event ev = KEY5D_EVENT_NONE;
        if (App_InputPoll(now, &ev) && ev == KEY5D_EVENT_PRESSED
            && App_InputGetStableKey() == KEY5D_KEY_RIGHT
            && state == T1_IDLE) {
            state = T1_TRACKING;
            Encoder_Init();
            LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
            detectCount=0U; sLeft=600; sRight=600; prevErr=0.0f; trimBias=0;
            calibSum=0; calibCnt=0; calFrame=0; startTick=now;
            uart0_send_string("T1: START\r\n");
        }
    }

    /* IDLE状态不做后续处理 */
    if (state == T1_IDLE) { Set_Speed(0, 0); return; }

    /* ---- 20ms 固定节拍 ---- */
    if ((uint32_t)(now - lastTick) < 20U) return;
    lastTick = now;

    /* ---- 灰度采样 + 偏差计算 ---- */
    Grayscale_Read();
    raw = Grayscale_GetRaw();
    (void)LineTrace_CalcActiveLowWeightedError(raw, &errorTenths, &activeCount);

    /* 连续自适应：增益和速度随 |偏差| 平滑调节 */

    /* ---- 横切线检测 ---- */
    if (state == T1_TRACKING &&
        LineTrace_DetectCrossLine(raw, activeCount, &crossLockout, &crossConfirm)
        == CROSS_LINE_DETECTED) {
        detectCount++; Set_Speed(0, 0); state = T1_STOPPED;
        { uint32_t t=(uint32_t)(now-startTick);
          char m[32]; (void)snprintf(m,sizeof(m),"T1: A! %lu.%lus\r\n",
            (unsigned long)(t/1000U),(unsigned long)((t%1000U)/100U));
          uart0_send_string(m); }
    }

    /* ---- 循迹修正 ---- */
    if (state == T1_TRACKING) {
        int16_t err = errorTenths;
        static uint8_t lostCnt; float fe;

        if (activeCount == 0U) {
            lostCnt++;
            err = (prevErr > 0.0f) ? (int16_t)(prevErr * 1.5f) : (int16_t)(prevErr * 1.5f);
            if (lostCnt >= 60U) { Set_Speed(0, 0); state = T1_STOPPED; uart0_send_string("T1: LOST\r\n"); }
        } else {
            lostCnt = 0U;
            if (err >= -3 && err <= 3) err = 0;
            fe = (float)err;
            fe = 0.6f * prevErr + 0.4f * fe;
            prevErr = fe; err = (int16_t)fe;
        }

        /* 自校准 */
        calFrame++;
        if (calibCnt < 300U && calFrame > 20U && err >= -5 && err <= 5) { calibSum += (int32_t)err; calibCnt++; }
        if (calibCnt > 0U) trimBias = (int16_t)((-calibSum * 3) / ((int32_t)calibCnt * 2));

        /* 自适应：曲率 = 滤波后的 |偏差|，增益和速度随之变化 */
        {
            static float adaptLevel;
            int16_t absErr = (err >= 0) ? err : (int16_t)(-err);
            adaptLevel = 0.9f * adaptLevel + 0.1f * (float)absErr;

            /* gain = 1.0 + level×0.3, speed = 600 - level×20, rate = 30 + level×3 */
            int16_t ag = (int16_t)(10.0f + adaptLevel * 3.0f);   /* ×0.1, 范围 10~70 */
            int16_t bp = (int16_t)(600.0f - adaptLevel * 20.0f); /* 范围 600~200 */
            int16_t sm = (int16_t)(150.0f + adaptLevel * 10.0f); /* 范围 150~350 */
            int16_t sr = (int16_t)(30.0f + adaptLevel * 3.0f);   /* 范围 30~90 */
            if (ag < 10) ag = 10; if (ag > 70) ag = 70;
            if (bp < 200) bp = 200; if (bp > 600) bp = 600;
            if (sm < 150) sm = 150; if (sm > 350) sm = 350;
            if (sr < 30) sr = 30; if (sr > 90) sr = 90;

            int16_t corr = (int16_t)(((int32_t)err * ag) / 10) + trimBias;
            if (corr > sm) corr = sm; if (corr < -sm) corr = -sm;

            int16_t tl = (int16_t)(bp + corr), tr = (int16_t)(bp - corr), d;
            if (tl < 0) tl = 0; if (tl > 1800) tl = 1800;
            if (tr < 0) tr = 0; if (tr > 1800) tr = 1800;

            d = (int16_t)(tl - sLeft); if (d > sr) sLeft += sr; else if (d < -sr) sLeft -= sr; else sLeft = tl;
            d = (int16_t)(tr - sRight); if (d > sr) sRight += sr; else if (d < -sr) sRight -= sr; else sRight = tr;
            Set_Speed((int)sLeft, (int)sRight);
        }
    }

    if (state == T1_STOPPED) Set_Speed(0, 0);

    /* ======== UART0 遥测(200ms周期) ======== */
    {
        static uint32_t telemTick;
        if ((uint32_t)(now - telemTick) >= 200U) {
            telemTick = now;
            char m[100];
            (void)snprintf(m, sizeof(m),
                "T1|st=%u|raw=0x%02X|e=%d|a=%u|C=%d|T=%d|P=%d/%d|L=%d|R=%d\r\n",
                (unsigned)state, (unsigned)raw, (int)errorTenths,
                (unsigned)activeCount, 0, (int)trimBias,
                (int)sLeft, (int)sRight,
                (int)(crossLockout > 0U ? 1 : 0),
                (int)(detectCount));
            uart0_send_string(m);
        }
    }

    /* ======== OLED 显示(仅状态) ======== */
    OLED_ClearBuffer();
    (void)snprintf(line, sizeof(line), "Task1 %s",
                   state == T1_IDLE    ? "RIGHT->go" :
                   state == T1_STOPPED ? "DONE"      : "RUN");
    OLED_ShowString(0U, 0U, line, 16U, 1U);
    (void)snprintf(line, sizeof(line), "%s",
                   crossLockout > 0U ? "LOCK" : "");
    OLED_ShowString(0U, 16U, line, 16U, 1U);
    (void)snprintf(line, sizeof(line), "A:%lu T:%d",
                   (unsigned long)detectCount, (int)trimBias);
    OLED_ShowString(0U, 32U, line, 16U, 1U);
    OLED_ShowString(0U, 48U, "UART0=params", 16U, 1U);
    OLED_Refresh();
}

/*
 * ================================================================
 *  Task 2 —— 曲线调试入口（自适应算法）
 *
 *  操作：菜单选中 Task 2 → 小车放到半圆端点 → 按F1启动
 *  行为：循迹前进，增益和速度随偏差自适应调节，丢线(全白)停车
 *
 *  自适应原理：
 *    偏差幅度 |e| 反映弯道曲率——直道小、弯道大
 *    filtered = 低通滤波(|e|) 得到平滑的曲率估计
 *    gain  = BASE + filtered × GAIN_SCALE   (自动适应曲率)
 *    speed = BASE - filtered × SPEED_SCALE  (弯道自动降速)
 *    所有参数在100帧内自动收敛，无需手动调。
 * ================================================================
 */
void App_Task2Run(void)
{
    enum { T2_IDLE, T2_TRACKING, T2_STOPPED };

    const uint32_t now = BSP_Delay_GetTick();
    uint8_t  raw;
    char     line[17];

    static uint8_t  state = T2_IDLE;
    static uint8_t  lastActive;
    static uint32_t lastTick;
    static uint32_t startTick;
    static int16_t  sLeft  = 500;
    static int16_t  sRight = 500;
    static float    prevErr;       /* 偏差滤波 */
    static float    curveLevel;    /* 曲率估计（平滑后的|偏差|） */

    uint8_t  activeCount;
    int16_t  errorTenths;

    /* 菜单重入 */
    if (lastActive != 2U) {
        lastActive = 2U;
        state    = T2_IDLE;
        lastTick = 0U;
        sLeft    = 500;
        sRight   = 500;
        prevErr    = 0.0f;
        curveLevel = 0.0f;
        Set_Speed(0, 0);
    }

    if ((uint32_t)(now - lastTick) < 20U) return;
    lastTick = now;

    Grayscale_Read();
    raw = Grayscale_GetRaw();
    (void)LineTrace_CalcActiveLowWeightedError(raw, &errorTenths, &activeCount);

    switch (state) {

    case T2_IDLE: {
        uint8_t btn = (DL_GPIO_readPins(Key_PORT, Key_F1_PIN) == 0U) ? 1U : 0U;
        static uint8_t db;
        if (btn) {
            if (++db >= 3U) {
                db = 0U;
                state = T2_TRACKING;
                Encoder_Init();
                sLeft  = 500;
                sRight = 500;
                prevErr    = 0.0f;
                curveLevel = 0.0f;
                startTick = now;
                uart0_send_string("T2: CURVE START\r\n");
            }
        } else { db = 0U; }
        Set_Speed(0, 0);
        break;
    }

    case T2_TRACKING: {
        /* 丢线(全白) → 停车 */
        if (activeCount == 0U) {
            Set_Speed(0, 0);
            state = T2_STOPPED;
            {
                uint32_t t = (uint32_t)(now - startTick);
                char m[32];
                (void)snprintf(m, sizeof(m),
                               "T2: DONE t=%lu.%lus\r\n",
                               (unsigned long)(t / 1000U),
                               (unsigned long)((t % 1000U) / 100U));
                uart0_send_string(m);
            }
            break;
        }

        /* ---- 自适应算法 ---- */
        {
            int16_t err = errorTenths;
            float   fe, absErr;
            static uint8_t lostCnt;

            /* 死区（微小偏差不动） */
            if (err >= -2 && err <= 2) err = 0;

            /* 偏差低通滤波（α=0.6） */
            fe = (float)err;
            fe = 0.6f * prevErr + 0.4f * fe;
            prevErr = fe;
            err = (int16_t)fe;

            /* 曲率估计：低通滤波 |偏差|（α=0.95，缓慢变化） */
            absErr = (float)(err >= 0 ? err : -err);
            curveLevel = 0.95f * curveLevel + 0.05f * absErr;

            /*
             * 自适应参数（连续变化，无突变）：
             *   gain = 1.0 + curveLevel × 0.4    → 范围约 1.0~5.0
             *   speed = 500 - curveLevel × 25    → 范围约 500~200
             *   rate = 30 + curveLevel × 5       → 范围约 30~80
             */
            #define T2_BASE_GAIN    (10)   /* P增益基数(×0.1) */
            #define T2_GAIN_SCALE   (4)    /* 曲率→增益系数(×0.1) */
            #define T2_BASE_PWM     (500)  /* 基准速度 */
            #define T2_SPEED_SCALE  (25)   /* 曲率→减速系数 */
            #define T2_BASE_RATE    (30)   /* 基准变速 */
            #define T2_RATE_SCALE   (5)    /* 曲率→变速系数 */
            #define T2_MAX_CORR     (250)  /* 最大修正 */

            int16_t gain  = (int16_t)(T2_BASE_GAIN + (int16_t)(curveLevel * (float)T2_GAIN_SCALE));
            int16_t bp    = (int16_t)(T2_BASE_PWM - (int16_t)(curveLevel * (float)T2_SPEED_SCALE));
            int16_t rate  = (int16_t)(T2_BASE_RATE + (int16_t)(curveLevel * (float)T2_RATE_SCALE));

            /* 限幅 */
            if (gain < 10)  gain = 10;
            if (gain > 60)  gain = 60;
            if (bp   < 200) bp   = 200;
            if (bp   > 500) bp   = 500;
            if (rate < 20)  rate = 20;
            if (rate > 100) rate = 100;

            /* 比例修正 */
            int16_t corr = (int16_t)(((int32_t)err * gain) / 10);
            if (corr >  T2_MAX_CORR) corr =  T2_MAX_CORR;
            if (corr < -T2_MAX_CORR) corr = -T2_MAX_CORR;

            /* 目标PWM + 变化率整形 */
            int16_t tl = (int16_t)(bp + corr);
            int16_t tr = (int16_t)(bp - corr);
            int16_t d;
            if (tl < 0) tl = 0; if (tl > 1800) tl = 1800;
            if (tr < 0) tr = 0; if (tr > 1800) tr = 1800;

            d = (int16_t)(tl - sLeft);
            if (d > rate) sLeft += rate; else if (d < -rate) sLeft -= rate; else sLeft = tl;
            d = (int16_t)(tr - sRight);
            if (d > rate) sRight += rate; else if (d < -rate) sRight -= rate; else sRight = tr;

            Set_Speed((int)sLeft, (int)sRight);

            #undef T2_BASE_GAIN
            #undef T2_GAIN_SCALE
            #undef T2_BASE_PWM
            #undef T2_SPEED_SCALE
            #undef T2_BASE_RATE
            #undef T2_RATE_SCALE
            #undef T2_MAX_CORR
        }
        break;
    }

    case T2_STOPPED:
        Set_Speed(0, 0);
        break;

    default:
        break;
    }

    /* OLED */
    OLED_ClearBuffer();
    (void)snprintf(line, sizeof(line), "%u%u%u%u%u%u%u%u",
                   (unsigned)((raw>>7)&1),(unsigned)((raw>>6)&1),
                   (unsigned)((raw>>5)&1),(unsigned)((raw>>4)&1),
                   (unsigned)((raw>>3)&1),(unsigned)((raw>>2)&1),
                   (unsigned)((raw>>1)&1),(unsigned)(raw&1));
    OLED_ShowString(0U, 0U, line, 16U, 1U);
    (void)snprintf(line, sizeof(line), "T2 %s",
                   state==T2_IDLE?"F1->go":state==T2_STOPPED?"DONE":"RUN");
    OLED_ShowString(0U, 16U, line, 16U, 1U);
    (void)snprintf(line, sizeof(line), "Cv:%.0f G:%.1f",
                   (double)curveLevel, (double)((float)(10+(int16_t)(curveLevel*4))/10.0f));
    OLED_ShowString(0U, 32U, line, 16U, 1U);
    (void)snprintf(line, sizeof(line), "P:%d %d/%d",
                   (int)(500-(int16_t)(curveLevel*25)), (int)sLeft, (int)sRight);
    OLED_ShowString(0U, 48U, line, 16U, 1U);
    OLED_Refresh();
}

/*
 * ================================================================
 *  Task 3 —— 题4：A→B 直线循迹 + 经过B
 *
 *  指标：AB时间 ≤8s
 *  B点无标记，用 IMU gyro_z 检测入弯事件作为 B 点通过标志。
 * ================================================================
 */
void App_Task3Run(void)
{
    /* TODO: Task1 直道参数 + B点检测 */
}

/*
 * ================================================================
 *  Task 4 —— 题5：单圈循迹 + 经过A（不停车）
 *
 *  指标：≤30s，钢球稳定在摆杆中心 O（±1cm）
 *  循迹同 Task1 但速度更慢，检测到A后只记录时间不停车。
 * ================================================================
 */
void App_Task4Run(void)
{
    /* TODO: Task1 参数降速版，检测A后不停车 */
}

/*
 * ================================================================
 *  Task 5 —— 题6：单圈循迹 + 经过A（不停车）+ 球任意位置
 *
 *  指标：≤30s，钢球稳定在摆杆任意指定位置（±1cm）
 *  循迹同 Task4，球控制区别于 Task4。
 * ================================================================
 */
void App_Task5Run(void)
{
    /* TODO: 循迹同 Task4，球控制由队友补充 */
}

/*
 * ================================================================
 *  Task 分发
 * ================================================================
 */
void App_TasksRun(void)
{
    switch (g_active_task)
    {
    case 1U: App_Task1Run(); break;
    case 2U: App_Task2Run(); break;
    case 3U: App_Task3Run(); break;
    case 4U: App_Task4Run(); break;
    case 5U: App_Task5Run(); break;
    default: break;
    }
}

/**
 * @brief 运行电池电压检测任务。
 * @param 无。
 * @note 1Hz 读取 PA27/BAT_ADC，并把电压和低电量状态发布给一级菜单第四行。
 * @retval 无。
 */
static void App_BatteryRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    uint16_t batteryMv;
    bool batteryLow;

    if ((uint32_t)(now - lastBatteryTick) < APP_BATTERY_SAMPLE_PERIOD_MS)
    {
        return;
    }
    lastBatteryTick = now;

    batteryMv = BSP_ADC_BatteryReadMv();
    batteryLow = (batteryMv < APP_BATTERY_LOW_MV);
    App_MenuSetBatteryData(batteryMv, batteryLow);
}

/*
 * (旧测试函数已删除，循迹逻辑移至 App_Task1Run)
 */
static void App_GrayscaleDisplayRun_removed(void)
{
    enum { T_IDLE, T_TRACKING, T_BRAKE, T_STOPPED };

    const uint32_t now = BSP_Delay_GetTick();
    uint8_t raw;
    char line[17];

    static uint8_t  state = T_IDLE;
    static uint32_t lastTick;
    static uint16_t crossLockout;
    static uint8_t  crossConfirm;
    static uint32_t detectCount;
    static int16_t  sLeft, sRight;  /* 整形后的 PWM */
    static uint32_t startTick;
    static float    prevErr;    /* 滤波状态 */
    static int16_t  trimBias;   /* 自校准偏置 */
    static int32_t  calibSum;   /* 校准累积 */
    static uint16_t calibCnt;   /* 校准采样数 */
    static uint16_t calFrame;   /* 帧计数 */

    uint8_t  activeCount;
    int16_t  errorTenths;
    uint8_t  crossNow;

    if ((uint32_t)(now - lastTick) < 20U) return;
    lastTick = now;

    /* ---- 读灰度 ---- */
    Grayscale_Read();
    raw = Grayscale_GetRaw();
    (void)LineTrace_CalcActiveLowWeightedError(raw, &errorTenths, &activeCount);

    /* ---- 横切线实时判定：任意连续3路全黑 ---- */
    {
        uint8_t b0 = (((raw >> 0) & 1U) == 0U) ? 1U : 0U;
        uint8_t b1 = (((raw >> 1) & 1U) == 0U) ? 1U : 0U;
        uint8_t b2 = (((raw >> 2) & 1U) == 0U) ? 1U : 0U;
        uint8_t b3 = (((raw >> 3) & 1U) == 0U) ? 1U : 0U;
        uint8_t b4 = (((raw >> 4) & 1U) == 0U) ? 1U : 0U;
        uint8_t b5 = (((raw >> 5) & 1U) == 0U) ? 1U : 0U;
        uint8_t b6 = (((raw >> 6) & 1U) == 0U) ? 1U : 0U;
        uint8_t b7 = (((raw >> 7) & 1U) == 0U) ? 1U : 0U;
        crossNow = ((b0&&b1&&b2)||(b1&&b2&&b3)||(b2&&b3&&b4)
                 ||(b3&&b4&&b5)||(b4&&b5&&b6)||(b5&&b6&&b7)) ? 1U : 0U;
    }

    /* ---- 状态机 ---- */
    switch (state) {

    case T_IDLE:
        Set_Speed(0, 0);
        /* 等右键按下 */
        {
            Key5D_Event ev = KEY5D_EVENT_NONE;
            if (App_InputPoll(now, &ev) && ev == KEY5D_EVENT_PRESSED
                && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
                state = T_TRACKING;
                Encoder_Init();
                LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
                detectCount = 0U;
                sLeft = sRight = 600;
                prevErr = 0.0f;
                calibSum = 0;
                calibCnt = 0;
                calFrame = 0;
                trimBias = 0;
                startTick = now;
                uart0_send_string("TRACK START\r\n");
            }
        }
        break;

    case T_TRACKING:
        /* 横切线检测 + 计数 */
        if (LineTrace_DetectCrossLine(raw, activeCount, &crossLockout, &crossConfirm)
            == CROSS_LINE_DETECTED) {
            detectCount++;
            state = T_BRAKE;
            uart0_send_string("STOP LINE!\r\n");
            break;
        }

        /* 循迹修正：P 控制 + 固定偏置补偿机械不对称 */
        {
            int16_t err = errorTenths;
            int16_t corr;
            static uint8_t lostCnt;
            float fe;
            #define CALIB_FRAMES 200           /* 前200帧(~4s)校准 */
            #define CALIB_SKIP    20           /* 跳过前20帧(车未稳) */

            /* 丢线处理 */
            if (activeCount == 0U) {
                lostCnt++;
                err = (prevErr > 0) ? (int16_t)(prevErr * 1.5f) : (int16_t)(prevErr * 1.5f);
                if (lostCnt > 60U) { Set_Speed(0, 0); state = T_STOPPED; break; }
            } else {
                lostCnt = 0U;
                if (err >= -3 && err <= 3) err = 0;
                fe = (float)err;
                fe = 0.7f * prevErr + 0.3f * fe;
                prevErr = fe;
                err = (int16_t)fe;
            }

            /* 校准：跳过起步不稳，|err|<5 时采样，×1.5 补偿 */
            calFrame++;
            if (calibCnt < CALIB_FRAMES && calFrame > CALIB_SKIP) {
                if (err >= -5 && err <= 5) {
                    calibSum += err;
                    calibCnt++;
                }
            }
            if (calibCnt > 0) {
                trimBias = (int16_t)((-calibSum * 3) / ((int32_t)calibCnt * 2));
            }

            /* P 修正 + 自校准偏置 */
            corr = (int16_t)(((int32_t)err * 12) / 10) + trimBias;
            if (corr > 150) corr = 150;
            if (corr < -150) corr = -150;

            /* 目标 PWM */
            int16_t tl = (int16_t)(600 + corr);
            int16_t tr = (int16_t)(600 - corr);
            int16_t d;
            if (tl < 0) tl = 0; if (tl > 1800) tl = 1800;
            if (tr < 0) tr = 0; if (tr > 1800) tr = 1800;

            /* 变化率限制（更平缓） */
            int16_t rate = (sLeft < 200) ? 80 : 30;
            d = (int16_t)(tl - sLeft);
            if (d > rate) sLeft += rate; else if (d < -rate) sLeft -= rate; else sLeft = tl;
            d = (int16_t)(tr - sRight);
            if (d > rate) sRight += rate; else if (d < -rate) sRight -= rate; else sRight = tr;

            Set_Speed((int)sLeft, (int)sRight);
        }
        break;

    case T_BRAKE:
        /* 平缓减速：直接停转，靠惯性滑行 */
        Set_Speed(0, 0);
        state = T_STOPPED;
        uart0_send_string("STOPPED\r\n");
        break;

    case T_STOPPED:
        /* 保持停车 */
        /* 按右键重新开始 */
        {
            Key5D_Event ev = KEY5D_EVENT_NONE;
            if (App_InputPoll(now, &ev) && ev == KEY5D_EVENT_PRESSED
                && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
                state = T_TRACKING;
                LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
                detectCount = 0U;
                sLeft = sRight = 600;
                prevErr = 0.0f;
                calibSum = 0;
                calibCnt = 0;
                calFrame = 0;
                trimBias = 0;
                startTick = now;
                uart0_send_string("TRACK START\r\n");
            }
        }
        break;
    }

    /* ---- OLED ---- */
    OLED_ClearBuffer();

    (void)snprintf(line, sizeof(line), "%u%u%u%u%u%u%u%u",
                   (unsigned)((raw >> 7) & 1), (unsigned)((raw >> 6) & 1),
                   (unsigned)((raw >> 5) & 1), (unsigned)((raw >> 4) & 1),
                   (unsigned)((raw >> 3) & 1), (unsigned)((raw >> 2) & 1),
                   (unsigned)((raw >> 1) & 1), (unsigned)(raw & 1));
    OLED_ShowString(0U, 0U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "STOP:%s %s",
                   crossNow ? "YES" : "no ",
                   (state == T_IDLE) ? "RIGHT->go" :
                   (state == T_STOPPED) ? "DONE" : "");
    OLED_ShowString(0U, 16U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "C:%lu T:%d sL=%d",
                   (unsigned long)detectCount, (int)trimBias, (int)sLeft);
    OLED_ShowString(0U, 32U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "0x%02X t=%lus",
                   (unsigned)raw,
                   (unsigned long)((now - startTick) / 1000UL));
    OLED_ShowString(0U, 48U, line, 16U, 1U);

    OLED_Refresh();
}

/**
 * @brief 初始化 App 层公共资源。
 * @param 无。
 * @note 初始化输入、菜单、自检、UART0 和 OLED；车辆/IMU/ToF 在各自任务首次运行时初始化。
 * @retval 无。
 */
void App_Init(void)
{
    /* 先初始化不依赖外设输出的状态机，避免后续菜单显示读取到未定义状态。 */
    App_InputInit();
    App_MenuInitData();
    lastBatteryTick = BSP_Delay_GetTick() - APP_BATTERY_SAMPLE_PERIOD_MS;

    /* 软件自检只检查纯逻辑，硬件在线状态由各任务首次运行时再诊断。 */
    g_key5d_self_test_failures = Key5D_RunSelfTest();
    g_key5d_self_test_complete = 1U;
    g_menu_self_test_failures = Menu_RunSelfTest();
    g_menu_self_test_complete = 1U;
    g_dl1a_self_test_failures = DL1A_RunSelfTest();
    g_dl1a_self_test_complete = 1U;
    g_line_trace_self_test_failures = LineTrace_RunSelfTest();
    g_line_trace_self_test_complete = 1U;
    g_bt_command_self_test_failures = BluetoothCommand_RunSelfTest();
    g_bt_command_self_test_complete = 1U;
    App_ElectromagnetInit();

    uart0_init();
    uart0_send_string("BOOT uart0 OK\r\n");
    uart0_send_string("BOOT oled init\r\n");
    OLED_Init();
    uart0_send_string("BOOT oled OK\r\n");
    App_LogSelfTests();
}

/**
 * @brief App 层主循环调度入口。
 * @param 无。
 * @note 在 main 的 while(1) 中重复调用；通过注释任务调用行控制模板功能开关。
 * @retval 无。
 */
void App_Run(void)
{
    static uint8_t runLoopLogged;

    if (runLoopLogged == 0U) {
        runLoopLogged = 1U;
        uart0_send_string("RUN loop OK\r\n");
    }

    App_BatteryRun();
    App_ElectromagnetRun();
    App_MenuRun();

    /* 无任务时停车；有任务时由 Task 接管 */
    if (g_active_task == 0U) {
        Set_Speed(0, 0);
    }

    App_TasksRun();
}
