# Gowin SSPI SRAM 配置工具（V3S/Linux）

此工具按 UG290-2.9.1 第 7.4 节通过 Gowin SSPI 向 FPGA SRAM 下载二进制配置流。它使用 Linux `spidev`，无需额外库。

## 前置条件

- FPGA 的 `MODE[2:0]` 配置为 SSPI 模式 `001`，且 SSPI 接口已使能；FPGA 已上电，`RECONFIG_N` 状态允许进入 SSPI 配置。
- V3S 已启用 SPI 控制器及 `spidev`，两个设备节点须属于同一个 SPI 控制器并共享 SCLK、MOSI、MISO。
- `fpga-spidev` 的 CS 连接 FPGA `SSPI_CS_N`。另一个 CS-high 时钟设备的 CS 必须未连接 FPGA 的 `SSPI_CS_N`，也不能选中其他会受影响的设备；此节点只用于在 FPGA CS 保持高电平时产生 SCLK。
- 接好公共地，并确认 V3S 与 FPGA 的 IO 电压兼容。`SO` 接 MISO、`SI` 接 MOSI，`SCLK` 接 SCLK；`CLKHOLD_N` 保持高电平。
- 使用 Gowin 工具生成的 SSPI 兼容二进制配置流。此程序发送文件原始字节，不解析 `.fs` 文本、`.binx`、寄存器初始化文件或其他容器格式。

SPI 使用 mode 0、MSB first、8 bit/word 和 50 kHz。该速率低于 UG290 的 SCLK 上限，并便于在板级连线下稳定配置。

## 编译及运行

```sh
cd fpga_config
make
sudo ./gowin_sspi /dev/spidev0.1 /path/to/design.bin /dev/spidev0.0
```

第一个设备节点的 CS 必须连 FPGA SSPI 片选。示例假设 `/dev/spidev0.0` 与 `/dev/spidev0.1` 属于同一 SPI 总线，且 CS0 未连接到 FPGA；请按 V3S 设备树和实际接线替换节点。关闭会占用该 SPI 总线的其他程序。

## 传输限制

UG290 要求 `Write Data (0x3B)` 期间 CS 一直为低，因此命令字节和完整 bitstream 会作为单次 SPI message 发送，不能拆成多个事务。Linux `spidev` 默认缓冲区常为 4096 字节；若 bitstream 加上 `0x3B` 超过此值，须在目标系统将 `spidev.bufsiz` 提高到至少 `bitstream 字节数 + 1`，再运行程序。例如可在系统启动参数中设置 `spidev.bufsiz=65536`。缓冲区不够时程序会报告 SPI message 失败，不会改用破坏 SSPI 时序的分块传输。

## 配置序列

程序在 FPGA 主 CS 保持高时通过另一个片选输出 8 个时钟，再拉低 FPGA CS 发送每个命令，符合 UG290 对命令前后 CS 高电平时至少两个时钟的要求。配置过程为：

1. 读取 `Read Status Code (0x41)`；要求 `READY`（bit 15）和 `POR`（bit 16）均为 1，否则停止配置；若 `Done Final`（bit 13）已置位，则发送 `Erase SRAM (0x0500)` 并等待至少 10 ms。
2. 发送 `Init Address (0x1200)`、`Write Enable (0x1500)`。
3. 在一个 CS 低电平事务中发送 `0x3B` 和完整配置流。
4. 发送 `Write Disable (0x3A00)`，轮询 `READY`（bit 15）、`Done Final`（bit 13）和错误标志（bits 0–3），然后读取并显示 ID/User Code。只有 `READY=1`、`Done Final=1` 且错误标志全为 0 才判为成功。

板级下载成功仍依赖 SSPI 引脚复用、FPGA 模式管脚、CS 接线和所选配置流正确；运行日志会报告 SPI 访问错误或 FPGA 状态校验失败。
