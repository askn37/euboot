# euboot : EDBG USB bootloader for AVR-DU series

*Switching document languages* : __日本語__, [English](README.md)

## 機能

- AVR-DU シリーズ専用 USB ブートローダー。
- 標準 USB-HID/CMSIS-DAP/EDBG プロトコルを使用し、AVRDUDE<=8.0 からは `jtag3updi` デバイスとして認識される。
- USB-HID Full-Speed の上限に近い、高速なメモリの読み書き速度。
- フットプリントは 2.5KiB 未満。

### 最近の更新

3.72.50 (26/08/28)

- Firmware Update Mode （ブートローダー活性化）を開始する条件を厳格化
  - 起動選択 SWに外付けの抵抗／キャパシタを追加していた場合、意図しない挙動になる問題に対応。

## 開発の理由

AVR-DU シリーズは、USB 周辺機器を内蔵した唯一の modernAVR 世代だが、以前の同様の製品 (ATMEL 世代) とは異なり、DFU ブートローダーは付属しておらず、ベアメタル チップ フラッシュは常に空だ。

これは ATMEL が USB-IF DFU 標準開発の正規メンバーであったのに対し、Microchip はそうではなかったことが一因だろう。そのため将来的に DFU サポートが提供されることはあまり期待できない。

現在、AVRDUDE には USB-CDC と USB-HID という 2つの代替ブートローダに使えるアプローチがある。

CDC (VCP、VCOM) は多くの実装でよく使われているが、少しの利点と引き換えに多くの欠点がある。
USBプロトコルや低階層は隠蔽され、ストリームはバイト志向なのでリードに手間がかかる。
パケット喪失対策がない場合、不正なデータの侵入を防止できない。
さらに悪いことに新しいチップのメモリを適切にサポートするには AVRDUDE に新しいパッチが必要になる。
新規開発で選択するには魅力がほとんどない。

一方、USB-HID を使用したバルク転送通信は USB の専門知識が必要であるため、あまり使われていない。
しかし CMSIS-DAP および EDBG プロトコルをサポートし、`jtag3updi` も処理できる [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/) が、今や手元にある。これをベースに必要な部分のみを実装し、NVM 制御を追加するのは容易いことだ。
結果は良好で、フットプリント 2.5KiB の `jtag3updi` をサポートする USB ブートローダを作り出す事ができた。
DFU ほど小さくはないが実用上は十分コンパクトだ。
またストリームはブロック志向なので、リードライトともUSBプロトコルのネイティブスピードに比して、ほとんど速度低下が発生しない。

> [!TIP]
> USBディスクリプタや定数テーブルを BOOTROW 領域に移動すれば、フットプリントを 2.0KiB 未満にできることは明白だ。
> だが現在はそのような実装を選択していない。将来的にはビルドオプションを用意して選択可能にする可能性はある。

## ブートローダーファームウェアを作成するために必要なもの

これを行うには、次の環境が必要だ:

#### [MultiX Zinnia Product SDK [modernAVR] @0.3.0+](https://github.com/askn37/multix-zinnia-sdk-modernAVR)

Arduino IDE/CLIボードマネージャで簡単にインストールできるベアメタル開発SDK。AVR-LIBCを使いやすくする各種マクロを備えており、Arduino-APIに近い感覚で低レベルコードを記述できる。AVRDUDE 8.0+も同時にインストールされる。

#### [Arduino-CLI @1.0.3+](https://arduino.github.io/arduino-cli/1.0/installation/)

付属の Makefile を使用してファームウェアをビルドする場合に必要。
これがなくとも modernAVR SDK だけでバイナリファイルを出力できるが、メニュー設定が煩雑になる。

> [!TIP]
> Windows 環境では、`make` をそのまま使用することはできない。WSL などの方法を使用する必要がある。

#### AVR-DU シリーズ用の UPDI 互換プログラマー

これは主に `PICKit4` などである。だが世界で最も入手しやすく安価なのは ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a) だろう。まずこれを入手すれば、AVR64DU32 の完全なスタンドアロン開発環境がこれ一つで揃う。また [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/) をインストールすると、AVRDUDEからは `PICKit4` のような UPDIプログラマーとして使えるようにもできる。詳細は各リンクを参照のこと。

## ブートローダーファームウェアの作成とインストール

modernAVR SDKをインストールし、Arduino-CLIとAVRDUDE 8.0を実行パスに追加し、準備ができたら`euboot`ディレクトリに移動して`make all`を実行する。生成されたファイルは`hex`ディレクトリに保存される。

```sh
cd euboot 
make all
```

