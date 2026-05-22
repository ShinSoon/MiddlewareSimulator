# C++ Middleware Simulator

## Description / Purpose

A command-line C++ application that simulates core functionality of TV / Set-Top-Box middleware. It demonstrates C++ skills (OOD, STL, bit-level binary parsing, file I/O, build systems, unit testing) and broadcast-domain knowledge (Service Information processing, EPG handling, MPEG transport streams).

The simulator does two things:

1. **Parses** a structured text format into an in-memory model of channels and their programs (an EPG).
2. **Encodes that model into a real MPEG-TS transport stream** (PAT, PMT, SDT, EIT) written to a `.ts` file, and **decodes it back** — a full round-trip that mirrors how real middleware processes DVB SI/PSI tables.

## Features Implemented

### Domain Model
* `struct ChannelInfo` — service id, name, provider, service type, PMT/video/audio PIDs, logical channel number.
* `struct ProgramInfo` — event id, name, description, start/end time (epoch ms), genre, parental age rating.

### Text Parser (`Parser`)
* Parses a custom pipe-delimited (`|`) format with record-type prefixes (`CH`, `PG`).
    * `CH|ID|Name|[Provider]|[ServiceType]|[LCN]`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description|[Genre]|[ParentalAge]`
    * Trailing fields are optional and backward-compatible.
* Stores channels in a `std::vector`, programs in a `std::map<int, std::vector<ProgramInfo>>` keyed by channel id.
* Tracks known channel ids in a `std::set<int>` and rejects "orphan" program records whose channel was never declared.
* Error handling for invalid numbers (`std::stoi`), malformed/incomplete records, and end-before-start times.

### Time Handling
* Time strings (`YYYY-MM-DD HH:MM:SS`) are parsed as **UTC** and stored as `long long` epoch milliseconds for cheap, correct comparison.
* UTC conversion uses `_mkgmtime` (Windows) / `timegm` (POSIX), not `mktime`, which would apply the local timezone offset.

### EPG Queries
* `getChannels()`, `getProgramsForChannel(id)`, `getProgramsOnNow(id, nowMs)`, `getProgramsForTimeRange(id, startMs, endMs)` (interval-overlap logic).

### MPEG-TS Pipeline
A from-the-byte-up encoder/decoder for DVB SI/PSI tables:

* **Bit-level I/O** (`BitWriter` / `BitReader`) — MSB-first packing for non-byte-aligned fields (13-bit PID, 12-bit section_length, etc.).
* **TS packets** (`TsPacket*`) — 188-byte packets, `0x47` sync byte, 13-bit PID, continuity counter; the reader resynchronizes on the sync byte.
* **PSI sections** (`PsiSection`, `Crc32`) — generic section envelope protected by **CRC-32/MPEG-2**; corrupted sections are rejected.
* **PAT** — maps `program_number` → PMT PID.
* **PMT** — per-service PCR PID and video/audio elementary streams (`stream_type`).
* **SDT** — `service_descriptor` (0x48): service name, provider, type.
* **EIT** — the EPG: events with **MJD + BCD** start/duration plus `short_event_descriptor` (0x4D), `content_descriptor` (0x54, genre nibble), and `parental_rating_descriptor` (0x55).
* **Mux / Demux** (`TsMuxer` / `TsDemuxer`) — encode a channel list + schedules to a transport stream and decode it back. The running simulator writes a real `output.ts` file and reads it back.

### Unit Testing
* 43 tests using **GoogleTest**, covering the parser, time handling, EPG queries, and every layer of the TS pipeline.
* Includes **known-byte-vector** tests validated against the specs, not just self-consistency: CRC-32/MPEG-2 check value `0x0376E6E7`, MJD `0xC079` (DVB Annex C example), and the PAT byte layout `00 01 E1 00`.

### C++ Best Practices
* Header guards, declaration/implementation separation for the parser, STL containers (`vector`, `map`, `set`, `string`), exception handling, and class-based design.

## Architecture / Module Map

```
MiddlewareSimulator/
  Domain model :  ChannelInfo.h, ProgramInfo.h
  Text parser  :  Parser.{h,cpp}, simulated_data.txt
  Bit I/O      :  BitWriter.h, BitReader.h
  TS packet    :  TsPacket.h, TsPacketWriter.h, TsPacketReader.h
  PSI framing  :  Crc32.h, PsiSection.h
  SI tables    :  PsiTables.h (PAT/PMT), SdtTable.h, EitTable.h, MjdTime.h
  Mux / Demux  :  TsMuxer.h, TsDemuxer.h
  Application  :  main.cpp
