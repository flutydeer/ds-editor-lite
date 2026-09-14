# 远程设备调试（日志回传 + 一键部署）

触摸与笔输入的调试必须在真机上进行，但开发机没有触摸屏。在加入这套工具之前，
流程是"改代码 → 通过共享目录把构建产物拷到平板 → 手动启动 → 找到日志文件再拷回来"，
每一步都要人工介入。

本方案把其中可以自动化的两步彻底自动化：

1. **日志实时回传**：平板上的编辑器把每条日志通过 UDP 发到开发机，实时打印，
   不再需要收集日志文件。
2. **一键部署 + 重启**：一条命令完成构建、传输、结束旧进程、带参启动。

> **输入操作无法远程化。** 触摸/笔的事件类型正是要观测的对象，而远程桌面（RDP）
> 会把触摸点重写成鼠标事件，任何远程注入同理。所以调试时人必须在设备前用真手指/笔
> 操作；这套工具解决的是"部署"和"看日志"。

## 功能概览

| 能力 | 实现 | 位置 |
| --- | --- | --- |
| 编辑器日志 UDP 回传 | `--log-udp host:port` 启动参数 | `src/libs/Support/Log.cpp`、`src/app/Bootstrap/StartupArguments.cpp` |
| 开发机端日志监听 | PowerShell UDP 监听并打印 | `scripts/dev/listen-remote-log.ps1` |
| 一键部署 + 重启 | 构建 install → scp → taskkill → 计划任务启动 | `scripts/dev/deploy-to-device.ps1` |
| 设备端启动脚本 | 结束旧实例并带参启动 | `scripts/dev/device/start-editor.cmd` |

远程断点调试**不在**本方案范围内（需要设备端运行 VS Remote Debugger 并匹配工具集）。

## 部署什么：install 树

部署的不是构建目录，而是 **CMake install 树**。默认 preset 是 `package-dml-portable`
（RelWithDebInfo），它打开了 `LITE_INSTALL`，因此 install 会把 Qt 运行时、`plugins/`、
`Resources/`、`configs/` 一并铺好，产出一个自包含的目录：

```
dist/stage/portable/bin/
```

这个目录**已经包含 MSVC 运行库**（它们和其他依赖一样被部署进去），所以设备上
**不需要预装 VC++ Redistributable**，也不需要装 Visual Studio。

两个可选 preset：

| preset | 配置 | install 树 |
| --- | --- | --- |
| `package-dml-portable`（默认） | RelWithDebInfo + DirectML | `dist/stage/portable/bin` |
| `package-dml-release` | Release + DirectML | `dist/stage/dml/bin` |

`debug` / `release` preset 不设 `LITE_INSTALL`，它们的 `install/bin` 只会得到几个孤立
的支撑 DLL，无法运行——脚本会直接拒绝并提示（见 `$PresetBinDirs`）。

## 设备端一次性准备

在用作调试机的 Windows 平板（Surface）上，以**管理员** PowerShell 执行：

```powershell
# 1. 安装 OpenSSH 服务端
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0

# 2. 启动并设为自启
Set-Service -Name sshd -StartupType Automatic
Start-Service sshd

# 3. 放行 22 端口（安装时通常已自动添加规则）
New-NetFirewallRule -Name sshd -DisplayName "OpenSSH Server (sshd)" `
    -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22
```

然后把该网络连接设为**专用网络**（公共网络配置下 sshd 默认不接受入站）：

```powershell
Get-NetConnectionProfile      # 先看清 InterfaceAlias 的真实名字
Set-NetConnectionProfile -InterfaceAlias "<上一步显示的别名>" -NetworkCategory Private
```

> 接口别名**不一定**是 `"Wi-Fi"`：中文系统或某些驱动下会是 `WLAN`、`以太网` 等。
> 别名写错会得到 `ObjectNotFound`。另外，若 `NetworkCategory` 已经是 `Private`，
> 这一步本来就不需要执行。

### 配置免密登录（强烈建议）

否则每次 `ssh` / `scp` 调用都会提示输入密码，脚本会被反复打断。在**开发机**上确认已有密钥：

```powershell
ssh-keygen -t ed25519        # 若还没有密钥；也可以继续用已有的 id_rsa
Get-Content $env:USERPROFILE\.ssh\*.pub
```

然后在**设备**上以**管理员身份**打开 PowerShell，执行下面的脚本，把上一步输出的公钥填进
`$pubKey`。这里有两个容易踩的坑：

- **管理员账户的密钥文件不是用户目录下的 `authorized_keys`**，而是
  `C:\ProgramData\ssh\administrators_authorized_keys`。写错位置 sshd 不会报错，只是
  静默地永远不认这个密钥。
- 判断账户类型**不能**用 `IsInRole(Administrator)`：在未提升的 PowerShell 里，UAC 会
  把管理员角色从 token 中过滤掉，判断会得到错误结果。下面改用 Administrators 组的 SID，
  它在任何语言的系统上都是稳定的，也不受提升状态影响。

```powershell
$pubKey = 'ssh-rsa AAAA... replace-with-your-public-key ...'

