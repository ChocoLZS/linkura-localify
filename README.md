# linkura-localify

# Statement

本仓库内的所有代码与资源仅供开发者学习与参考。作者不对代码的准确性、完整性或适用性作任何保证，使用者因使用本代码而引发的任何直接或间接的后果、损失或法律责任，均与作者无关，由使用者自行承担全部风险。

[中文](doc/zh/README.md) | [日本語](doc/jp/README.md) | [English](doc/en/README.md)

[![GitHub stars](https://img.shields.io/github/stars/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/stargazers) [![GitHub forks](https://img.shields.io/github/forks/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/network/members) [![GitHub license](https://img.shields.io/github/license/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub contributors](https://img.shields.io/github/contributors/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues) [![GitHub issues closed](https://img.shields.io/github/issues-closed/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues?q=is%3Aissue+is%3Aclosed) [![GitHub pull requests](https://img.shields.io/github/issues-pr/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/pulls) [![GitHub last commit](https://img.shields.io/github/last-commit/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/commits) 
[![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub repo size](https://img.shields.io/github/repo-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify)

基于 [**`学园偶像大师 本地化插件`**](https://github.com/chinosk6/gakuen-imas-localify) 的整体框架 二次开发

- 林库拉(リンクラ) 本地化插件
- 如果您有添加更多功能的想法/需求，欢迎在issue/discussion板块中进行讨论。

# Usage

- **插件内部已经集成了[JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch)的修补框架，安卓9+可以正常使用。**
- 这是一个 XPosed 插件，已 Root 用户可以使用 [LSPosed](https://github.com/LSPosed/LSPosed)，未 Root 用户可以使用 [LSPatch](https://github.com/JingMatrix/LSPatch)。
- 安卓 15 及以上的用户，请使用 [JingMatrix/LSPosed](https://github.com/JingMatrix/LSPosed) 或 [JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch)。因为原版已停止更新。
- 关于模拟器的选择，请参考 [模拟器参考](doc/zh/simulator.md)。
- 本地化数据仓库，[linkura-localify-assets](https://github.com/ChocoLZS/linkura-localify-assets)

# Star History

[![Star History Chart](https://api.star-history.com/svg?repos=ChocoLZS/linkura-localify&type=Date)](https://star-history.com/#ChocoLZS/linkura-localify&Date)

# Development

## 1. **clone本仓库**

```bash
git clone https://github.com/ChocoLZS/linkura-localify.git
```

## 2. **下载并安装好最新版Android Studio**
> 版本至少需要 >= 2024.1.1

a. 根据[官方文档](https://developer.android.com/studioinstall)下载并安装android studio

b. 确保Android SDK > SDK Tools 中：
- 通常情况 Android Studio 会自动帮你处理SDK问题
- `CMake` 勾选并且版本 >= 3.22.1
- `NDK` 勾选并且版本 >= 26.3
- `Android SD Build-Tools` 勾选并且版本 >= 29, 推荐34。

## 3. **使用Android Studio 打开此仓库**

1. Android Studio 依赖下载
2. Android Studio 建立索引
3. 手动点击编译


# Special Thanks

- [gkmasToolkit](https://github.com/kishidanatsumi/gkmasToolkit)
- [UmaPyogin-Android](https://github.com/akemimadoka/UmaPyogin-Android)
- [UnityResolve.hpp](https://github.com/issuimo/UnityResolve.hpp)

---

- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper)
- [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp)
- [gakuen-imas-localify](https://github.com/chinosk6/gakuen-imas-localify)
- [inspix-hailstorm](https://github.com/vertesan/inspix-hailstorm)
- You