MiddlewareSimulatorTests/
  Tests.cpp    :  GoogleTest suite (43 tests)
```

The same domain model (`ChannelInfo` / `ProgramInfo`) is the single source of truth for both the text parser and the TS pipeline.

## Technologies Used

* **Language:** C++ (C++17)
* **Core Libraries:** C++ Standard Library (`<vector>`, `<map>`, `<set>`, `<string>`, `<array>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, `<chrono>`/`<ctime>` for time, `<cstdint>` for fixed-width binary types).
* **Testing:** GoogleTest (NuGet: `Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn`).
* **Build System:** Visual Studio 2022 / MSBuild (`.sln`, `.vcxproj`).
* **Compiler:** MSVC (v143 toolset, ships with Visual Studio 2022).

## Setup / Prerequisites

* Visual Studio 2022 with the **"Desktop development with C++"** workload.
* A C++17 compliant compiler (MSVC 19.30+, comes with Visual Studio 2022).
* Git (for cloning).

## How to Build

This project is built with **Visual Studio 2022** (MSBuild). Any edition (Community / Professional / Enterprise) works.

1. Clone the repository: `git clone <repository-url>`
2. Open `MiddlewareSimulator/MiddlewareSimulator.sln` in Visual Studio 2022.
3. Select build configuration (`Debug` or `Release`) and platform (`x64`).
4. Build: **Build → Build Solution** (`Ctrl+Shift+B`).

The simulator executable is produced at `MiddlewareSimulator/x64/Debug/MiddlewareSimulator.exe` (or `x64/Release/...`).

## How to Run / Use

