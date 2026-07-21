"""
PI 速度环自动调参脚本 —— 阶跃响应法
======================================
原理：
  1. 纯 P 控制下，给定目标速度阶跃，采集响应曲线
  2. 测量上升时间、超调量、稳态误差
  3. 估算电机一阶模型参数: 增益 K_plant、时间常数 tau
  4. 用 IMC-PI 规则推荐 Kp/Ki，消除稳态误差且无超调

用法（先关闭 VOFA+ 释放串口！）：
  pip install pyserial
  python pid_autotuner.py --port COM15 --target 20

参数：
  --port    串口号（UART0 对应的 USB-TTL COM 口）
  --baud    波特率，默认 115200
  --target  目标速度（cm/s），默认 20
  --kp_try  纯P测试的 Kp 列表，空格分隔，默认 "10 20 30"
"""

import argparse
import struct
import time
import threading
import sys
import serial

# JustFloat 帧尾
TAIL = bytes([0x00, 0x00, 0x80, 0x7F])
N_CH = 6
# CH0=Motor1_Speed CH1=Motor2_Speed CH2=TargetA CH3=TargetB CH4=OutA CH5=OutB

def parse_frames(buf):
    frames = []
    while True:
        idx = buf.find(TAIL)
        if idx == -1:
            break
        start = idx - N_CH * 4
        if start < 0:
            buf = buf[idx + 4:]
            continue
        raw = buf[start:idx]
        if len(raw) == N_CH * 4:
            frames.append(struct.unpack('<6f', raw))
        buf = buf[idx + 4:]
    return frames, buf

class Logger:
    def __init__(self, ser):
        self.ser = ser
        self.data = []   # (timestamp, ch0..ch5)
        self._buf = bytearray()
        self._stop = False
        self._t = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._t.start()

    def stop(self):
        self._stop = True
        self._t.join(timeout=2)

    def clear(self):
        self.data.clear()

    def _run(self):
        while not self._stop:
            raw = self.ser.read(self.ser.in_waiting or 1)
            if raw:
                self._buf.extend(raw)
                frames, self._buf = parse_frames(self._buf)
                t = time.time()
                for f in frames:
                    self.data.append((t, f[0], f[1], f[4]))

def send_cmd(ser, cmd):
    ser.write((cmd + '\r\n').encode())
    print(f"  >> {cmd}")
    time.sleep(0.08)