# The Administrators group SID is locale-independent and visible even in an
# unelevated session, unlike the role checked by IsInRole().
$isAdminAccount = (whoami /groups) -match 'S-1-5-32-544'

if ($isAdminAccount) {
    $keyFile = Join-Path $env:ProgramData 'ssh\administrators_authorized_keys'
} else {
    $keyDir = Join-Path $env:USERPROFILE '.ssh'
    if (-not (Test-Path $keyDir)) { New-Item -ItemType Directory -Path $keyDir | Out-Null }
    $keyFile = Join-Path $keyDir 'authorized_keys'
}

if (-not (Test-Path $keyFile)) { New-Item -ItemType File -Path $keyFile -Force | Out-Null }
Add-Content -Path $keyFile -Value $pubKey

# sshd refuses a key file that other accounts can write to.
if ($isAdminAccount) {
    icacls $keyFile /inheritance:r /grant 'Administrators:F' /grant 'SYSTEM:F' | Out-Null
} else {
    $userName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    icacls $keyFile /inheritance:r /grant "${userName}:F" | Out-Null
}

whoami
```

`whoami` 的输出（形如 `surface\dev`）就是 `Target` 参数里的用户名。

在**开发机**上验证：

```powershell
ssh <user>@<device-ip> echo ok
```

> 设备上必须存在一个可用于登录的账户，`Target` 参数即 `<user>@<device-ip>`。

## 日常使用

### 一条命令完成全部

在开发机仓库根目录：

```powershell
.\scripts\dev\deploy-to-device.ps1 dev@192.168.1.42
```

它会依次：

1. 用 preset `package-dml-portable` 配置并构建 `install` 目标，产出 `dist/stage/portable/bin`；
2. 把该目录镜像到本地暂存目录，剔除调试符号（`.pdb`/`.ilk`）和测试程序——设备用不到，
   而且它们占了相当一部分体积；
3. 通过 `scp` 把暂存内容同步到设备的 `C:\Data\Lite`；
4. 结束设备上正在运行的 `DsEditorLite.exe`；
5. 通过**计划任务**启动编辑器，附带 `--log-udp <开发机IP>:9999`；
6. 在本机开始接收并打印日志，直到 Ctrl+C。

### 为什么用计划任务启动

这是整套流程里唯一不显然的一步，也是"平板上死活不出现窗口"的根因。

sshd 是**服务**，它派生出来的进程运行在 **Session 0**（会话名 `Services`）。Session 0 是
隔离的、没有交互桌面的，所以从 SSH 直接启动的编辑器进程虽然能跑、能写日志，但
**永远不可能在平板上显示窗口**。

解决办法是 `schtasks /create ... /it` + `schtasks /run`：`/it` 让任务在"交互式用户"
（会话名 `Console`，通常是会话 1）里运行，窗口才会真正出现在桌面上。

验证进程落在哪个会话：

```powershell
ssh dev@192.168.1.42 'tasklist /FI "IMAGENAME eq DsEditorLite.exe" /FO LIST'
```

`会话名` 应该是 `Console`、`会话#` 是 `1`；如果看到 `Services` / `0`，说明窗口不会出现。

> 附带一个坑：`schtasks /tr` 只接受一个命令字符串，而引号在
> PowerShell → `ssh.exe` → 远程 `cmd` 的传递过程中会丢失，导致 `--log-udp` 被当成
> schtasks 自己的选项而报错。所以脚本会生成一个**不带参数**的包装脚本 `run-editor.cmd`，
> 把选项写死在文件里，任何一层都不需要处理引号。