1.  **Prepare a Data File:** A sample `simulated_data.txt` is included in the `MiddlewareSimulator/` project folder. The format is:
    * `CH|ChannelID|ChannelName|[Provider]|[ServiceType]|[LCN]`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description|[Genre]|[ParentalAge]`
    * StartTime/EndTime use `YYYY-MM-DD HH:MM:SS`, interpreted as **UTC**.
    * Example:
        ```
        CH|1|Channel One|MyProvider|1|101
        PG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|News|Current events|News|0
        CH|2|Channel Two
        PG|2|201|2025-04-16 10:30:00|2025-04-16 12:00:00|Movie|Action film|Movie|12
        ```

2.  **Run from Visual Studio:** Set `MiddlewareSimulator` as the startup project, open **Project → Properties → Debugging → Command Arguments**, enter the data file path (e.g. `simulated_data.txt`), and press `F5`.

3.  **Run from a terminal:**
    ```powershell
    cd MiddlewareSimulator\x64\Debug
    .\MiddlewareSimulator.exe ..\..\simulated_data.txt
    ```

4.  **Observe Output:** The program prints parsing status, warnings for malformed lines, the channel list, each channel's programs, an EPG "on now" query, and an **MPEG-TS pipeline demo**: it muxes the channels into a transport stream, writes it to **`output.ts`**, reads it back, and prints the recovered service/PID map.

## How to Run Tests

Tests use **GoogleTest** and live in the `MiddlewareSimulatorTests` project, which links the simulator's source/headers (one copy on disk, compiled into both the simulator and the test runner).

1. Set `MiddlewareSimulatorTests` as the startup project.
2. Build the solution (`Ctrl+Shift+B`).
3. Open **Test → Test Explorer** and click **Run All Tests**, or press `F5` to run the test executable in console mode.

The 50-test suite covers: time parsing (with a UTC-epoch regression test), file-parsing validation paths, orphan-record rejection, EPG queries, bit I/O, TS packet framing/resync, CRC-32 and PSI sections, PAT/PMT/SDT/EIT round-trips, DVB MJD/BCD time encoding, and the HDMI-CEC message codec — including known-byte-vector tests validated against the specifications.

## Scripts / Tooling

* **`generate_epg.py`** (Python 3) — generates synthetic EPG data in the simulator's pipe format. Useful for stress-testing the parser and as an independent, cross-language data source (Python writes, C++ reads).
    ```bash
    python generate_epg.py --channels 10 --programs-per-channel 8 --output big_epg.txt
    ```
* **`build_and_test.ps1`** (PowerShell) — locates MSBuild via `vswhere`, builds the solution, and runs the GoogleTest suite. One-command build + test from a terminal.
    ```powershell
    .\build_and_test.ps1                       # Debug | x64
    .\build_and_test.ps1 -Configuration Release
    ```

## Known Limitations / Future Improvements

* **Single-packet sections:** Each PSI/SI section is assumed to fit in one 188-byte packet. Real sections can span multiple packets; multi-section/multi-packet support is the next step (this also limits how many long-described EIT events fit in one service section).
* **Payload-only packets:** TS packets carry payload only — no adaptation field, so PCR/timing information is not modeled.
* **Descriptor coverage:** A representative subset of descriptors is implemented (service, short-event, content, parental-rating). Genre uses the DVB content-nibble level-1 subset; parental rating covers ages 4–18.
* **Time zones:** Times are treated as UTC; there is no per-record timezone field or DST handling for arbitrary zones.
* **Input format:** Text input uses a custom pipe format. Parsing **XMLTV** (a real text EPG format) and ingesting a captured binary `.ts` file would add realism.
* **Testing/CI:** Coverage is unit-level; no integration/fuzz tests and no CI pipeline yet.
* **Concurrency:** All processing is synchronous on the main thread.

---
# C++ 中间件模拟器

## 描述 / 目的

一个模拟电视 / 机顶盒中间件核心功能的命令行 C++ 应用程序。它展示 C++ 技能（OOD、STL、位级二进制解析、文件 I/O、构建系统、单元测试）以及广播领域知识（服务信息处理、EPG 处理、MPEG 传输流）。

模拟器做两件事：

1. **解析** 结构化文本格式，构建频道及其节目（EPG）的内存模型。
2. **将该模型编码为真实的 MPEG-TS 传输流**（PAT、PMT、SDT、EIT）并写入 `.ts` 文件，再 **解码回来** —— 一个完整的往返流程，模仿真实中间件处理 DVB SI/PSI 表的方式。

## 已实现功能

### 数据模型
* `struct ChannelInfo` —— 服务 ID、名称、提供商、服务类型、PMT/视频/音频 PID、逻辑频道号。
* `struct ProgramInfo` —— 事件 ID、名称、描述、起止时间（epoch 毫秒）、类型、家长分级。

### 文本解析器 (`Parser`)
* 解析自定义管道符 (`|`) 分隔格式，带记录类型前缀 (`CH`, `PG`)。
    * `CH|ID|Name|[Provider]|[ServiceType]|[LCN]`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description|[Genre]|[ParentalAge]`
    * 尾部字段可选，向后兼容。
* 频道存于 `std::vector`，节目存于以频道 ID 为键的 `std::map<int, std::vector<ProgramInfo>>`。
* 用 `std::set<int>` 跟踪已知频道 ID，拒绝其频道从未声明的“孤立”节目记录。
* 对无效数字 (`std::stoi`)、格式错误/不完整记录、结束早于开始的时间进行错误处理。

### 时间处理
* 时间字符串 (`YYYY-MM-DD HH:MM:SS`) 按 **UTC** 解析，存储为 `long long` epoch 毫秒。
* UTC 转换使用 `_mkgmtime`（Windows）/ `timegm`（POSIX），而非会应用本地时区偏移的 `mktime`。

### EPG 查询
* `getChannels()`、`getProgramsForChannel(id)`、`getProgramsOnNow(id, nowMs)`、`getProgramsForTimeRange(id, startMs, endMs)`（区间重叠逻辑）。

### MPEG-TS 流水线
一个从字节层构建的 DVB SI/PSI 表编/解码器：