def step_analysis(data, target, t_step):
    after = [(t, s) for t, s, _, __ in data if t >= t_step]
    if len(after) < 5:
        return None
    speeds = [s for _, s in after]
    times  = [t for t, _ in after]

    n_last = max(3, len(speeds) * 3 // 10)
    steady = sum(speeds[-n_last:]) / n_last
    steady_err = target - steady
    steady_err_pct = abs(steady_err) / target * 100 if target != 0 else 0

    t10 = t90 = None
    for t, s in after:
        if t10 is None and s >= target * 0.10:
            t10 = t
        if t90 is None and s >= target * 0.90:
            t90 = t

    rise_ms = (t90 - t10) * 1000 if (t10 and t90) else None
    tau = (t90 - t_step) / 2.3 if t90 else None

    peak = max(speeds)
    overshoot_pct = (peak - target) / target * 100 if peak > target else 0.0

    return {
        'steady':         steady,
        'steady_err':     steady_err,
        'steady_err_pct': steady_err_pct,
        'rise_ms':        rise_ms,
        'overshoot_pct':  overshoot_pct,
        'peak':           peak,
        'tau':            tau,
    }

def imc_pi(kp_used, target, res):
    steady = res['steady']
    tau    = res['tau']
    if steady is None or tau is None or steady <= 0:
        return None, None
    err = abs(target - steady)
    if err < 0.05:
        err = 0.05
    K_plant = steady / (kp_used * err)
    lam = max(tau * 0.5, 0.02)
    Kp_rec = tau / (K_plant * lam)
    Ki_rec = Kp_rec / tau
    return Kp_rec, Ki_rec

def main():
    parser = argparse.ArgumentParser(description='PI 速度环阶跃响应自动整定')
    parser.add_argument('--port',   default='COM15')
    parser.add_argument('--baud',   type=int, default=115200)
    parser.add_argument('--target', type=float, default=20.0)
    parser.add_argument('--kp_try', default='10 20 30')
    args = parser.parse_args()
    kp_list = [float(x) for x in args.kp_try.split()]
    target  = args.target

    print(f"\n{'='*58}")
    print(f"  PI 阶跃响应整定  |  端口:{args.port}  |  目标:{target} cm/s")
    print(f"{'='*58}")
    print(f"\n  !! 请确认已关闭 VOFA+ 以释放 {args.port} !!\n")

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"  错误: 无法打开串口: {e}")
        if 'PermissionError' in str(e) or '拒绝访问' in str(e):
            print(f"  -> 端口被占用，请关闭 VOFA+ 后重试")
        sys.exit(1)

    time.sleep(0.3)
    ser.reset_input_buffer()
    logger = Logger(ser)
    logger.start()

    print("[初始化] 设置 Ki=0 Kd=0，电机停止...")
    send_cmd(ser, 'KI:0.0')
    send_cmd(ser, 'KD:0.0')
    send_cmd(ser, 'S:0')
    time.sleep(1.5)

    best_result = None
    best_kp     = None
    all_results = {}

    print(f"\n[阶跃测试] 依次测试 Kp = {kp_list}")
    print(f"  步骤: S:0 -> 稳定1s -> S:{target} -> 采集3s -> 停止\n")

    for kp in kp_list:
        print(f"  ─── Kp = {kp} ───")
        send_cmd(ser, f'KP:{kp:.1f}')
        send_cmd(ser, 'S:0')
        time.sleep(1.2)
        logger.clear()

        t_step = time.time()
        send_cmd(ser, f'S:{target}')
        time.sleep(3.2)
        send_cmd(ser, 'S:0')
        time.sleep(0.6)

        res = step_analysis(logger.data, target, t_step)
        if res is None:
            print(f"    ✗ 未收到数据（当前共 {len(logger.data)} 帧）")
            print(f"      检查: 1)MCU是否在运行 2)串口接线 3)MCU VOFA发送间隔")
            continue

        rise_str = f"{res['rise_ms']:.0f}ms" if res['rise_ms'] else "未到达90%"
        print(f"    稳态速度:  {res['steady']:.2f} cm/s  (误差 {res['steady_err_pct']:.1f}%)")
        print(f"    上升时间:  {rise_str}")
        print(f"    超调量:    {res['overshoot_pct']:.1f}%")
        print(f"    峰值速度:  {res['peak']:.2f} cm/s")

        all_results[kp] = res
        if res['overshoot_pct'] < 20 and res['steady'] > target * 0.4:
            if best_result is None or (res['rise_ms'] or 9999) < (best_result['rise_ms'] or 9999):
                best_result = res
                best_kp     = kp

    send_cmd(ser, 'S:0')
    time.sleep(0.3)
    logger.stop()
    ser.close()

    print(f"\n{'='*58}")
    if best_result is None:
        print("  未找到合适 Kp，请尝试更小的值：")
        print(f"  python pid_autotuner.py --port {args.port} --target {target} --kp_try \"3 5 8\"")
    else:
        Kp_rec, Ki_rec = imc_pi(best_kp, target, best_result)
        rise_str = f"{best_result['rise_ms']:.0f}ms" if best_result['rise_ms'] else "N/A"
        print(f"\n  最优基准: Kp={best_kp}  上升时间={rise_str}  超调={best_result['overshoot_pct']:.1f}%")
        print(f"\n  ┌──────────────────────────────────────────────┐")
        kp_show = Kp_rec if Kp_rec else best_kp
        ki_show = Ki_rec if Ki_rec else 0.15
        print(f"  │  Kp = {kp_show:<8.2f}  （IMC整定，无超调快响应）  │")
        print(f"  │  Ki = {ki_show:<8.4f}  （消除稳态误差）           │")
        print(f"  │  Kd = 0.0        （速度环不需要D项）         │")
        print(f"  └──────────────────────────────────────────────┘")
        print(f"\n  重新连接 VOFA+ 后发送以下命令验证：")
        print(f"    KP:{kp_show:.2f}")
        print(f"    KI:{ki_show:.4f}")
        print(f"    KD:0.0")
        print(f"    S:{target}")
        print(f"\n  验证要点：")
        print(f"    - CH0/CH1(实际速度) 应快速稳定贴合 CH2/CH3(目标)")
        print(f"    - 用手阻碍后松手，速度应在 200ms 内恢复")
        print(f"    - 如仍有稳态误差，每次增加 Ki +0.05 直至消除")
        print(f"    - 如出现振荡，减小 Ki 一半\n")

if __name__ == '__main__':
    main()