> [!TIP]
> `Perl5`実行ファイルがある場合、hexおよびbinファイルには`CRCSCAN`周辺機器で使用するための CRC32 が埋め込まれる。
> これを使用するように FUSEを変更すると、ブートローダー予約領域が改竄された場合、MCUの通常動作を停止することができる。

生成されたファイルをターゲットにアップロードする。この例でのターゲットは "CURIOSITY NANO" だが、これには `pkobn_updi` が組み込まれているため簡単に試す事ができる。

```sh
avrdude -cpkobn_updi -pavr64du32 -e -Uflash:w:hex/euboot_LF2_SF6.hex:i -Ufuses:w:hex/euboot_LF2_SF6.fuse:i
```

> [!TIP]
> `*.fuse` ファイルのアップロードは初めて使用する時だけで良い。

ブートローダーのアップロードが成功すると、ユーザーアプリケーション領域はまだ空っぽなので、自己リセットを繰り返す状態になる。
そこで `SW0(PF6)` を1回押すと、`LED(PF2)` は次のパターンで点滅を開始するだろう。
これはホスト PC との USB 列挙がまだ完了していないことを意味する。

- LED(PF2): 🟠⚫️⚫️⚫️ (USB列挙を待機中)

2本目の USB ケーブルをターゲット USB ポートに接続するか、USBケーブルをデバッガーポートから差し替えると、LED が次の点滅パターンに変わり、EDBG プロトコル通信を開始する準備ができたことを示す。

- LED(PF2): 🟠🟠⚫️⚫️ (スタンバイモード)

準備完了？ では USB ブートローダーが正しく応答するかどうかを確認しよう。`-P` オプションの記述に注意。

```sh
avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -Usib:r:-:r
```

```console
Avrdude version 8.0-20241010 (0b92721a)
Copyright see https://github.com/avrdudes/avrdude/blob/main/AUTHORS

System wide configuration file is /usr/local/etc/avrdude.conf
User configuration file is /Users/user/.avrduderc

Using port            : usb:04d8:0b12
Using programmer      : jtag3updi
AVR part              : AVR64DU32
Programming modes     : SPM, UPDI
Programmer type       : JTAGICE3_UPDI
Description           : Atmel AVR JTAGICE3 in UPDI mode
ICE HW version        : 52
ICE FW version        : 3.72 (rel. 49)
Serial number         : euboot:CMSIS-DAP:EDBG
Vtarget               : 3.30 V
PDI/UPDI clk          : 2560 kHz

Partial Family_ID returned: "AVR "
Silicon revision: 1.3

AVR device initialized and ready to accept instructions
Device signature = 1E 96 21 (AVR64DU32)
Reading sib memory ...
Writing 32 bytes to output file <stdout>
AVR     P:4D:1-3M2 (EDBG.Boot.)
Avrdude done.  Thank you.
```

ファームウェア固有の情報は、`ICE HW version` 以降にリストされる。AVRDUDE にこれらの特定情報を表示する機能がないため、項目名と内容が一致しないのは仕様だ。代わりに未使用の項目を使って表示される。

- __ICE HW version__: 常に 52。これは元々 `4` の文字コードであり、NVM制御器のバージョンを示す。
- __ICE FW version__: USB ブートローダのバージョンと更新番号を示す。
- __Serial number__: USB デバイス製品文字列を示す。USB シリアル番号文字列は使用されておらず、両方とも同じ文字列になっている。
- __Vtarget__: VDD に供給されている現在の動作電圧を示す。
- __PDI/UPDI clk__: これは実際の動作速度ではなく、`FUSE_BOOTSIZE` 設定から計算されたユーザー アプリケーション の プログラム開始アドレスだ。たとえば `2560` は、ユーザー アプリケーション が プログラム開始アドレス `0x0A00` から実行されることを示す。`0` の場合、必要な FUSEは正しく設定されていない。

前述のプログラムアドレスから実行されるように構成されたユーザー アプリケーションは、次のようにスタンバイ状態の USB ブートローダで書き込むことができる。`-D` オプションは推奨。メモリの読み取り/書き込み中に点滅する LED は、次のように変わる:

- LED(PF2): 🟠🟠🟠🟠 (通信中)

```sh
# `USERAPP.ino` must be built with 'build.text_section_start=.text=0xA00'
avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -D -Uflash:w:USERAPP.ino.hex:i
```

同様に `-U` オプションを使用して、`eeprom`、`userrow`、および `bootrow` の書き込みと読み取りを行うことができる。
`fuse(s)` および `lock` は読み取り専用であり、変更できない。

> [!TIP]
> 一旦 `LED(PF2)` が点灯した後に USBケーブルを抜くと、自己リセットが発生してユーザーアプリケーションの実行が開始される。
> ブートローダー自体にはタイムアウトがないため、スケッチ書き込みを試すか、USBケーブルを抜くのがブートローダーの正規の停止方法となる。

