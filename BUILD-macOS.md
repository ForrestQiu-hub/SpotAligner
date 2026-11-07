# macOS 构建(只有 VST 3,不含 AU)

1.0.0 只发了 Windows。源码本身没有平台相关代码(没有 `windows.h`、没有文件读写、没有 `__declspec`),
所以 Mac 版就是同一份源码用 clang 再编一遍,产出 `SpotAligner.vst3`。
总线、参数、界面逻辑都不动;Windows 构建也不受影响。

## 一、用 GitHub Actions(不需要 Mac 硬件)

1. 推到 GitHub;
2. Actions → **macOS VST3** → *Run workflow*;
3. 跑完下载 artifact `SpotAligner-macos-vst3.zip`,里面是 `SpotAligner.vst3`。

workflow 做的是:拉 SDK(`env.VST3SDK_TAG`,钉版本)→ Xcode 生成器 → universal(`x86_64` + `arm64`)
→ ad-hoc 签名 → 打包 → 另外单独跑一遍 SDK 自带的 validator(只报告,不挡打包:它说有问题就去日志里看)。

若第一步就红,多半是 SDK 的 tag 名不对,改 `.github/workflows/macos-vst3.yml` 里的 `VST3SDK_TAG`。

## 二、本机 Mac

```bash
git clone --depth 1 --branch v3.8.1_build_84 --recursive \
    https://github.com/steinbergmedia/vst3sdk.git vst3sdk

cmake -G Xcode -B build-mac -S SpotAligner \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DSMTG_CODE_SIGN_IDENTITY_MAC=-

cmake --build build-mac --config Release --target SpotAligner
```

产物:`build-mac/VST3/Release/SpotAligner.vst3`

只编当前机器:`cmake -G Xcode -B build-mac -S SpotAligner` 即可(不带那两个 `CMAKE_OSX_*`)。

## 三、安装

```
/Library/Audio/Plug-Ins/VST3/       所有用户(需管理员)
~/Library/Audio/Plug-Ins/VST3/      仅当前用户
```

macOS 上**没有** `Common Files` 那条路径,那是 Windows 的;VST2 的 `VST/` 目录也不收 `.vst3`。

从 zip 解出来的 bundle 带隔离属性,清掉再扫描:

```bash
xattr -dr com.apple.quarantine /path/to/SpotAligner.vst3
```

宿主必须**支持 VST3**:Logic、GarageBand、Pro Tools 不行(要 AU / AAX);Cubase、Nuendo、Studio One、
REAPER、Bitwig、Live 10.1+、Digital Performer 可以。

## 四、签名状态

ad-hoc 签名(`codesign -s -`),没有 Developer ID,也没有公证。自己拷进去能用;
发给别人会看到"无法验证开发者",需要对方右键打开或清 quarantine 属性。

要在下载后完全无提示,得 Apple Developer ID($99/年)+ `notarytool` 公证 —— 这一步没做。

## 五、这次改了什么

| 文件 | 改动 |
|---|---|
| `SpotAligner/CMakeLists.txt` | `CMAKE_OSX_DEPLOYMENT_TARGET` 10.13 → **11.0**(arm64 不能低于 11.0);纯 Intel 包用 `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 -DCMAKE_OSX_ARCHITECTURES=x86_64` 单独编 |
| `SpotAligner/resource/Info.plist.in` | 新增。Xcode 自己生成的 Info.plist 把 `CFBundlePackageType` 写成 `APPL`(Finder 就会把 `.vst3` 当应用);这份模板写 `BNDL`,并在配置阶段交给 Xcode,所以不需要构建后再改、也不用重新签名 |
| `.github/workflows/macos-vst3.yml` | 新增 |
| `.gitignore` | 加 `vst3sdk/`、`build-mac/` |

源码一行没动。**没做**:AU、AAX、签名证书、公证、Mac 安装器。

## 六、已经验证的 / 还没验证的

**已验证(2026-11-07,run #3,3 分 42 秒,11 个步骤全绿)**

- GitHub Actions 的 `macos-15` 上出包成功;产物挂在预发布 `macos-latest`:
  `https://github.com/ForrestQiu-hub/SpotAligner/releases/download/macos-latest/SpotAligner-macos-vst3.zip`
- 二进制是 **universal**:fat 头两个切片,x86_64 2,088,288 字节 + arm64 2,005,824 字节;
- bundle 结构:`Contents/MacOS/SpotAligner`、`Info.plist`(`CFBundleExecutable=SpotAligner`、
  `com.Forrest.SpotAligner`、`LSMinimumSystemVersion 11.0`、Xcode 16.4 / SDK macosx15.5)、
  `Resources/editor.uidesc`、`moduleinfo.json`、两张 Snapshot 都在;
- 签名:内嵌 CodeDirectory + `_CodeSignature/CodeResources`,CI 里 `codesign --verify --deep --strict` 通过;
- SDK validator 跑过一遍,没有报错。

**还没验证**

- **宿主里真实加载、界面打开、参数存取**:需要有人在 Mac 上过一遍。二进制格式对、签名有效,
  不等于 Cubase / Studio One 里能扫出来。
- **界面字体**:`resource/editor.uidesc` 用了 `Arial`(macOS 有)和 `Noto Sans SC`(macOS 默认没有)→
  标题会回退到系统字体,字号不变。要完全一致就把那行改成 `Arial` 或 `Helvetica`。
- **`CFBundlePackageType` 已由 `APPL` 改成 `BNDL`**:Xcode 生成的 Info.plist 默认写成 APPL,Finder 于是把 `.vst3`
  当成应用程序——双击就会请 Gatekeeper 去启动一个未公证的程序,弹出"Apple 无法验证"。
  现在由 `resource/Info.plist.in` 提供这份文件(`configure_file` 填版本号),在配置阶段交给 Xcode,
  签名自然覆盖它,workflow 里也加了断言(不是 BNDL 就直接红)。
  **2026-11-07 之前构建的包仍然是 APPL,要以新包为准。**
- **`macos-15` 以后也会被下线**(`macos-14` 就是这样被取消的)。届时改 `.github/workflows/macos-vst3.yml`
  里 `runs-on` 一行即可;判断方法:job 的 check-run annotation 会写 `The configuration '...' is not supported`。
