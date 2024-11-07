# euboot -- EDBG USB bootloader for AVR-DU series

*Switching document languages* : __日本語__, [English](README.md)

## 機能

- AVR-DU シリーズ専用 USB ブートローダー。
- 標準 USB-HID/CMSIS-DAP/EDBG プロトコルを使用し、AVRDUDE<=8.0 からは `jtag3updi` デバイスとして認識される。
- USB Full-Speed の上限に近い、高速なメモリの読み書き速度。
- フットプリントは 2.5KiB 未満。

## 開発の理由

AVR-DU シリーズは、USB 周辺機器を内蔵した唯一の modernAVR 世代だが、以前の同様の製品 (ATMEL 世代) とは異なり、DFU ブートローダーは付属しておらず、ベアメタル チップ フラッシュは常に空だ。

これは ATMEL が USB-IF DFU 標準開発の正規メンバーであったのに対し、Microchip はそうではなかったことが一因だろう。そのため将来的に DFU サポートが提供されることはあまり期待できない。

現在、AVRDUDE には USB-CDC と USB-HID という 2 つの代替ブートローダ アプローチがある。CDC (VCP) は非常によく使われているが、通信をバイトに分割して解釈しなければならず、プロトコルが複雑で遅く、SRAM 管理に多くの労力が必要であり、さらに悪いことに新しいチップのメモリを適切にサポートするには AVRDUDE に新しいパッチが必要になるなど多くの欠点があり、新規開発で選択するには、利点がほとんど見出せない。

一方、USB-HID を使用したバルク転送通信は USB の専門知識が必要であるため、あまり使われていない。しかし CMSIS-DAP および EDBG プロトコルをサポートし、`jtag3updi` も処理できる [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/) が、今や手元にある。これをベースに必要な部分のみを実装し、NVM 制御を追加するのは容易いことだ。結果は良好で、フットプリント 2.5KiB の `jtag3updi` をサポートする USB ブートローダを作り出す事ができた。DFU ほど小さくはないが実用上は十分コンパクトだ。また内部オーバーヘッドが少ないため、USB Full-Speed の上限に近いメモリ読み取り速度を簡単に実現できる。

## ブートローダ ファームウェアを作成するために必要なもの

これを行うには、次の環境が必要だ:

#### [MultiX Zinnia Product SDK [modernAVR] @0.3.0+](https://github.com/askn37/multix-zinnia-sdk-modernAVR)

Arduino IDE/CLI の ボード マネージャーで簡単にインストールできるベアメタル開発SDK。AVR-LIBC を使いやすくするさまざまなマクロを備えており、Arduino-API に近い使用感で低レベル コードを記述できる。同時に AVRDUDE 8.0+ もインストールされる。

#### [Arduino-CLI @1.0.3+](https://arduino.github.io/arduino-cli/1.0/installation/)

付属の Makefile を使用してファームウェアをビルドする場合に必要。
これがなくとも modernAVR SDK だけでバイナリファイルを出力できるが、メニュー設定が煩雑になる。

> [!HINT]
> Windows 環境では、`make` をそのまま使用することはできない。WSL などの方法を使用する必要がある。

#### AVR-DU シリーズ用の UPDI 互換プログラマー

