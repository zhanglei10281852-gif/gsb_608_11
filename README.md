# Qt 模块级分级日志系统

## How to Run

### 方式一：Docker 运行（推荐，无需本地安装 Qt）

```bash
# 构建并运行（构建过程中自动执行全部测试，测试失败则构建中断）
docker-compose up --build
```

### 方式二：一键 Docker 测试

```bash
# Windows
run_tests.bat

# Linux / macOS
chmod +x run_tests.sh && ./run_tests.sh
```

脚本会自动构建测试镜像 → 运行全部 45 个用例 → 输出详细结果 → 清理镜像。

### 方式三：本地编译运行

前置条件：Qt6 开发环境、CMake 3.16+

```bash
cd qt-log-system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 运行主程序
./build/QtLogSystem

# 运行测试
cd build && ctest --output-on-failure
# 或查看详细输出
./tst_log_helper -v2
```

### 方式四：环境变量配置后运行

```bash
# Linux / macOS
export QT_LOGGING_RULES="app.*.debug=false;app.network.debug=true"
./build/QtLogSystem

# Windows CMD
set QT_LOGGING_RULES=app.*.debug=false;app.network.debug=true
build\QtLogSystem.exe

# Windows PowerShell
$env:QT_LOGGING_RULES="app.*.debug=false;app.network.debug=true"
.\build\QtLogSystem.exe
```

## Services

| 服务 | 说明 | 技术栈 | Docker |
|------|------|--------|--------|
| qt-log-system | 主程序（含 6 个演示场景） | Qt6 + CMake + C++17 | 构建时自动跑 ctest |
| qt-log-test | 测试服务（45 个单元测试） | Qt6 + QTest | 运行时输出详细结果 |

## 测试账号

本项目为 C++ 控制台应用，无需登录账号。

## 题目内容

基于 Qt C++ 原生日志系统（QLoggingCategory + qCDebug/qCInfo/qCWarning/qCCritical），实现一个完整的模块级+分级控制的日志系统，支持：

1. 模块级控制：每个业务模块独立开启/关闭日志
2. 分级关闭控制：每个模块单独设置日志级别，自动过滤低级别日志
3. 双配置方式：代码动态配置 + 环境变量静态配置
4. 日志标识：输出携带模块名和日志级别

---

## 项目结构

```
.
├── docker-compose.yml              # Docker Compose 编排（主程序 + 测试）
├── run_tests.bat                   # Windows 一键 Docker 测试脚本
├── run_tests.sh                    # Linux/macOS 一键 Docker 测试脚本
├── .gitignore
├── README.md
├── docs/
│   └── project_design.md           # 系统设计文档（含架构图、ER图）
└── qt-log-system/
    ├── CMakeLists.txt               # CMake 构建配置（主程序 + 测试目标）
    ├── Dockerfile                   # 主程序镜像（多阶段构建，构建时自动跑测试）
    ├── Dockerfile.test              # 测试专用镜像（运行时输出详细测试结果）
    ├── src/
    │   ├── main.cpp                 # 主函数 + 6 个演示场景
    │   ├── log_categories.h/cpp     # 日志分类声明与定义（集中管理）
    │   ├── log_helper.h/cpp         # 日志辅助工具类（级别控制、批量配置、线程安全）
    │   ├── log_formatter.h/cpp      # 日志格式化输出模块（独立封装、线程安全）
    │   ├── network_module.h/cpp     # 网络模块示例
    │   ├── database_module.h/cpp    # 数据库模块示例
    │   └── fileio_module.h/cpp      # 文件IO模块示例
    └── tests/
        └── tst_log_helper.cpp       # QTest 单元测试（45 个用例）
```

## 整体设计思路

### 核心实现逻辑

1. **模块分类定义**：通过 `Q_DECLARE_LOGGING_CATEGORY` / `Q_LOGGING_CATEGORY` 为每个业务模块定义独立的日志分类（如 `app.network`、`app.database`、`app.fileio`、`app.ui`），集中在 `log_categories.h/cpp` 中管理。

2. **过滤规则配置**：利用 Qt 原生的 `QLoggingCategory::setFilterRules()` 设置过滤规则。规则格式为 `分类名.级别=true/false`，支持通配符（如 `app.*.debug=false`）。后设置的规则优先级更高。

