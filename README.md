# C++ Middleware Simulator - 6-Week Learning Project

## Description / Purpose

This is a command-line C++ application developed simulating basic functionalities of TV/Set-Top-Box middleware. It focuses on demonstrating core C++ skills (OOD, STL, File I/O, Build Systems) and understanding middleware concepts relevant to the broadcast industry (Service Information processing, EPG data handling). The goal was to create a functional simulator that can parse structured data representing channels and programs, mimicking how real middleware processes SI/PSI tables.

## Features Implemented

* **Data Structures:** Defines `struct ChannelInfo` and `struct ProgramInfo` to hold parsed data.
* **Parsing Logic:** Implements a `Parser` class responsible for reading simulated data.
    * Parses a custom pipe-delimited (`|`) format with record type prefixes (`CH`, `PG`) from an input file.
    * Handles Channel (`CH|ID|Name`) and Program (`PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description`) records.
    * Stores parsed channels in a `std::vector` and programs in a `std::map<int, std::vector<ProgramInfo>>` keyed by Channel ID.
    * Includes basic error handling for invalid number formats (`std::stoi`) and malformed records during parsing.
    * Includes basic data validation (e.g., checks for expected number of parts per record type).
* **File Input:** Reads simulated data from a text file specified via a command-line argument.
* **EPG Queries:** Implements basic EPG logic:
    * `getChannels()`: Returns all parsed channels.
    * `getProgramsForChannel(int channelId)`: Returns all programs associated with a specific channel ID.
    * `getProgramsOnNow(int channelId, const std::string& currentTimeStr)`: Basic simulation finding programs active at a specific time (using simple HH:MM string comparison - **limitation noted**).
    * `getProgramsForTimeRange(int channelId, long long startMillis, long long endMillis)`: Finds programs overlapping a given time range (assumes time stored as epoch milliseconds).
* **Build System:** Uses CMake (`CMakeLists.txt`) for cross-platform building. Configured for C++17.
* **Command-Line Arguments:** Accepts the input data file path as a command-line argument (`argc`, `argv`). Includes basic usage instructions if the argument is missing.
* **Basic Testing:** Includes placeholder structure/concept for simple test functions callable via a `--test` flag (implementation of tests may be basic).
* **C++ Best Practices:** Uses header guards, separates declaration (`.h`) from implementation (`.cpp`), utilizes STL containers (`vector`, `map`, `string`), includes basic exception handling (`try-catch`), and demonstrates class design (Parser). Time data stored as `long long` (epoch ms) for robustness in later stages.

## Technologies Used