これは主に `PICKit4` などである。だが世界で最も入手しやすく安価なのは ["AVR64DU32 Curiosity Nano : EV59F82A"](https://www.microchip.com/en-us/development-tool/ev59f82a) だろう。まずこれを入手すれば、AVR64DU32 の完全なスタンドアロン開発環境がこれ一つで揃う。また [UPDI4AVR-USB](https://github.com/askn37/UPDI4AVR-USB/) をインストールすると、AVRDUDEからは `PICKit4` のような UPDIプログラマーとして使えるようにもできる。詳細は各リンクを参照のこと。

## ブートローダーファームウェアの作成とインストール

modernAVR SDKをインストールし、Arduino-CLIとAVRDUDE 8.0を実行パスに追加し、準備ができたら`euboot`ディレクトリに移動して`make all`を実行する。生成されたファイルは`hex`ディレクトリに保存される。

```sh
euboot $ make all
```

> [!HINT]
> `Perl5`実行ファイルがある場合、hex/binファイルには`CRCSCAN`周辺機器で使用するための CRC32 が埋め込まれる。

生成されたファイルをターゲットにアップロードする。この例でのターゲットは CURIOSITY NANO (CNANO) だが、これには `pkobn_updi` が組み込まれているため簡単に試す事ができる。

```sh
euboot $ avrdude -cpkobn_updi -pavr64du32 -Uflash:w:hex/euboot_LF2_SF6.hex:i -Ufuses:w:hex/euboot_LF2_SF6.fuse:i
```

アップロードが成功すると、ファームウェアはすぐに動作を開始する。CNANO のターゲット USB ポートに何も接続されていない場合、LED (PF2) は次のパターンで点滅し続ける。これはホスト PC との USB 列挙が完了していないことを意味する。

- LED(PF2): 🟠⚫️⚫️⚫️ (USB列挙を待機中)

2本目の USB ケーブルをターゲット USB ポートに接続するか、USBケーブルをデバッガーポートから差し替えると、LED が次の点滅パターンに変わり、EDBG プロトコル通信を開始する準備ができたことを示す。

- LED(PF2): 🟠🟠⚫️⚫️ (スタンバイモード)

準備完了？ では USB ブートローダーが正しく応答するかどうかを確認しよう。`-P` オプションの記述に注意。

```sh
$ avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v
```

```text
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
ICE FW version        : 3.72 (rel. 48)
Serial number         : euboot:CMSIS-DAP:EDBG
Vtarget               : 3.30 V
PDI/UPDI clk          : 2560 kHz

Partial Family_ID returned: "AVR "
Silicon revision: 1.3

AVR device initialized and ready to accept instructions
Device signature = 1E 96 21 (AVR64DU32)

Avrdude done.  Thank you.
```

ファームウェア固有の情報は、`ICE HW バージョン` 以降にリストされる。AVRDUDE にこれらの特定情報を表示する機能がないため、項目名と内容が一致しないのは仕様だ。代わりに未使用の項目を使って表示される。

- __ICE HW バージョン__: 常に 52。これは元々 `4` の文字コードであり、NVM コントローラーのバージョンを示す。
- __ICE FW バージョン__: USB ブートローダのバージョンと更新番号を示す。
- __シリアル番号__: USB デバイス製品文字列を示す。USB シリアル番号文字列は使用されないため、両方とも同じ文字列になっている。
- __Vtarget__: VDD に供給されている現在の動作電圧を示す。
- __PDI/UPDI clk__: これは実際の動作速度ではなく、`FUSE_BOOTSIZE` 設定から計算されたユーザー アプリケーション の プログラム開始アドレスだ。たとえば `2560` は、ユーザー アプリケーション が プログラム開始アドレス `0x0A00` から実行されることを示す。`0` の場合、必要な FUSEは正しく設定されていない。

前述のプログラムアドレスから実行されるように構成されたユーザー アプリケーションは、次のようにスタンバイ状態の USB ブートローダで書き込むことができる。`-D` オプションは推奨。メモリの読み取り/書き込み中に点滅する LED は、次のように変わる:

- LED(PF2): 🟠🟠🟠🟠 (通信中)

```sh
$ avrdude -Pusb:04d8:0b12 -cjtag3updi -pavr64du32 -v -D -Uflash:w:USERAPP.hex:i
```

同様に `-U` オプションを使用して、`eeprom`、`userrow`、および `bootrow` の書き込みと読み取りを行うことができる。
`fuse(s)` および `lock` は読み取り専用であり、変更できない。

## USB ブートローダーの有効化

ユーザー アプリケーションがまだ書き込まれておらず、フラッシュが空の場合、USB ブートローダーの実行が優先されるが、ひとたび ユーザー アプリケーションが書き込まれると、USB ブートローダーはスキップされる。そうではなく USB ブートローダをアクティブにするには、`SW0(PF6)` を押したまま CNANO の電源を入れなければならない。

成功すると、LED が点滅してスタンバイ モードを示す。

既定の `FUSES` 設定では、`SW0(PF6)` はハード リセット スイッチとして機能せず、通常の GPIO 入力だ。これを リセット スイッチとして使用するには、ユーザー アプリケーションで必要なコードを実装しなければならない。

ユーザー アプリケーションは、WDT 操作と SWRST 操作の 2 つのリセット方法を実装できる。WDT リセットは常にユーザー アプリケーションをアクティブにし、アクティブ スイッチを無視する。SWRST リセットは、アクティブ スイッチが LOW の場合に USB ブートローダをアクティブにする。

## SPM スニペット

USB ブートローダの最初のアドレスは、`PROGMEM` 領域の `PROGMEM_START` から始まる。ここには特別なマジック ナンバーと SPM スニペット コードが含まれる。

|Series|Address|Magic number: uint32_t (LE)|
|-|-|-|
|AVR_DA/DB/DD/DU/EA/EB|PROGMEM_START + 2 bytes|0x95089361|

> [!TIPS]
> `(pgm_read_dword(PROGMEM_START + 2) == 0x95089361L`

|Offset|HWV=52|OP code|
|-|-|-|
|$02|nvm_stz|ST Z+, R22 \n RET
|$06|nvm_ldz|LD R24, Z+ \n RET
|$0A|nvm_spm|SPM Z+     \n RET
|$0E|nvm_cmd|(function)

これらは BOOT 領域保護特権を使用して、CODE/APPEND および BOOTROW 領域のフラッシュを消去/書き換えるために使用できる。

> 実際の使用例については、[[FlashNVM ツールリファレンス]](https://github.com/askn37/askn37.github.io/wiki/FlashNVM)を参照のこと。

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
