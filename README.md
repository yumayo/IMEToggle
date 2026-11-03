# IMEToggle

英語配列キーボードで、Macの「英数」「かな」のようにIMEを切り替えるWindows用タスクトレイ常駐アプリです。

| キー | 動作 |
| --- | --- |
| F13 | IME OFF（英数入力） |
| F14 | IME ON（日本語入力） |

変換・無変換キーは不要です。キーボード側でスペースキーの左右などにF13・F14を割り当てておけば、そのキーで切り替えられます。キーの割り当てはこのバージョンでは固定です。F13を何度押してもOFF、F14を何度押してもONになります。

C++17とWin32 APIで実装しています。C/C++ライブラリを静的リンクし、Windows標準のDLLだけを使用します。配布するファイルは `IMEToggle.exe` ひとつです。

## 動作環境

- Windows 10 / 11（x64）
- 日本語のMicrosoft IMEがインストールされ、選択されていること
- F13・F14を送信できるキーボード、またはキーボードの割り当てツール

入力言語は日本語のMicrosoft IMEにしたまま使用してください。英語**配列**のキーボードで使用でき、Windowsの入力言語を英語へ変更する必要はありません。

## Dockerで開発・ビルドする

ホストにはDockerとDocker Compose v2が必要です。WindowsではDocker DesktopなどでLinuxコンテナを利用できるようにしてください。C++コンパイラやCMakeなどの開発ツールはコンテナに含まれます。Dockerは開発用であり、完成したexeの実行には不要です。

リポジトリのルートで実行します。

```sh
docker compose up -d --build dev
docker compose exec -T dev sh /usr/local/bin/imetoggle-build
```

Windowsでは `.\build.cmd` でも同じ処理を実行できます。

初回のイメージ作成にはDebianイメージと開発ツールのダウンロードが必要です。起動後の開発コンテナは外部通信を行わない内部ネットワークに配置します。

- `compose.yml`：開発コンテナ、ソース・成果物・ビルドキャッシュの共有を設定します。
- `docker/Dockerfile`：Debian、Linux用G++、Windows向けMinGW-w64、CMake、Ninjaを用意します。
- `docker/build.sh`：Linux上でテストし、Windows用exeをクロスコンパイルしてDLL依存を検査します。
- `CMakeLists.txt`：C++のビルドとテストを定義します。

ソースはコンテナの `/workspace` に読み取り専用でマウントします。ビルドキャッシュは `/build` の名前付きボリューム、成果物は `/output` に出力し、ホストの **`out/docker/IMEToggle.exe`** から取り出せます。ソースはホスト側のエディターやAIコンテナで編集してください。

ビルドでは次を順に実行します。

1. Linux用コンパイラでキー処理のテストをビルドし、CTestで9件のテストを実行。
2. MinGW-w64でWindows x64向けのアプリをビルドし、C/C++ライブラリを静的リンク。
3. `objdump` でexeのインポートを調べ、Windows標準以外のDLLが含まれる場合は失敗。
4. exeを `out/docker` へ出力。

停止する場合は `docker compose down` を実行します。ビルドキャッシュのボリュームは保持します。

## AIコンテナから開発コンテナを使う

Dockerイメージのビルドと開発コンテナの起動はホスト側で実行してください。AIコンテナのDocker Socket Proxyはこれらの操作に対応していません。

既存の `.aicontainer` にある `docker-proxy-name` と `docker-proxy-containers=imetoggle-dev-1` を使用します。設定変更後はAIコンテナを再作成してください。起動済みのAIコンテナには反映されません。

`cmake` と `ctest` の既存の実行許可で、次のように個別実行できます。共有ソースは `/workspace` にあるため、`-c /workspace` を指定します。

