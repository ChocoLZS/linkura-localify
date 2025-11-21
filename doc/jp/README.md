# linkura-localify

# 免責事項

このリポジトリ内のすべてのコードとリソースは、開発者の学習と参考のためのみに提供されています。作者はコードの正確性、完全性、または適用性について一切の保証をいたしません。本コードの使用により生じるいかなる直接的または間接的な結果、損失、または法的責任についても、作者は一切責任を負わず、使用者が全てのリスクを負担するものとします。

[![GitHub stars](https://img.shields.io/github/stars/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/stargazers) [![GitHub forks](https://img.shields.io/github/forks/ChocoLZS/linkura-localify?style=social)](https://github.com/ChocoLZS/linkura-localify/network/members) [![GitHub license](https://img.shields.io/github/license/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub contributors](https://img.shields.io/github/contributors/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues) [![GitHub issues closed](https://img.shields.io/github/issues-closed/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/issues?q=is%3Aissue+is%3Aclosed) [![GitHub pull requests](https://img.shields.io/github/issues-pr/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/pulls) [![GitHub last commit](https://img.shields.io/github/last-commit/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify/commits) 
[![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify) [![GitHub repo size](https://img.shields.io/github/repo-size/ChocoLZS/linkura-localify)](https://github.com/ChocoLZS/linkura-localify)


[**`学園アイドルマスター ローカライゼーションプラグイン`**](https://github.com/chinosk6/gakuen-imas-localify) の全体フレームワークをベースとした二次開発

- リンクラ(リンクラ) ローカライゼーションプラグイン
- もし、より多くの機能を追加するためのアイデアやニーズがある場合は、issue/discussionセクションで自由に議論してください。

# 使用方法

- **プラグインには[JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch)のパッチングフレームワークが統合されており、Android 9以上で正常に動作します。**
- これはXPosedプラグインです。Root化済みユーザーは [LSPosed](https://github.com/LSPosed/LSPosed) を、Root化していないユーザーは [LSPatch](https://github.com/LSPosed/LSPatch) を使用できます。
- Android 15以上のユーザーは、[JingMatrix/LSPosed](https://github.com/JingMatrix/LSPosed) または [JingMatrix/LSPatch](https://github.com/JingMatrix/LSPatch) を使用してください。オリジナル版は更新が停止されているためです。
- エミュレーターの選択については、[エミュレーター参考資料](simulator.md) をご覧ください。
- 本地化データリポジトリは、[linkura-localify-assets](https://github.com/ChocoLZS/linkura-localify-assets) です。

# スター履歴

[![Star History Chart](https://api.star-history.com/svg?repos=ChocoLZS/linkura-localify&type=Date)](https://star-history.com/#ChocoLZS/linkura-localify&Date)

# 開発

## 1. **このリポジトリをクローンする**

```bash
git clone https://github.com/ChocoLZS/linkura-localify.git
```

## 2. **最新バージョンのAndroid Studioをダウンロードしてインストールする**
> バージョンは >= 2024.1.1 である必要があります

a. [公式ドキュメント](https://developer.android.com/studioinstall) に従ってAndroid Studioをダウンロードしてインストールします。

b. Android SDK > SDK Tools で以下を確認します：
- 通常、Android Studioは自動的にSDKの問題を処理します
- `CMake` がチェックされており、バージョンは >= 3.22.1
- `NDK` がチェックされており、バージョンは >= 26.3
- `Android SD Build-Tools` がチェックされており、バージョンは >= 29、推奨は34。

## 3. **Android Studioを使用してこのリポジトリを開く**

1. Android Studioの依存関係のダウンロード
2. Android Studioのインデックス作成
3. 手動でのコンパイル

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