### 快速迭代

依赖（Qt / vcpkg 的 DLL、插件树）在两次代码修改之间几乎不变，因此日常迭代只传主程序即可：

```powershell
.\scripts\dev\deploy-to-device.ps1 dev@192.168.1.42 -ExeOnly
```

`-ExeOnly` 只传 install 树里的 `DsEditorLite.exe`，跳过整个暂存流程。配上 `-SkipBuild`
复用已有构建产物，日常循环就更短：

```powershell
.\scripts\dev\deploy-to-device.ps1 dev@192.168.1.42 -ExeOnly -SkipBuild
```

**何时需要完整部署**：改动了 vcpkg 依赖、Qt 模块、插件、`Resources/`、CMake 配置，
或首次部署到新设备时。改动资源文件（`Resources/`、`configs/`）尤其要注意——`-ExeOnly`
不会带上它们。

### 只想看日志

编辑器已经在设备上运行时（比如你手动启动的），可以单独起监听：

```powershell
.\scripts\dev\listen-remote-log.ps1 -Port 9999
```

然后在**设备上**带参数启动编辑器（不要通过 SSH 启动，否则没有窗口）：

```
C:\Data\Lite\start-editor.cmd --log-udp <开发机IP>:9999
```

### 常用参数

| 参数 | 说明 |
| --- | --- |
| `-Preset` | CMake preset，默认 `package-dml-portable`；另一个可用值是 `package-dml-release`。 |
| `-RemoteDir` | 设备上的部署目录，默认 `C:\Data\Lite`（避免空格）。 |
| `-LogPort` | 日志 UDP 端口，默认 `9999`，两台机器的该端口都要空闲。 |
| `-LogHost` | 设备回传的目标地址，默认自动探测通向设备的本机地址。 |
| `-BinDir` | 本地运行时目录，默认按 preset 推导为 install 树的 `bin/`。 |
| `-StageDir` | 本地暂存目录，默认 install 树同级的 `deploy-stage`。 |
| `-TaskName` | 设备上用于启动的计划任务名，默认 `DsEditorLiteDeploy`。 |
| `-ExeOnly` | 只传可执行文件，用于快速迭代。 |
| `-SkipBuild` | 不构建，直接部署现有产物。 |
| `-NoStart` | 只部署，不启动。 |
| `-NoStop` | 部署前不结束设备上的旧进程。 |
| `-NoListen` | 启动后不进入日志监听。 |
| `-ExtraArgs` | 传给编辑器的额外启动参数，如 `--mcp`。 |
| `-SshKey` | 指定私钥路径，传给 `ssh`/`scp` 的 `-i`。 |
| `-DryRun` | 只打印将执行的命令，不做任何实际操作。建议首次连接设备时用它检查。 |

## 日志回传的工作原理

编辑器侧的改动很小：`Log` 增加了一个 UDP 镜像出口。

- 启动参数 `--log-udp host:port` 指定目标，`host` 可以是 IP 字面量或主机名
  （主机名在启动时解析一次），IPv6 写 `[::1]:9999`。
- 每条日志在**调用线程内同步发送**，所以进程崩溃前最后几条也会发出去——这正是调试
  崩溃场景时最需要的。
- 发送是 fire-and-forget 的 UDP：设备不可达或开发机没在监听都不会阻塞、不会影响编辑器。
  代价是极端情况下可能丢包，日志文件仍是完整基准。
- 日志文件照常写在设备上 `QStandardPaths::AppDataLocation`（Windows 上即
  `%APPDATA%\<产品名>\Logs`）下，UDP 只是并行的一份副本。
- 报文是 UTF-8。若监听端显示乱码，先确认终端本身的编码，不要急着怀疑发送端——
  用 `xxd` 看一眼落盘字节即可判断。

### 注意：单实例机制

编辑器是单实例的。如果设备上已经有一个实例在运行，新启动的进程会把请求转发给它然后
退出——**日志回传不会生效**，因为日志出口属于已经在跑的那个进程。启动脚本会先结束旧
进程，所以脚本路径没有这个问题；如果手工启动，记得先关掉设备上的旧窗口。

## 故障排查