```sh
dp -p imetoggle-dev-1 -c /workspace cmake -S /workspace -B /build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
dp -p imetoggle-dev-1 -c /workspace cmake --build /build/native --parallel
dp -p imetoggle-dev-1 -c /workspace ctest --test-dir /build/native --output-on-failure
dp -p imetoggle-dev-1 -c /workspace cmake -S /workspace -B /build/windows-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/workspace/cmake/mingw-x64.cmake -DBUILD_TESTING=OFF
dp -p imetoggle-dev-1 -c /workspace cmake --build /build/windows-x64 --parallel
dp -p imetoggle-dev-1 -c /workspace cmake --install /build/windows-x64 --prefix /output
```

一括スクリプトを `dp` から直接実行する場合は、ホスト側の `.aicontainer` に `docker-proxy-allow=sh /usr/local/bin/imetoggle-build` を追加してAIコンテナを再作成します。このファイルがAIコンテナ内で読み取り専用の場合はホスト側で編集してください。

## Windows上で使用する

`out/docker/IMEToggle.exe` を好きなフォルダーにコピーして実行します。追加の設定ファイルは不要です。

タスクトレイに緑色の「あ」が表示されます。隠れている場合はタスクトレイの「^」を開いてください。メモ帳などの入力欄にカーソルを置き、F13で英数、F14で日本語入力になることを確認します。

アイコンを右クリックして操作します。

- **一時停止**：F13・F14の変換を停止します。もう一度選ぶと再開します。
- **ログイン時に起動**：現在のユーザーのログイン時に起動します。初期状態では登録しません。
- **使い方**：キーの説明を表示します。アイコンのダブルクリックでも表示できます。
- **終了**：フックを解除してアプリを終了します。

アイコンの緑色はアプリが有効、灰色は一時停止を示します。現在のIMEのON/OFF表示ではありません。Explorerの再起動時はトレイアイコンを再登録します。

自動起動の設定は `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` の `IMEToggle` に保存します。フォルダーを移動する場合は、移動前に自動起動を解除して、移動後に登録し直してください。アンインストールは自動起動を解除し、終了してからファイルを削除します。

## 動作の仕組みと注意点

`WH_KEYBOARD_LL` で修飾キーなしのF13・F14を受け取り、WindowsのIME専用キー `VK_IME_OFF`（0x1A）・`VK_IME_ON`（0x16）を `SendInput` で送信します。文字やIMEの状態を収集・保存する機能、外部への通信はありません。

- Ctrl・Shift・Alt・Winと組み合わせたF13・F14は元のキーのまま通します。
- 長押しでは、1回の押下につき1組のIMEキーの押下・解放を送信します。
- キーボード割り当てツールが生成したF13・F14も処理対象です。同じキーを複数のツールで重複して割り当てると競合する場合があります。
- IMEをONにする操作で、ひらがな・カタカナなどの変換モードの固定は行いません。
- 管理者権限で動くアプリにキーを送れない場合は、IMEToggleも同じ権限で起動してください。UACの確認画面には対応しません。
- キーを送信できても、入力先がIMEキーを受け付けたかどうかまでは判定しません。

## CMakeを直接使う場合

Docker内の一括ビルドを使わず、個別に実行することもできます。

```sh
# Linuxでキー処理をテスト
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native
ctest --test-dir build/native --output-on-failure

# MinGW-w64でWindows用exeをビルド
cmake -S . -B build/windows-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x64.cmake -DBUILD_TESTING=OFF
cmake --build build/windows-x64
cmake --install build/windows-x64 --prefix out/docker
```

Windows上にVisual StudioのC++ビルドツールとCMakeがある場合もビルドできます。MSVCでは `/MT`（Debugは `/MTd`）でランタイムを静的リンクします。

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release
ctest --test-dir build/windows -C Release --output-on-failure
cmake --install build/windows --config Release --prefix out/native
```

実際のMicrosoft IMEの挙動はWindows上で [手動確認手順](docs/manual-test.md) に従って確認してください。Linuxコンテナのテストではキー処理を確認でき、WindowsのGUIやIMEの動作は確認できません。