* **位级 I/O** (`BitWriter` / `BitReader`) —— 对非字节对齐字段（13 位 PID、12 位 section_length 等）进行 MSB 优先打包。
* **TS 包** (`TsPacket*`) —— 188 字节包，`0x47` 同步字节，13 位 PID，连续计数器；读取器在同步字节上重新同步。
* **PSI 段** (`PsiSection`, `Crc32`) —— 通用段封装，由 **CRC-32/MPEG-2** 保护；损坏的段被拒绝。
* **PAT** —— 映射 `program_number` → PMT PID。
* **PMT** —— 每个服务的 PCR PID 和视频/音频基本流 (`stream_type`)。
* **SDT** —— `service_descriptor` (0x48)：服务名称、提供商、类型。
* **EIT** —— EPG：事件携带 **MJD + BCD** 起止/时长，以及 `short_event_descriptor` (0x4D)、`content_descriptor` (0x54，类型 nibble)、`parental_rating_descriptor` (0x55)。
* **Mux / Demux** (`TsMuxer` / `TsDemuxer`) —— 将频道列表 + 排期编码为传输流并解码回来。运行时模拟器写出真实的 `output.ts` 文件并读回。

### 单元测试
* 使用 **GoogleTest** 的 43 个测试，覆盖解析器、时间处理、EPG 查询以及 TS 流水线的每一层。
* 包含针对规范（而非仅自洽）验证的 **已知字节向量** 测试：CRC-32/MPEG-2 校验值 `0x0376E6E7`、MJD `0xC079`（DVB 附录 C 示例）、PAT 字节布局 `00 01 E1 00`。

### C++ 最佳实践
* 头文件保护符、解析器的声明/实现分离、STL 容器 (`vector`, `map`, `set`, `string`)、异常处理、基于类的设计。

## 架构 / 模块图

```
MiddlewareSimulator/
  数据模型 :  ChannelInfo.h, ProgramInfo.h
  文本解析 :  Parser.{h,cpp}, simulated_data.txt
  位级 I/O :  BitWriter.h, BitReader.h
  TS 包    :  TsPacket.h, TsPacketWriter.h, TsPacketReader.h
  PSI 封装 :  Crc32.h, PsiSection.h
  SI 表    :  PsiTables.h (PAT/PMT), SdtTable.h, EitTable.h, MjdTime.h
  Mux/Demux:  TsMuxer.h, TsDemuxer.h
  应用程序 :  main.cpp
MiddlewareSimulatorTests/
  Tests.cpp:  GoogleTest 套件（43 个测试）
```

同一个数据模型 (`ChannelInfo` / `ProgramInfo`) 是文本解析器和 TS 流水线的单一事实来源。

## 使用的技术