**`ssh` 连不上**

先确认服务的运行状态与网络类别：

```powershell
# 在设备上
Get-Service sshd
Get-NetConnectionProfile        # NetworkCategory 应为 Private
```

**每次连接仍然要求输入密码**

几乎都是公钥写到了错误的位置。用 `whoami /groups` 确认登录账户是否属于 Administrators
（SID `S-1-5-32-544`）：如果是，sshd **只读**
`C:\ProgramData\ssh\administrators_authorized_keys`，用户目录下的 `authorized_keys`
会被完全忽略，而且不会有任何报错。

其次确认密钥文件的 ACL 没有把写权限开放给其他账户——sshd 会因此直接拒绝整个文件。

**`scp` 报路径错误**

`-RemoteDir` 里不要有空格，脚本内部的 `mkdir` 与远程命令行对引号的处理很脆弱。

**部署成功，但平板上不出现窗口**

按顺序排查：

1. 用上面那条 `tasklist /FO LIST` 命令看进程落在哪个会话。`Services` / `#0` 说明启动
   绕过了计划任务（比如误用了 `Win32_Process::Create` 或直接 SSH 启动），窗口必然不出现。
2. 确认是在**平板上已登录**的用户会话里运行。`/it` 任务需要有一个已登录的交互式会话。
3. 进程是否还活着？若已退出，见下一条。

**编辑器启动就退出（退出码 `0xC0000135`）**

`0xC0000135` 是 `STATUS_DLL_NOT_FOUND`，即缺运行库。正常的 install 树自带 MSVC 运行库，
所以先确认部署的是 `package-*` preset 的 install 树，而不是 `debug`/`release` 的残缺
`install/bin`。脚本里的 `Add-CompilerRuntime` 只会在 install 树确实缺少
`vcruntime140.dll` 时补一份（Debug 构建的运行库位于 `debug_nonredist`，不属于可再发行
文件，不要手工分发）。

GUI 子系统的程序不会往标准输出写东西，SSH 里看不到报错。用下面的方式取退出码：

```powershell
ssh dev@192.168.1.42 'schtasks /query /tn DsEditorLiteDeploy'
```

也可以顺势检查日志目录：如果里面只有旧的日志文件、没有本次启动的，说明进程在初始化
日志之前就死了，那就是加载 DLL 阶段的问题。

**日志监听起来了，但没有任何输出**

按顺序排查：

1. 设备上的编辑器是否真的带上了 `--log-udp`？若通过脚本启动，检查启动参数；
   若手工启动，确认参数写对。
2. 开发机 IP 是否写对？`-LogHost` 默认自动探测，多网卡（VPN、虚拟机网卡）时可能选错，
   此时显式传入，如 `-LogHost 192.168.1.10`。
3. 防火墙是否拦了入站 UDP？在开发机上为监听端口加一条入站规则：

   ```powershell
   New-NetFirewallRule -DisplayName "DS Editor log mirror" -Enabled True `
       -Direction Inbound -Protocol UDP -Action Allow -LocalPort 9999
   ```

4. 是否已有编辑器实例在运行，导致新的实例只是转发后退出（见上一节）。

**日志文字变成乱码**

发送端固定发 UTF-8。先确认不是终端/查看工具的显示问题：

```bash
grep -a '某个中文关键字所在的英文片段' 收到的日志文件 | head -1 | xxd | head -4
```

若看到 `e5 88 9d e5 a7 8b ...` 这类三字节序列，说明字节是对的，是显示环节按 GBK 解释了
UTF-8，改终端编码即可。

## 已知限制

- **无法远程操作触摸/笔输入**，必须在设备前手动操作（原因见文首）。
- **无远程断点**：需要单步调试时，仍需在设备上运行 VS Remote Debugger 后从开发机附加。
- **UDP 不保证送达**：极端丢包时以设备上的日志文件为准。
- 日志回传是显式的命令行开关，没有默认开启，也不写入配置；不传参数时行为与从前完全一致。
- **设备算力/显卡是设备的限制，不是这套工具的**：例如 `package-dml-portable` 部署到
  Surface 上时，日志里会出现 `InferEngine: Unable to find GPU device.`，推理引擎因此
  初始化失败。这不影响窗口、布局和输入路径的调试，但会影响合成相关功能。
