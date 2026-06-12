# FFRE

FFRE 是一个基于 Geant4 的裂变碎片空间核推进概念研究代码库，用于支撑《基于裂变碎片的空间核推进概念设计及可行性研究》中的模型搭建、输运统计和早期方案筛查。

项目关注的核心问题是：微米级 Am-242m 燃料壳层中产生的直接裂变碎片，能否在给定几何、源项和磁场条件下向外逃逸，并进入预设出口结构；在此基础上，进一步探索 H₂ 工质耦合和粉尘等离子体路线的一阶可行性边界。

本仓库不是完整反应堆设计程序，也不是可直接输出工程推力、完整比冲、喷管性能或飞行器总体性能的系统级工具。代码输出应理解为当前模型假设下的 Geant4 输运统计、后处理估算或 no-go 筛查证据。

重要说明：本项目代码、Shell 脚本和运行示例默认面向 Linux 环境。若在 Windows 上使用，建议通过 WSL、Linux 虚拟机或真实 Linux 工作站运行；README 中的命令也按 Linux/bash 语法书写。

## 项目结构

```text
.
├── B1/       Am-242m 裂变碎片输运主模型
├── B2/       H₂ 工质/后燃器耦合探索模型
└── plasma/   粉尘等离子体方案 no-go 筛查模型
```

### B1: 主输运模型

[`B1`](B1/README) 是论文主计算模型。它以 Geant4 B1 示例为框架，改写为 Am-242m 涂层燃料颗粒中的中子诱发裂变、直接裂变碎片逃逸、磁场准直输运以及 Shape3/Shape4 两侧弯曲出口命中统计。

主要能力包括：

- 建模石墨内核与微米级 Am-242m 金属燃料壳层；
- 使用 GPS 宏文件控制入射粒子、能量和源项；
- 识别 `nFission` 直接产生的核碎片；
- 统计 FuelShell 到 Shape1 的首次外向逃逸；
- 比较 `off`、`uniform`、`collimation` 三种磁场模式；
- 输出 `Fission Transport Summary`，用于输运统计和后续估算。

### B2: H₂ 工质耦合扩展

[`B2`](B2/README) 继承 B1 的几何、燃料颗粒、磁场和出口统计，并增加可选的 Shape1 氢气填充，用于探索裂变碎片与轻质工质或后燃器概念之间的耦合接口。

新增能力包括：

- 通过 `FFRE_FILL_SHAPE1_H2=true` 将 Shape1 填充为 H₂；
- 记录裂变碎片进入、离开 H₂ 区域的统计；
- 统计 H₂ 区域中的能量沉积、轴向动量变化和出口方向性；
- 使用 `scripts/compute_h2_startup_performance.py` 从运行日志进行简化后处理估算。

B2 的输出不能直接等同于完整气体温升、喷管膨胀或推力增益。

### plasma: 粉尘等离子体 no-go 筛查

[`plasma`](plasma/README) 用于前期粉尘等离子体裂变碎片推进路线的一阶可行性筛查。它建立低密度 DustCloud、轴向磁场约束、Ba-140/Kr-95 裂变碎片代理源项和热归一化评估链路。

主要用途包括：

- 判断低密度粉尘云是否利于裂变碎片输运与逃逸；
- 比较活动云质量与临界质量代理、热设计燃料质量的差距；
- 评估给定热功率与时间窗下的能量沉积和温升；
- 生成 PASS/FAIL 证据表和 no-go dashboard。

该模块用于“参数一致性与一阶热闭合筛查”，不代表最终推进器方案。

## 依赖环境

每个子目录都是相对独立的 Geant4/CMake 工程。典型环境包括：

- Linux 运行环境；
- CMake 3.16 到 3.21；
- C++ 编译器；
- Geant4 及对应数据集；
- Python 3，用于部分后处理脚本；
- Bash 环境，用于运行 `scripts/*.sh`。

如果启用 Geant4 可视化，CMake 会默认查找 `ui_all` 和 `vis_all`。如只需要批处理构建，可使用：

```bash
cmake -S . -B build -DWITH_GEANT4_UIVIS=OFF
```

## 快速开始

以 B1 为例：

```bash
cd B1
cmake -S . -B build
cmake --build build
```

准备一个 Geant4 宏文件，例如 `run.mac`：

```text
/run/initialize
/gps/particle neutron
/gps/pos/type Point
/gps/pos/centre 0 0 0 m
/gps/ang/type iso
/gps/ene/type Mono
/gps/ene/mono 0.0253 eV
/run/beamOn 10000
```

运行：

```bash
./build/exampleB1 path/to/run.mac
```

B2 和 plasma 的构建方式相同：

```bash
cd B2
cmake -S . -B build
cmake --build build
```

```bash
cd plasma
cmake -S . -B build
cmake --build build
```

当前源码树主要保留源码、数据补丁和脚本，不保留历史构建目录、历史运行宏和历史结果表。论文扫描或批处理运行所需的宏文件通常需要由运行者自行准备。

## 常用环境变量

B1/B2 共享的磁场和输出控制：

```bash
FFRE_FIELD_MODE=off|uniform|collimation
FFRE_ENABLE_COLLIMATION_FIELD=false
FFRE_B0_T=1.5
FFRE_GRADB_T_PER_M=-0.04
FFRE_SPLIT_ANGLE_DEG=10
FFRE_PRINT_EVENT_SUMMARY=0
```

B2 的 H₂ 控制：

```bash
FFRE_FILL_SHAPE1_H2=true
FFRE_H2_TEMPERATURE_K=300
FFRE_H2_PRESSURE_ATM=1
FFRE_H2_DENSITY_KG_M3=<density>
```

更多参数说明见各子目录 README。

## 数据与脚本

- `data/G4NDL4.6_patched/` 和 `data/G4NDL4.6_overlay/` 保存 Am-242/Am-242m 相关的 NeutronHP 数据补丁或覆盖文件；
- `scripts/sweep_magnetic_modes.sh` 用于生成并运行磁场模式扫描；
- `scripts/compare_magnetic_modes.sh` 用于对比磁场模式运行日志；
- `scripts/generate_escape_speed_distribution_table.sh` 用于整理逃逸速度分布；
- `B2/scripts/compute_h2_startup_performance.py` 用于 H₂ 启动阶段简化后处理；
- `plasma/tools/no_go_evidence_report.py` 用于生成 no-go 筛查证据表和 dashboard。

部分脚本依赖用户提供宏文件或本地 Geant4 数据路径。运行前请先阅读对应子目录 README 和脚本参数。

## 如何解释结果

结果解释时建议区分三层：

- `B1`：当前几何、源项、物理列表和磁场参数下的裂变碎片输运统计；
- `B2`：在 B1 框架上加入 H₂ 区域后的能量沉积、动量变化和简化后处理估算；
- `plasma`：粉尘等离子体路线在当前参数口径下的 no-go 筛查证据。

这些结果不应直接外推为完整反应堆临界性能、工程磁体设计、喷管膨胀、飞行器总体质量闭合或最终推进系统性能。

## 许可证

当前仓库尚未声明开源许可证。
