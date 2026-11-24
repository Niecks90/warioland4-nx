# WarioLand4Recomp (瓦力欧大陆4 静态重编译)

《瓦力欧大陆4》（Wario Land 4，Game Boy Advance 美版/欧版）适用于 Windows x64 的静态重编译项目。

Game Boy Advance 的 CPU 为 ARM7TDMI。本项目在构建期将卡带 ROM 翻译为原生的 C++ 代码，并编译为标准的 Windows x64 本地程序运行。游戏以原生速度运行，无任何第三方模拟器中间层。GBA BIOS 同样通过静态重编译并在运行时由同一分发调度器作为原生代码执行（在主路径上不打桩、不使用简化 HLE）。

**本项目不包含任何受版权保护的游戏代码、ROM 镜像或 BIOS 固件。** 玩家需提供合法自持的卡带 ROM 与 BIOS 文件；详细校验参数请参阅 [baserom.md](baserom.md)。

---

## 实验性预览版本说明 (Experimental Preview)

```text
Experimental Preview
Full-game compatibility has not been exhaustively tested.
（实验性预览版：全流程游戏兼容性尚未经过穷尽测试）
```

只有在兼容性状态清单（[docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)）中记录的有界测试用例经过了严格验证。后续关卡、Boss 战斗、游戏通关、深层边界反应状态以及长时间游戏会话等尚未穷尽测试。

---

## 游戏支持与文件校验基准

| 项目 | 要求基准 |
| --- | --- |
| 基准 ROM | `Wario Land 4 (USA, Europe).gba` |
| ROM 大小 | 8,388,608 字节 (`0x00800000`) |
| ROM MD5 | `5fe47355a33e3fabec2a1607af88a404` |
| ROM SHA-1 | `b9fe05a8080e124b67bce6a623234ee3b518a2c1` |
| ROM SHA-256 | `d16c7bf6e62bb84049fff1b387108fbd1e6e2cd38ca994ab5310dd9cbf9ba414` |
| GBA BIOS | `bios/gba_bios.bin`，16,384 字节，SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492` |
| 存档类型 | SRAM，32,768 字节 (`SRAM_V112`，卡带实机签名) |

*注：日版（`AWAJ`）由于代码与资源布局不同，不在当前版本支持范围内。任何哈希不匹配的文件都将被启动安全门禁严格拒绝。*

---

## 核心特性

### 1. 现代化原生 GUI 启动器 (`recomp-ui`)
- **双击即玩**：双击 `WarioLand4Recomp.exe` 直接呈现美观的图形启动界面，无控制台黑框干扰。
- **文件选择与身份安全门禁**：内置文件浏览与拖拽选择，实时校验 ROM 和 BIOS 的文件大小及 SHA-1 密码学哈希，拒绝无效文件。
- **输入控制映射**：全面支持键盘键位映射以及 SDL2 GameController 标准手柄（包括 D-Pad、摇杆、ABLR 按键自定义）。
- **画面与音效设置**：支持整数倍画面缩放、窗口化/全屏无缝切换、原生 3:2 (240×160) 画面比例保持以及主音量调节。
- **启动偏好与配置持久化**：支持设置“跳过启动器”直接启动游戏，并在需要时通过快捷方式或 `--launcher` 重新唤起启动器。
- **存档无缝兼容**：保持原生 SRAM 存档格式（`SRAM_V112`，32KB），自动识别 `saves/` 目录与历史存档。

### 2. 玩家零编译器依赖与动态 IWRAM 兼容
- 《瓦力欧大陆4》在运行时会动态向栈内存空间（`0x03007D0C..0x03007D90`）写入 Thumb 返回桩代码。
- 早期版本依赖开发机外部 GCC (`GBARECOMP_HEAL_CXX`) 进行运行时即时编译补丁，对普通玩家部署造成困难。
- 本版本内置 **动态 IWRAM 解释器回退挂钩** (`0x03007D00..0x03007E00`)：
  - 玩家模式下完全关闭运行时 GCC 自愈编译 (`GBARECOMP_SELFHEAL_RECOMPILE=0`)。
  - 遇到栈动态代码时，由内置解释器直接读取当前 IWRAM 真实内存指令安全执行。
  - **普通玩家彻底无需安装 GCC、MSYS2、CMake、Ninja 或 Python**，解压即可运行。

---

## 运行方式与命令行参数

### 1. 玩家图形模式（默认）
直接双击运行发行包中的 `WarioLand4Recomp.exe` 即可进入 `recomp-ui` 图形启动器。

### 2. 命令行与自动化启动选项
```powershell
# 直接指定 ROM 与 BIOS 路径启动（跳过启动器）
.\WarioLand4Recomp.exe --rom "Wario Land 4 (USA, Europe).gba" --bios bios\gba_bios.bin

# 强制重新唤起 GUI 启动器（即便之前已勾选跳过启动器）
.\WarioLand4Recomp.exe --launcher

# 开启开发者诊断模式（启用未命中代码碎片追踪与自愈编译器诊断）
.\WarioLand4Recomp.exe --dev --rom "Wario Land 4 (USA, Europe).gba" --bios bios\gba_bios.bin
```

### 3. 默认键位映射

| GBA 按键 | 键盘默认映射 |
| --- | --- |
| A | `X` |
| B | `Z` |
| L | `C` |
| R | `V` |
| Start | `Enter` |
| Select | `Right Shift` |
| 方向键 (D-pad) | `方向键 (Up/Down/Left/Right)` |
| 全屏切换 | `Alt` + `Enter` |
| 暂停 | `Shift` + `P` |
| 加速 (Turbo) | `Tab` |

---

## 本地构建指南 (适用于开发者)

如果您希望从源码自行重编译与构建：

### 环境要求
1. **CMake** 3.20+ 与 **Ninja**
2. **MinGW-w64 GCC** (建议 GCC 14+)
3. **SDL2 2.32+** 开发库 (`x86_64-w64-mingw32`)
4. **PowerShell 7** (`pwsh`)
5. 合法的 GBA ROM 和 BIOS 镜像

### 构建步骤
```powershell
# 1. 获取并初始化上游框架及打上项目特有补丁
pwsh tools/regeneration/setup-framework.ps1

# 2. 编译重编译器核心 gba_recompile
pwsh tools/regeneration/build-framework.ps1

# 3. 将本地 ROM 和 BIOS 翻译为 C++ (生成至 generated/)
pwsh tools/regeneration/regen.ps1

# 4. 构建游戏宿主可执行程序 (内置 recomp-ui 与动态 IWRAM 挂钩)
pwsh tools/regeneration/build-host.ps1 -Jobs 2

# 5. 执行基础冒烟测试验证
pwsh tools/validation/smoke.ps1
```

---

## 许可协议与免责声明

- 本项目编写的源代码采用 **PolyForm Noncommercial License 1.0.0** 许可（参见 [LICENSE](LICENSE)）。
- 本项目严禁用于任何商业目的。
- 本项目是静态重编译实现，**绝不分发或包含任天堂受版权保护的任何代码、音频、图形资产或 ROM 镜像**。玩家必须拥有正版卡带并自行导出。
- 第三方组件（GBARecomp、recomp-ui、Dear ImGui、SDL2、toml++、MinGW 运行时）保留各自的许可协议，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