## USB ブートローダーの有効化

USB ブートローダーの動作を活性化するにするには、`SW0 (PF6)` を押したまま AVR-DU の電源を入れなければならない。
これに成功すると、LED は スタンバイ状態の点滅を示す。

既定の `FUSES` 設定では、`SW0 (PF6)` はハードリセットスイッチとして機能せず、通常の GPIO 入力だ。
これをリセットスイッチとして使用するには、ユーザーアプリケーションで必要なコードを実装しなければならない。

ユーザーアプリケーションは、WDT 操作と SWRST 操作の 2 つのリセット方法を実装できる。
WDTリセットは常にユーザーアプリケーションを活性化し、ブートローダー活性化スイッチを無視する
SWRSTリセットは、ブートローダー活性化スイッチが LOW の場合に USB ブートローダを活性化する。

> [!TIP]
> "CURIOSITY NANO" で動作テストしている場合、`SW0 (PF6)`はハードリセットスイッチとしては機能しないがデバッガーは動作しているため、リモートで`UPDI`リセットを行うことができる。
> つまり単に以下のコマンドを実行するだけだ。その前から`SW0 (PF6)`を押し下げていれば、ブートローダーが活性化される。
> これは USBケーブルの抜き差しを省略できるので便利な方法だ。\
> `avrdude -cpkobn_updi -pavr64du32`

## スケッチ内からの euboot 活性化

`euboot v3.72.49` と `Multix-Zinnia-modernAVR-0.4.1` および [SerialUSB クラス](https://github.com/askn37/MacroMicroAPI_lib/blob/main/examples/USB%20for%20AVR-DU%20series/SerialUSB_Echo) との組み合わせで、実験的ながら Arduino IDE/CLI からのスケッチアップロードで、euboot が自動的に活性化するようになった。

つまり Arduino Leonard/Yun/Micro/Every Nano 等と同様の挙動を再現する。

具体的には `SerialUSB` を使用してかつシリアルコンソールが通信可能であれば、当該シリアルポートを 1200bps で開いて閉じることにより、euboot 活性化モードに切り替わる。この動作はサンプルスケッチ[SerialUSBの基本的な実演：イベント表示付き](https://github.com/askn37/MacroMicroAPI_lib/tree/9ccbe8eaa2c88be4962cbc3bb87ed13fa39cc985/examples/USB%20for%20AVR-DU%20series/SerialUSB_Echo_and_Event)で実際に確認できる。

> [!TIP]
シリアル通信を使わず、応用コード内から直接 euboot 活性化モードへ CPU動作を切り替えるには以下のようにする。

```cpp
#include <avr/io.h>
*(uint16_t*)(RAMEND - 1) = 0;       // SRAM領域最後の2byteをゼロ初期化して
_PROTECTED_WRITE(RSTCTRL_SWRR, 1);  // ソフトウェアリセットを実行
```

> [!TIP]
> SerialUSB 以外の実装では、このコードを USB-CDC の何処かに組み込む必要がある。

## SPM スニペット

USB ブートローダの最初のアドレスは、`PROGMEM` 領域の `PROGMEM_START` から始まる。ここには特別なマジック ナンバーと SPM スニペット コードが含まれる。

|Series|Address|Magic number: uint32_t (LE)|
|-|-|-|
|AVR_DA/DB/DD/DU/EA/EB|PROGMEM_START + 2 bytes|0x95089361|

> [!TIP]
> `(pgm_read_dword(PROGMEM_START + 2) == 0x95089361L`

|Offset|HWV=52|OP code|
|-|-|-|
|$02|nvm_stz|ST Z+, R22 \n RET
|$06|nvm_ldz|LD R24, Z+ \n RET
|$0A|nvm_spm|SPM Z+     \n RET
|$0E|nvm_cmd|(function)

これらは BOOT 領域保護特権を使用して、CODE/APPEND および BOOTROW 領域のフラッシュを消去/書き換えるために使用できる。

> 実際の使用例については、[[FlashNVM ツールリファレンス]](https://github.com/askn37/askn37.github.io/wiki/FlashNVM)を参照のこと。

## Related link and documentation

- [MultiX Zinnia Product SDK [modernAVR]](https://github.com/askn37/multix-zinnia-sdk-modernAVR)
  - v0.4.0+ で avrdude @v8.1 を同梱。
- [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB) : OSS/OSHW Programmer for UPDI/TPI/PDI
- [AVRDUDE](https://github.com/avrdudes/avrdude) @8.0+ （AVR-DUシリーズは8.0以降で正式サポート）

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
