# linkura-localify

# Statement

All code and resources in this repository are for developer learning and reference only. The author makes no warranties regarding the accuracy, completeness, or applicability of the code. Users assume all risks and the author shall not be liable for any direct or indirect consequences, losses, or legal liabilities arising from the use of this code.

[![GitHub stars](https://img.shields.io/github/stars/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/stargazers) [![GitHub forks](https://img.shields.io/github/forks/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/network/members) [![GitHub license](https://img.shields.io/github/license/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub contributors](https://img.shields.io/github/contributors/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues) [![GitHub issues closed](https://img.shields.io/github/issues-closed/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues?q=is%3Aissue+is%3Aclosed) [![GitHub pull requests](https://img.shields.io/github/issues-pr/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/pulls) [![GitHub last commit](https://img.shields.io/github/last-commit/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/commits) 
[![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub repo size](https://img.shields.io/github/repo-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify)


Secondary development based on the overall framework of [**`Gakuen Idolmaster Localization Plugin`**](https://github.com/chinosk6/gakuen-imas-localify)

- Linkura (リンクラ) Localization Plugin
- If you have ideas/needs for adding more features, feel free to discuss them in the issue/discussion section.

# Usage

- **The plugin has integrated the patching framework of [JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch), and it works properly on Android 9 and above.**
- This is an XPosed plugin. Rooted users can use [LSPosed](https://github.com/LSPosed/LSPosed), while non-rooted users can use [LSPatch](https://github.com/LSPosed/LSPatch).
- For Android 15 and above users, please use [JingMatrix/LSPosed](https://github.com/JingMatrix/LSPosed) or [JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch), as the original versions have stopped updating.
- For emulator selection, please refer to [Emulator Reference](simulator.md).
- Localization data repository: [linkura-localify-assets](https://github.com/ChocoLZS/linkura-localify-assets)

# Star History

[![Star History Chart](https://api.star-history.com/svg?repos=ChocoLZS/linkura-localify&type=Date)](https://star-history.com/#ChocoLZS/linkura-localify&Date)

# Development

## 1. **Clone this repository**

```bash
git clone https://github.com/ChocoLZS/linkura-localify.git
```

## 2. **Download and install the latest version of Android Studio**
> Version must be >= 2024.1.1

a. Download and install Android Studio according to the [official documentation](https://developer.android.com/studioinstall)

b. Ensure that in Android SDK > SDK Tools:
- Usually, Android Studio will automatically handle SDK issues for you
- `CMake` is checked and version >= 3.22.1
- `NDK` is checked and version >= 26.3
- `Android SD Build-Tools` is checked and version >= 29, recommended 34.

## 3. **Open this repository using Android Studio**

1. Android Studio dependency download
2. Android Studio indexing
3. Manual compilation

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