3. **级别控制原理**：`LogHelper` 工具类将级别概念（Debug < Info < Warning < Critical < Off）转换为 Qt 原生过滤规则。设置某模块最低级别为 Warning 时，自动生成 `debug=false, info=false, warning=true, critical=true` 的规则组合。

4. **消息格式化**：`LogFormatter` 独立模块封装了日志格式化逻辑，通过 `qInstallMessageHandler` 安装自定义处理器，输出格式为 `[时间] [级别] [模块名] 消息内容`，使用 QMutex 保证线程安全。集成只需一行代码：`LogFormatter::install()`。

5. **与事件循环的关系**：本日志系统的所有功能（分类过滤、级别控制、格式化输出）均为同步操作，**不依赖 Qt 事件循环**。`main.cpp` 演示程序中虽然创建了 `QCoreApplication` 但未调用 `app.exec()` 启动事件循环，日志系统仍然完全正常工作。详细说明见下方 [注意事项 > 3. 事件循环（QEventLoop）](#3-事件循环qeventloop) 章节。

### 工程质量保障

1. **线程安全**：`LogHelper` 内部使用 `QMutex` + `QMutexLocker` 保护所有公共方法对 `s_moduleLevels` 的读写操作，可在多线程环境下安全调用配置接口。`LogFormatter` 的输出操作同样通过独立的 `QMutex` 保护，防止多线程日志交错。

2. **规则无限增长修复**：使用 `QMap<QString, Level>` 维护每个模块的当前级别，每次变更时通过 `rebuildAndApplyRules()` 从 QMap 重新生成完整规则字符串，替代旧的字符串追加（`+=`）方式，彻底避免规则无限堆积导致的内存增长和解析性能下降。

3. **格式化逻辑封装**：将原本内联在 `main.cpp` 中的 `customMessageHandler` 提取为独立的 `LogFormatter` 模块（`log_formatter.h/cpp`），提供 `install()/uninstall()/messageHandler()` 接口，用户拷贝 `src/` 目录即可获得完整的可集成日志系统。


## 配置详解

### 代码动态配置

#### 单个模块配置

```cpp
#include "log_helper.h"

// 设置网络模块仅输出 Warning 及以上级别
LogHelper::setModuleLevel("app.network", LogHelper::Level::Warning);

// 完全禁用数据库模块日志
LogHelper::disableModule("app.database");

// 恢复文件模块所有日志
LogHelper::enableModule("app.fileio");
```

#### 批量模块配置

```cpp
// 一次性配置多个模块的不同级别
LogHelper::setModuleLevels({
    {"app.network",  LogHelper::Level::Warning},   // 网络: Warning+
    {"app.database", LogHelper::Level::Off},        // 数据库: 全部关闭
    {"app.fileio",   LogHelper::Level::Info}         // 文件: Info+
});
```

#### 全局默认级别

```cpp
// 所有 app.* 模块默认关闭 Debug
LogHelper::setGlobalLevel(LogHelper::Level::Info);

// 再单独开启某个模块的 Debug（后设置优先级更高）
LogHelper::setModuleLevel("app.network", LogHelper::Level::Debug);
```

#### 原始规则字符串（高级用法）

```cpp
LogHelper::applyRawRules(
    "app.*.debug=false\n"
    "app.*.info=false\n"
    "app.network.debug=true\n"
    "app.network.info=true\n"
);
```

### 环境变量静态配置

通过 `QT_LOGGING_RULES` 环境变量在程序启动前配置，无需修改代码。

#### Linux / macOS (bash)

```bash
# 全局关闭 Debug 日志
export QT_LOGGING_RULES="app.*.debug=false"

# 多条规则用分号分隔
export QT_LOGGING_RULES="app.*.debug=false;app.network.debug=true;app.database=false"

# 运行程序
./build/QtLogSystem
```

#### Windows CMD

```cmd
set QT_LOGGING_RULES=app.*.debug=false;app.network.debug=true
build\QtLogSystem.exe
```

#### Windows PowerShell

```powershell
$env:QT_LOGGING_RULES="app.*.debug=false;app.network.debug=true"
.\build\QtLogSystem.exe
```

#### macOS (zsh)

```zsh
export QT_LOGGING_RULES="app.*.debug=false;app.network.warning=false"
./build/QtLogSystem
```

> **注意**：环境变量中多条规则用分号 `;` 分隔（不是换行符）。代码中 `setFilterRules()` 的优先级高于环境变量。

## 常见场景示例

以下 6 个场景均在 `main.cpp` 中实现为可直接运行的演示代码，运行主程序即可查看每个场景的实际输出效果。

### 场景1：默认状态 — 所有模块输出所有级别

适用场景：开发初期，需要查看所有模块的完整日志输出以了解系统运行状态。

```cpp
// 无需任何配置，系统默认所有模块的所有级别均开启
NetworkModule net;
DatabaseModule db;
FileIOModule file;

net.sendRequest("https://api.example.com/users");   // 输出 Debug + Info
db.connect("localhost", 3306);                        // 输出 Debug + Info
file.readFile("/data/config.json");                   // 输出 Debug + Info
```

配置逻辑：`Q_LOGGING_CATEGORY` 定义时默认最低级别为 `QtDebugMsg`，即所有级别均通过。

预期输出：每个模块的 Debug、Info、Warning、Critical 日志全部输出，格式为 `[时间] [级别] [模块名] 消息`。

### 场景2：禁用数据库模块所有日志

适用场景：数据库模块日志量大且已稳定运行，临时关闭以减少日志噪音，专注排查其他模块问题。

```cpp
// 方式A: 代码配置
LogHelper::disableModule("app.database");

// 方式B: 环境变量
// export QT_LOGGING_RULES="app.database=false"
```

配置逻辑：`disableModule()` 内部调用 `setModuleLevel("app.database", Level::Off)`，生成规则 `app.database.debug=false`、`app.database.info=false`、`app.database.warning=false`、`app.database.critical=false`。

预期输出：数据库模块的 Debug/Info/Warning/Critical 全部不输出，网络模块和文件模块不受影响，正常输出所有级别日志。

### 场景3：仅保留文件模块 Critical 日志

适用场景：文件IO模块在正常运行时日志量大，仅需关注磁盘满、写入失败等严重错误。

```cpp
// 方式A: 代码配置
LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Critical);

// 方式B: 环境变量
// export QT_LOGGING_RULES="app.fileio.debug=false;app.fileio.info=false;app.fileio.warning=false"
```

配置逻辑：设置最低级别为 Critical，自动生成 `debug=false, info=false, warning=false, critical=true` 的规则组合。

预期输出：文件模块的 `readFile()`、`writeFile()` 中的 Debug/Info 日志被过滤，仅 `simulateDiskFull()` 中的 Critical 日志（"Disk full, write operation aborted"）输出。

### 场景4：全局默认过滤 Debug，仅网络模块开启 Debug

适用场景：系统整体进入稳定阶段，关闭大部分 Debug 日志降低噪音，但网络模块正在排查连接问题，需要保留 Debug 级别。

```cpp
// 方式A: 代码配置
LogHelper::setGlobalLevel(LogHelper::Level::Info);                    // 全局: Info+
LogHelper::setModuleLevel("app.network", LogHelper::Level::Debug);    // 网络: Debug+

// 方式B: 环境变量
// export QT_LOGGING_RULES="app.*.debug=false;app.network.debug=true"
```

配置逻辑：先通过 `setGlobalLevel()` 设置通配符规则 `app.*.debug=false`，再通过 `setModuleLevel()` 设置具体规则 `app.network.debug=true`。Qt 规则引擎中，具体分类规则优先于通配符规则，因此网络模块的 Debug 不受全局规则影响。

预期输出：网络模块输出全部 4 个级别（含 Debug），数据库和文件模块仅输出 Info/Warning/Critical（Debug 被全局规则过滤）。

### 场景5：批量配置多个模块不同级别

适用场景：生产环境中根据各模块的重要程度和稳定性，一次性为所有模块设置差异化的日志级别。

```cpp
// 方式A: 代码配置
LogHelper::setModuleLevels({
    {"app.network",  LogHelper::Level::Warning},   // 网络: 仅 Warning+（已稳定）
    {"app.database", LogHelper::Level::Off},        // 数据库: 全部关闭（日志量大）
    {"app.fileio",   LogHelper::Level::Info}         // 文件: Info+（需要监控读写）
});

// 方式B: 环境变量
// export QT_LOGGING_RULES="app.network.debug=false;app.network.info=false;app.database=false;app.fileio.debug=false"
```

配置逻辑：`setModuleLevels()` 接受 `QMap<QString, Level>`，内部遍历所有条目更新 `s_moduleLevels` 映射表，然后一次性调用 `rebuildAndApplyRules()` 生成完整规则并应用，避免多次调用 `setFilterRules()` 的开销。

预期输出：网络模块仅输出 `simulateTimeout()` 中的 Warning 和 Critical；数据库模块完全静默；文件模块输出 Info/Warning/Critical（Debug 被过滤）；UI 模块未配置，保持默认全部输出。

### 场景6：使用原始规则字符串（高级用法）

适用场景：需要精细控制规则优先级，或从配置文件/远程服务加载规则字符串直接应用。

```cpp
// 方式A: 代码配置
LogHelper::applyRawRules(
    "app.*.debug=false\n"       // 全局关闭 Debug
    "app.*.info=false\n"        // 全局关闭 Info
    "app.network.debug=true\n"  // 网络模块例外: 开启 Debug
    "app.network.info=true\n"   // 网络模块例外: 开启 Info
);

// 方式B: 环境变量
// export QT_LOGGING_RULES="app.*.debug=false;app.*.info=false;app.network.debug=true;app.network.info=true"
```

配置逻辑：`applyRawRules()` 会清空内部 `s_moduleLevels` 映射表，将传入的原始规则字符串直接传给 `QLoggingCategory::setFilterRules()`。规则按行解析，后出现的规则覆盖先出现的，具体分类（`app.network`）优先于通配符（`app.*`）。

预期输出：网络模块输出全部 4 个级别；数据库模块仅输出 Warning/Critical（Debug 和 Info 被通配符规则关闭）。

## 测试用例

### 运行测试

```bash
# 方式一：一键 Docker 测试（推荐，无需本地 Qt 环境）
run_tests.bat          # Windows
./run_tests.sh         # Linux/macOS

# 方式二：docker-compose（测试 + 主程序一起跑）
docker-compose up --build

# 方式三：本地编译运行
cd qt-log-system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
# 或查看详细输出:
./tst_log_helper -v2
```

### 测试结果（Docker 实测）

```
********* Start testing of TestLogHelper *********
Config: Using QtTest library 6.2.4, Qt 6.2.4 (x86_64-little_endian-lp64 shared (dynamic) release build; by GCC 11.3.0), ubuntu 22.04
PASS   : TestLogHelper::initTestCase()
PASS   : TestLogHelper::testLevelName_Debug()
PASS   : TestLogHelper::testLevelName_Info()
PASS   : TestLogHelper::testLevelName_Warning()
PASS   : TestLogHelper::testLevelName_Critical()
PASS   : TestLogHelper::testLevelName_Off()
PASS   : TestLogHelper::testDefaultState_AllLevelsEnabled()
PASS   : TestLogHelper::testDefaultState_AllModulesEnabled()
PASS   : TestLogHelper::testSetModuleLevel_DebugLevel_AllPass()
PASS   : TestLogHelper::testSetModuleLevel_InfoLevel_FilterDebug()
PASS   : TestLogHelper::testSetModuleLevel_WarningLevel_FilterDebugAndInfo()
PASS   : TestLogHelper::testSetModuleLevel_CriticalLevel_OnlyCritical()
PASS   : TestLogHelper::testSetModuleLevel_OffLevel_NonePass()
PASS   : TestLogHelper::testModuleIsolation_DisableOneModuleOthersUnaffected()
PASS   : TestLogHelper::testModuleIsolation_DifferentLevelsPerModule()
PASS   : TestLogHelper::testDisableModule_AllLogsSuppressed()
PASS   : TestLogHelper::testEnableModule_AllLogsRestored()
PASS   : TestLogHelper::testDisableEnable_Cycle()
PASS   : TestLogHelper::testSetGlobalLevel_Info_AllModulesFilterDebug()
PASS   : TestLogHelper::testSetGlobalLevel_Off_AllModulesSilenced()
PASS   : TestLogHelper::testSetGlobalLevel_Debug_AllModulesFullOutput()
PASS   : TestLogHelper::testGlobalWithException_GlobalInfoButNetworkDebug()
PASS   : TestLogHelper::testGlobalWithException_GlobalWarningButFileIOInfo()
PASS   : TestLogHelper::testSetModuleLevels_BatchConfig()
PASS   : TestLogHelper::testSetModuleLevels_EmptyMap_NoChange()
PASS   : TestLogHelper::testSetModuleLevels_SingleEntry()
PASS   : TestLogHelper::testApplyRawRules_DisableAllDebug()
PASS   : TestLogHelper::testApplyRawRules_ComplexRules()
PASS   : TestLogHelper::testApplyRawRules_EmptyString_ResetsToDefault()
PASS   : TestLogHelper::testLogMessageContent_CorrectText()
PASS   : TestLogHelper::testLogMessageContent_CategoryTag()
PASS   : TestLogHelper::testLevelOverride_LaterRuleWins()
PASS   : TestLogHelper::testLevelOverride_MultipleOverrides()
PASS   : TestLogHelper::testEdgeCase_SetLevelOnUnknownCategory()
PASS   : TestLogHelper::testEdgeCase_RapidToggle()
PASS   : TestLogHelper::testEdgeCase_EmptyCategory()
PASS   : TestLogHelper::testGlobalOffThenRestoreSingleModule()
PASS   : TestLogHelper::testPreciseFiltering(Debug)
PASS   : TestLogHelper::testPreciseFiltering(Info)
PASS   : TestLogHelper::testPreciseFiltering(Warning)
PASS   : TestLogHelper::testPreciseFiltering(Critical)
PASS   : TestLogHelper::testPreciseFiltering(Off)
PASS   : TestLogHelper::testApplyRawRules_OverridesPreviousSetModuleLevel()
PASS   : TestLogHelper::testMultiModuleSimultaneous()
PASS   : TestLogHelper::cleanupTestCase()
Totals: 45 passed, 0 failed, 0 skipped, 0 blacklisted, 7ms
********* Finished testing of TestLogHelper *********
```

### 测试框架

基于 Qt 自带的 QTest 框架，通过 `qInstallMessageHandler` 拦截日志输出到内存列表，对每条日志的级别、分类名、消息内容进行断言验证。每个测试用例执行前自动重置过滤规则和清空捕获日志，确保用例间无状态污染。

### 测试用例清单

共 16 组、37 个测试用例（含 5 组数据驱动子用例），覆盖所有公共 API 及边界场景。

| 编号 | 测试组 | 用例名 | 验证内容 | 预期结果 |
|------|--------|--------|---------|---------|
| 1.1 | levelName() | testLevelName_Debug | `levelName(Debug)` 返回值 | "Debug" |
| 1.2 | levelName() | testLevelName_Info | `levelName(Info)` 返回值 | "Info" |
| 1.3 | levelName() | testLevelName_Warning | `levelName(Warning)` 返回值 | "Warning" |
| 1.4 | levelName() | testLevelName_Critical | `levelName(Critical)` 返回值 | "Critical" |
| 1.5 | levelName() | testLevelName_Off | `levelName(Off)` 返回值 | "Off" |
| 2.1 | 默认状态 | testDefaultState_AllLevelsEnabled | 重置后单模块四级别全部输出 | 4 条日志，Debug/Info/Warning/Critical 各 1 条 |
| 2.2 | 默认状态 | testDefaultState_AllModulesEnabled | 重置后所有模块四级别全部输出 | 每个模块各 4 条，共 16 条 |
| 3.1 | setModuleLevel() | testSetModuleLevel_DebugLevel_AllPass | 设为 Debug 级别，全部通过 | 4 条 |
| 3.2 | setModuleLevel() | testSetModuleLevel_InfoLevel_FilterDebug | 设为 Info 级别，过滤 Debug | 3 条，无 Debug |
| 3.3 | setModuleLevel() | testSetModuleLevel_WarningLevel_FilterDebugAndInfo | 设为 Warning 级别，过滤 Debug+Info | 2 条，仅 Warning+Critical |
| 3.4 | setModuleLevel() | testSetModuleLevel_CriticalLevel_OnlyCritical | 设为 Critical 级别，仅 Critical 通过 | 1 条 |
| 3.5 | setModuleLevel() | testSetModuleLevel_OffLevel_NonePass | 设为 Off，全部过滤 | 0 条 |
| 4.1 | 模块隔离性 | testModuleIsolation_DisableOneModuleOthersUnaffected | 禁用 database，network/fileio 不受影响 | database=0, network=4, fileio=4 |
| 4.2 | 模块隔离性 | testModuleIsolation_DifferentLevelsPerModule | 不同模块设不同级别互不干扰 | network=2, database=4, fileio=3 |
| 5.1 | disable/enable | testDisableModule_AllLogsSuppressed | disableModule 后全部静默 | 0 条 |
| 5.2 | disable/enable | testEnableModule_AllLogsRestored | disable 后 enable 恢复全部输出 | 4 条 |
| 5.3 | disable/enable | testDisableEnable_Cycle | 连续 3 轮 disable→enable 切换 | 每轮 disable=0, enable=4 |
| 6.1 | setGlobalLevel() | testSetGlobalLevel_Info_AllModulesFilterDebug | 全局 Info，所有模块过滤 Debug | 每模块 3 条，无 Debug |
| 6.2 | setGlobalLevel() | testSetGlobalLevel_Off_AllModulesSilenced | 全局 Off，所有模块静默 | 每模块 0 条 |
| 6.3 | setGlobalLevel() | testSetGlobalLevel_Debug_AllModulesFullOutput | 全局 Off→Debug 恢复 | 4 条 |
| 7.1 | 全局+例外 | testGlobalWithException_GlobalInfoButNetworkDebug | 全局 Info + 网络单独 Debug | network=4(含Debug), database=3(无Debug) |
| 7.2 | 全局+例外 | testGlobalWithException_GlobalWarningButFileIOInfo | 全局 Warning + 文件单独 Info | network=2, fileio=3(含Info) |
| 8.1 | 批量配置 | testSetModuleLevels_BatchConfig | 三模块不同级别批量设置 | network=2, database=0, fileio=3, ui=4 |
| 8.2 | 批量配置 | testSetModuleLevels_EmptyMap_NoChange | 空 Map 不影响现有配置 | 4 条 |
| 8.3 | 批量配置 | testSetModuleLevels_SingleEntry | 单条目批量设置 | ui=1(仅Critical) |
| 9.1 | applyRawRules() | testApplyRawRules_DisableAllDebug | 通配符关闭所有 Debug | 无 Debug，有 Info |
| 9.2 | applyRawRules() | testApplyRawRules_ComplexRules | 通配符+具体规则组合 | network=4, database=2 |
| 9.3 | applyRawRules() | testApplyRawRules_EmptyString_ResetsToDefault | 空字符串恢复默认 | 4 条 |
| 10.1 | 消息内容 | testLogMessageContent_CorrectText | 日志消息文本正确传递 | 包含 "hello-world-12345" |
| 10.2 | 消息内容 | testLogMessageContent_CategoryTag | 日志分类标签正确 | category="app.fileio", type=Warning |
| 11.1 | 级别覆盖 | testLevelOverride_LaterRuleWins | Off→Warning，后设置生效 | 2 条 |
| 11.2 | 级别覆盖 | testLevelOverride_MultipleOverrides | 连续三次覆盖，最后一次生效 | 3 条(Info级别) |
| 12.1 | 边界场景 | testEdgeCase_SetLevelOnUnknownCategory | 不存在的分类名不崩溃 | 已有模块不受影响 |
| 12.2 | 边界场景 | testEdgeCase_RapidToggle | 100 次快速 disable/enable 切换 | 不崩溃，最终状态正确(4条) |
| 12.3 | 边界场景 | testEdgeCase_EmptyCategory | 空分类名不崩溃 | 已有模块不受影响 |
| 13.1 | 全局Off+恢复 | testGlobalOffThenRestoreSingleModule | 全局 Off 后仅恢复网络模块 | network=4, database=0 |
| 14.1 | 数据驱动 | testPreciseFiltering(Debug) | Debug 级别精确过滤验证 | D=1,I=1,W=1,C=1 |
| 14.2 | 数据驱动 | testPreciseFiltering(Info) | Info 级别精确过滤验证 | D=0,I=1,W=1,C=1 |
| 14.3 | 数据驱动 | testPreciseFiltering(Warning) | Warning 级别精确过滤验证 | D=0,I=0,W=1,C=1 |
| 14.4 | 数据驱动 | testPreciseFiltering(Critical) | Critical 级别精确过滤验证 | D=0,I=0,W=0,C=1 |
| 14.5 | 数据驱动 | testPreciseFiltering(Off) | Off 级别精确过滤验证 | D=0,I=0,W=0,C=0 |
| 15.1 | 规则覆盖 | testApplyRawRules_OverridesPreviousSetModuleLevel | applyRawRules 完全替换之前的 setModuleLevel | 从 0 条恢复到 4 条 |
| 16.1 | 多模块同时 | testMultiModuleSimultaneous | 四模块同时设置不同策略 | net=0, db=2, file=1, ui=4 |

### 测试覆盖维度

| 维度 | 覆盖情况 |
|------|---------|
| API 覆盖 | setModuleLevel / setModuleLevels / disableModule / enableModule / setGlobalLevel / applyRawRules / levelName — 全部覆盖 |
| 级别覆盖 | Debug / Info / Warning / Critical / Off — 全部 5 个级别逐一验证 |
| 模块覆盖 | app.network / app.database / app.fileio / app.ui — 全部 4 个模块 |
| 隔离性 | 单模块设置不影响其他模块 |
| 状态切换 | disable→enable 循环、多次覆盖、全局→单模块例外 |
| 边界场景 | 空分类名、不存在的分类、空 Map、空规则字符串、100 次快速切换 |
| 消息内容 | 日志文本正确传递、分类标签正确 |
| 数据驱动 | 5 个级别的精确过滤矩阵验证 |

## 注意事项

### 1. 编译依赖

- Qt6 Core 模块：`find_package(Qt6 REQUIRED COMPONENTS Core)`
- Qt6 Test 模块（仅测试需要）：`find_package(Qt6 REQUIRED COMPONENTS Core Test)`
- CMake 3.16+
- C++17 标准（`CMAKE_CXX_STANDARD 17`）
- 编译器：GCC 9+、Clang 10+、MSVC 2019+ 均可
- 如使用 Docker 构建，无需本地安装 Qt 环境，`Dockerfile` 中已包含完整依赖安装

### 2. 线程安全

本日志系统在三个层面保证线程安全：

- `LogHelper` 配置接口：所有公共方法（`setModuleLevel()`、`setModuleLevels()`、`disableModule()`、`enableModule()`、`setGlobalLevel()`、`applyRawRules()`）通过 `QMutex` + `QMutexLocker` 保护内部 `QMap<QString, Level>` 状态。多线程环境下可安全并发调用配置接口，不会产生数据竞争。
- `LogFormatter` 输出操作：自定义消息处理器中使用独立的 `QMutex` 保护 `fprintf` 输出，确保多线程环境下日志行不会交错混乱。
- `QLoggingCategory::setFilterRules()`：Qt 官方文档明确说明此函数本身是线程安全的，内部有自己的同步机制。

注意：`qInstallMessageHandler()` 是全局函数，设置的消息处理器是进程级别的（非线程级别）。如果多个线程同时调用 `qInstallMessageHandler()` 设置不同的处理器，会产生竞争。因此建议在程序启动时（单线程阶段）调用一次 `LogFormatter::install()`，之后不再更换处理器。

### 3. 事件循环（QEventLoop）

本日志系统的核心功能（日志分类、过滤规则、格式化输出）**不依赖 Qt 事件循环**，具体说明如下：

- `QLoggingCategory::setFilterRules()` 和 `qCDebug()`/`qCInfo()` 等宏是同步调用，不需要事件循环驱动，可在 `QCoreApplication` 创建前后使用。
- `qInstallMessageHandler()` 是全局注册函数，注册后立即生效，不依赖事件循环。但建议在 `QCoreApplication` 构造之后尽早调用 `LogFormatter::install()`，原因是：
  - `QCoreApplication` 构造时会初始化 Qt 内部的日志基础设施（如默认消息处理器）
  - 在 `QCoreApplication` 之前安装的处理器可能被 Qt 内部初始化覆盖
  - `LogFormatter::messageHandler()` 中使用了 `QDateTime::currentDateTime()`，该函数在 `QCoreApplication` 初始化后才能正确处理时区
- `QtFatalMsg` 的特殊行为：当日志级别为 `QtFatalMsg` 时，`LogFormatter::messageHandler()` 在输出日志后会调用 `abort()` 立即终止进程。这是 Qt 的标准行为——Fatal 消息表示不可恢复的错误，`abort()` 会绕过事件循环、绕过析构函数、绕过 `atexit` 处理器，直接终止进程并生成 core dump（如果系统允许）。
- 如果项目中使用了 `QTimer`、信号槽等依赖事件循环的机制来触发日志输出，需要确保 `QCoreApplication::exec()` 已经启动事件循环。但日志系统本身不要求这一点。

### 4. 规则管理与内存

- `LogHelper` 使用 `QMap<QString, Level>` 维护模块级别映射，每次变更时通过 `rebuildAndApplyRules()` 重新生成完整规则字符串，不会出现规则无限堆积。
- 适合长运行服务（如后台守护进程、嵌入式设备）中频繁动态调整日志级别的场景，内存占用恒定。
- `applyRawRules()` 会清空内部映射表，完全替换为传入的原始规则，不会与之前的 `setModuleLevel()` 调用产生冲突。

### 5. 规则优先级

- 代码中 `setFilterRules()` 的优先级 **高于** 环境变量 `QT_LOGGING_RULES`
- 同一规则集中，后出现的规则覆盖先出现的规则
- 具体分类规则（如 `app.network.debug`）优先于通配符规则（如 `app.*.debug`）
- 环境变量中多条规则用分号 `;` 分隔（不是换行符）

### 6. 性能影响

- 被过滤的日志在编译优化后几乎零开销。Qt 内部通过 `QLoggingCategory::isDebugEnabled()` 等方法在宏展开时短路求值，如果该级别被禁用，参数表达式不会被求值，字符串不会被构造。
- `rebuildAndApplyRules()` 每次调用会遍历 `QMap` 生成规则字符串，但模块数量通常很少（个位数到几十个），性能开销可忽略。

### 7. 新增模块

添加新业务模块只需三步：

1. 在 `log_categories.h` 中声明：`Q_DECLARE_LOGGING_CATEGORY(logNewModule)`
2. 在 `log_categories.cpp` 中定义：`Q_LOGGING_CATEGORY(logNewModule, "app.newmodule", QtDebugMsg)`
3. 在业务代码中使用：`qCDebug(logNewModule) << "message"`

新模块自动受 `LogHelper` 管理，无需修改 `LogHelper` 或 `LogFormatter` 的任何代码。通配符规则（如 `app.*.debug=false`）会自动匹配新模块。

### 8. 模块集成

将 `src/` 目录下的所有文件拷贝到目标项目即可集成，包含完整的日志分类、级别控制、格式化输出功能。集成步骤：

1. 拷贝 `src/` 下的 `log_categories.h/cpp`、`log_helper.h/cpp`、`log_formatter.h/cpp` 到目标项目
2. 在 CMakeLists.txt 中添加这些源文件并链接 `Qt6::Core`
3. 在 `main()` 中调用 `LogFormatter::install()` 安装格式化处理器
4. 在业务代码中使用 `qCDebug(logXxx) << "message"` 输出日志

无需从 `main.cpp` 中手动复制任何代码。

### 9. Qt5 兼容

如需兼容 Qt5，将 CMakeLists.txt 中的 `Qt6` 改为 `Qt5` 即可，所有 API（`QLoggingCategory`、`qCDebug`、`qInstallMessageHandler` 等）在 Qt5 和 Qt6 中完全一致。