* **Language:** C++ (Targeting C++17)
* **Core Libraries:** C++ Standard Library (STL - `<vector>`, `<map>`, `<string>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, `<chrono>`/`<ctime>` for time conversion), `<cassert>` (optional for simple tests).
* **Build System:** CMake (version 3.10+)
* **Compiler:** g++ or Clang (or MSVC on Windows)

## Setup / Prerequisites

* A C++17 compliant compiler (g++, Clang, MSVC).
* CMake (version 3.10 or higher).
* Make (or Ninja, or Visual Studio Build Tools) - the build tool invoked by CMake.
* Git (for cloning).

## How to Build

1.  Clone the repository: `git clone <repository-url>`
2.  Navigate to the project directory: `cd MiddlewareSimulator`
3.  Create a build directory: `mkdir build`
4.  Navigate into the build directory: `cd build`
5.  Run CMake to configure: `cmake ..`
6.  Run the build tool: `make` (or `cmake --build .`)

This will create an executable named `MiddlewareSimulator` (or `MiddlewareSimulator.exe`) inside the `build` directory.

## How to Run / Use

1.  **Prepare Data File:** Create a text file (e.g., `simulated_data.txt`) in the *project's root directory* (where `CMakeLists.txt` is). Populate it with data lines using the specified format:
    * `CH|ChannelID|ChannelName`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|ProgramName|ProgramDescription`
    * *Note on Time:* The parser currently expects StartTime/EndTime in a simple format like `YYYY-MM-DD HH:MM:SS` (or adjust parser) to convert to epoch milliseconds.
    * Example `simulated_data.txt`:
        ```
        CH|1|Channel One
        PG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|News|Current events
        PG|1|102|2025-04-16 10:00:00|2025-04-16 10:30:00|Weather|Forecast
        CH|2|Channel Two
        PG|2|201|2025-04-16 10:30:00|2025-04-16 12:00:00|Movie|Action film
        CH|invalid|Bad Channel Format
        ```

2.  **Run from Build Directory:** Navigate to the `build` directory in your terminal.
3.  **Execute:** Run the simulator, providing the path to your data file (relative to the build directory, so `../` is needed).
    ```bash
    ./MiddlewareSimulator ../simulated_data.txt
    ```
4.  **Observe Output:** The program will print parsing status, error/warning messages for malformed lines, the list of parsed channels, and the programs associated with each channel. It will also show results for the hardcoded EPG time queries in `main.cpp`.

5.  **Run Tests (Basic):**
    ```bash
    ./MiddlewareSimulator --test
    ```
    (This assumes the test logic was added to `main.cpp` behind the `--test` flag).

## Known Limitations / Future Improvements

* **Parsing Robustness:** The current parser is basic. It assumes well-formed data and doesn't handle complex edge cases (e.g., delimiters within data fields, character encoding issues). Error handling is basic logging.
* **Time Handling:** Time comparison for `getProgramsOnNow` might still be using string comparison if the chrono/epoch refactor wasn't fully completed. Proper time zone handling is missing. Assumes a simple input time format.
* **Data Model:** Data structures (`ChannelInfo`, `ProgramInfo`) are simplified and missing many fields found in real SI tables (e.g., detailed event descriptors, parental ratings, audio/video PIDs, genres).
* **EPG Logic:** EPG queries are basic ("on now", "time range"). Real EPGs handle complex scheduling, event overlaps, series linking, etc.
* **Input Format:** Uses a simple custom text format. Real middleware parses complex binary MPEG Transport Stream tables (DVB/ATSC SI/PSI). Parsing XMLTV format would be a good next step for realism with text-based data.
* **Testing:** Unit tests are very basic placeholders or rely on `assert`. A proper framework like Google Test should be used for thorough testing.
* **Concurrency:** All parsing and querying happens synchronously on the main thread.

---
# C++ 中间件模拟器 - 6周学习项目

## 描述 / 目的

这是一个应用程序模拟了电视/机顶盒中间件的基本功能。它专注于展示核心 C++ 技能（OOD、STL、文件 I/O、构建系统）以及对广播行业相关的中间件概念（服务信息处理、EPG 数据处理）的理解。目标是创建一个功能性模拟器，能够解析代表频道和节目的结构化数据，模仿真实中间件处理 SI/PSI 表的方式。

## 已实现功能

* **数据结构:** 定义了 `struct ChannelInfo` 和 `struct ProgramInfo` 来保存解析后的数据。
* **解析逻辑:** 实现了一个 `Parser` 类，负责读取模拟数据。
    * 解析来自输入文件的自定义管道符分隔 (`|`) 格式，带有记录类型前缀 (`CH`, `PG`)。
    * 处理频道 (`CH|ID|Name`) 和节目 (`PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description`) 记录。
    * 将解析的频道存储在 `std::vector` 中，并将节目存储在以频道 ID 为键的 `std::map<int, std::vector<ProgramInfo>>` 中。
    * 包含对解析过程中无效数字格式 (`std::stoi`) 和格式错误的记录的基本错误处理。
    * 包含基本的数据验证（例如，检查每种记录类型的预期部分数量）。
* **文件输入:** 从命令行参数指定的文本文件中读取模拟数据。
* **EPG 查询:** 实现了基本的 EPG 逻辑：
    * `getChannels()`: 返回所有解析的频道。
    * `getProgramsForChannel(int channelId)`: 返回与特定频道 ID 关联的所有节目。
    * `getProgramsOnNow(int channelId, const std::string& currentTimeStr)`: 基本模拟，查找在特定时间正在播放的节目（目前使用简单的 HH:MM 字符串比较 - **已注意局限性**）。
    * `getProgramsForTimeRange(int channelId, long long startMillis, long long endMillis)`: 查找与给定时间范围重叠的节目（假设时间存储为 epoch 毫秒）。
* **构建系统:** 使用 CMake (`CMakeLists.txt`) 进行跨平台构建。配置为 C++17。
* **命令行参数:** 接受输入数据文件路径作为命令行参数 (`argc`, `argv`)。如果缺少参数，则包含基本的使用说明。
* **基本测试:** 包含通过 `--test` 标志调用的简单测试函数的占位符结构/概念（测试的实现可能很简单）。
* **C++ 最佳实践:** 使用头文件保护符，将声明 (`.h`) 与实现 (`.cpp`) 分离，利用 STL 容器 (`vector`, `map`, `string`)，包含基本的异常处理 (`try-catch`)，并演示了类设计 (Parser)。为了后续的稳健性，时间数据存储为 `long long` (epoch ms)。

## 使用的技术

* **语言:** C++ (目标 C++17)
* **核心库:** C++ 标准库 (STL - `<vector>`, `<map>`, `<string>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, 用于时间转换的 `<chrono>`/`<ctime>`), `<cassert>` (可选，用于简单测试)。
* **构建系统:** CMake (版本 3.10+)
* **编译器:** g++ 或 Clang (或 Windows 上的 MSVC)

## 设置 / 先决条件

* 符合 C++17 标准的编译器 (g++, Clang, MSVC)。
* CMake (版本 3.10 或更高)。
* Make (或 Ninja, 或 Visual Studio Build Tools) - CMake 调用​​的构建工具。
* Git (用于克隆)。

## 如何构建

1.  克隆仓库: `git clone <repository-url>`
2.  导航到项目目录: `cd MiddlewareSimulator`
3.  创建构建目录: `mkdir build`
4.  导航到构建目录: `cd build`
5.  运行 CMake 进行配置: `cmake ..`
6.  运行构建工具: `make` (或 `cmake --build .`)

这将在 `build` 目录内创建一个名为 `MiddlewareSimulator` (或 `MiddlewareSimulator.exe`) 的可执行文件。

## 如何运行 / 使用

1.  **准备数据文件:** 在项目的*根目录*（`CMakeLists.txt` 所在的目录）中创建一个文本文件（例如 `simulated_data.txt`）。使用指定格式填充数据行：
    * `CH|ChannelID|ChannelName`
    * `PG|ChannelID|ProgramID|StartTime|EndTime|ProgramName|ProgramDescription`
    * *时间注意:* 解析器当前期望 StartTime/EndTime 采用简单格式，如 `YYYY-MM-DD HH:MM:SS` (或调整解析器) 以转换为 epoch 毫秒。
    * 示例 `simulated_data.txt`:
        ```
        CH|1|Channel One
        PG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|News|Current events
        PG|1|102|2025-04-16 10:00:00|2025-04-16 10:30:00|Weather|Forecast
        CH|2|Channel Two
        PG|2|201|2025-04-16 10:30:00|2025-04-16 12:00:00|Movie|Action film
        CH|invalid|Bad Channel Format
        ```

2.  **从构建目录运行:** 在终端导航到 `build` 目录。
3.  **执行:** 运行模拟器，提供数据文件的路径（相对于构建目录，因此需要 `../`）。
    ```bash
    ./MiddlewareSimulator ../simulated_data.txt
    ```
4.  **观察输出:** 程序将打印解析状态、格式错误行的错误/警告消息、解析的频道列表以及与每个频道关联的节目。它还将显示 `main.cpp` 中硬编码的 EPG 时间查询结果。

5.  **运行测试 (基本):**
    ```bash
    ./MiddlewareSimulator --test
    ```
    (假设测试逻辑已添加到 `main.cpp` 中，并由 `--test` 标志触发)。

## 已知限制 / 未来改进

* **解析健壮性:** 当前的解析器比较基础。它假设数据格式良好，并且不处理复杂的边缘情况（例如，数据字段中包含分隔符、字符编码问题）。错误处理只是基本的日志记录。
* **时间处理:** `getProgramsOnNow` 的时间比较可能仍在使用字符串比较（如果 chrono/epoch 重构未完全完成）。缺少适当的时区处理。假设输入时间格式简单。
* **数据模型:** 数据结构 (`ChannelInfo`, `ProgramInfo`) 比较简化，缺少真实 SI 表中的许多字段（例如，详细的事件描述符、家长分级、音频/视频 PID、类型）。
* **EPG 逻辑:** EPG 查询是基本的（“正在播放”、“时间范围”）。真实的 EPG 会处理复杂的调度、事件重叠、系列链接等。
* **输入格式:** 使用简单的自定义文本格式。真实的中间件解析复杂的二进制 MPEG 传输流表（DVB/ATSC SI/PSI）。解析 XMLTV 格式将是基于文本数据实现真实性的良好下一步。
* **测试:** 单元测试是非常基本的占位符或依赖于 `assert`。应使用像 Google Test 这样的正规框架进行全面测试。
* **并发性:** 所有解析和查询都在主线程上同步进行。