* **语言:** C++ (C++17)
* **核心库:** C++ 标准库 (`<vector>`, `<map>`, `<set>`, `<string>`, `<array>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, 时间用 `<chrono>`/`<ctime>`, 定宽二进制类型用 `<cstdint>`)。
* **测试:** GoogleTest（NuGet：`Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn`）。
* **构建系统:** Visual Studio 2022 / MSBuild（`.sln`, `.vcxproj`）。
* **编译器:** MSVC（v143 工具集，随 Visual Studio 2022 提供）。

## 设置 / 先决条件

* 安装了 **“使用 C++ 的桌面开发”** 工作负载的 Visual Studio 2022。
* 符合 C++17 标准的编译器（MSVC 19.30+，随 Visual Studio 2022 提供）。
* Git（用于克隆）。

## 如何构建

本项目使用 **Visual Studio 2022**（MSBuild）构建。任何版本（社区版 / 专业版 / 企业版）均可。

1. 克隆仓库: `git clone <repository-url>`
2. 在 Visual Studio 2022 中打开 `MiddlewareSimulator/MiddlewareSimulator.sln`。
3. 选择构建配置 (`Debug` 或 `Release`) 和平台 (`x64`)。
4. 构建: **生成 → 生成解决方案** (`Ctrl+Shift+B`)。

模拟器可执行文件生成于 `MiddlewareSimulator/x64/Debug/MiddlewareSimulator.exe`（或 `x64/Release/...`）。

## 如何运行 / 使用

1.  **准备数据文件:** `MiddlewareSimulator/` 项目文件夹中含示例 `simulated_data.txt`。格式：
    * `CH|ChannelID|ChannelName|[Provider]|[ServiceType]|[LCN]`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description|[Genre]|[ParentalAge]`
    * StartTime/EndTime 使用 `YYYY-MM-DD HH:MM:SS`，按 **UTC** 解释。
    * 示例:
        ```
        CH|1|Channel One|MyProvider|1|101
        PG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|News|Current events|News|0
        CH|2|Channel Two
        PG|2|201|2025-04-16 10:30:00|2025-04-16 12:00:00|Movie|Action film|Movie|12
        ```

2.  **从 Visual Studio 运行:** 将 `MiddlewareSimulator` 设为启动项目，打开 **项目 → 属性 → 调试 → 命令参数**，输入数据文件路径（例如 `simulated_data.txt`），按 `F5`。

3.  **从终端运行:**
    ```powershell
    cd MiddlewareSimulator\x64\Debug
    .\MiddlewareSimulator.exe ..\..\simulated_data.txt
    ```

4.  **观察输出:** 程序打印解析状态、格式错误行警告、频道列表、每个频道的节目、一个 EPG “正在播放” 查询，以及一个 **MPEG-TS 流水线演示**：将频道复用为传输流、写入 **`output.ts`**、读回并打印恢复出的服务/PID 映射。

## 如何运行测试

测试使用 **GoogleTest**，位于 `MiddlewareSimulatorTests` 项目中，该项目链接模拟器的源/头文件（磁盘上仅一份副本，同时编译进模拟器和测试运行器）。

1. 将 `MiddlewareSimulatorTests` 设为启动项目。
2. 生成解决方案 (`Ctrl+Shift+B`)。
3. 打开 **测试 → 测试资源管理器** 并点击 **全部运行**，或按 `F5` 在控制台模式下运行。

这 50 个测试覆盖：时间解析（含 UTC epoch 回归测试）、文件解析验证路径、孤立记录拒绝、EPG 查询、位 I/O、TS 包封装/重同步、CRC-32 与 PSI 段、PAT/PMT/SDT/EIT 往返、DVB MJD/BCD 时间编码，以及 HDMI-CEC 消息编解码 —— 包含针对规范验证的已知字节向量测试。

## 脚本 / 工具

* **`generate_epg.py`** (Python 3) —— 以模拟器的管道格式生成合成 EPG 数据。用于压力测试解析器，以及作为独立的跨语言数据源（Python 写、C++ 读）。
    ```bash
    python generate_epg.py --channels 10 --programs-per-channel 8 --output big_epg.txt
    ```
* **`build_and_test.ps1`** (PowerShell) —— 通过 `vswhere` 定位 MSBuild，构建解决方案并运行 GoogleTest 测试套件。从终端一条命令完成构建 + 测试。
    ```powershell
    .\build_and_test.ps1                       # Debug | x64
    .\build_and_test.ps1 -Configuration Release
    ```

## 已知限制 / 未来改进

* **单包段:** 假设每个 PSI/SI 段都能放入一个 188 字节的包。真实段可跨多个包；多段/多包支持是下一步（这也限制了单个服务段能容纳多少带长描述的 EIT 事件）。
* **仅负载包:** TS 包仅携带负载 —— 没有自适应字段，因此未建模 PCR/时序信息。
* **描述符覆盖:** 实现了具有代表性的描述符子集（service、short-event、content、parental-rating）。类型使用 DVB content-nibble 一级子集；家长分级覆盖 4–18 岁。
* **时区:** 时间按 UTC 处理；没有每条记录的时区字段或任意时区的夏令时处理。
* **输入格式:** 文本输入使用自定义管道格式。解析 **XMLTV**（真实的文本 EPG 格式）以及读取捕获的二进制 `.ts` 文件将增加真实性。
* **测试/CI:** 仅单元级覆盖；尚无集成/模糊测试和 CI 流水线。
* **并发性:** 所有处理都在主线程上同步进行。
