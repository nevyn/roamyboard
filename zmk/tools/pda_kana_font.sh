#!/bin/sh
# Generates zmk/src/display/font_kana_40.c, the PDA's kana: the 46 basic hiragana and 46
# basic katakana from Noto Sans JP (SIL Open Font License 1.1; notice and license in
# zmk/src/display/font_kana_40.LICENSE),
# 40 px, 1 bit per pixel.
# Usage: zmk/tools/pda_kana_font.sh NotoSansJP-Regular.otf
# The font comes from https://github.com/notofonts/noto-cjk (Sans/SubsetOTF/JP).
set -eu
font=$1
out=$(cd "$(dirname "$0")/../src/display" && pwd)/font_kana_40.c
hiragana="あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
katakana="アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン"
npx --yes lv_font_conv@1.5.3 --no-compress --no-prefilter --bpp 1 --size 40 --font "$font" \
    --symbols "$hiragana$katakana" --format lvgl --lv-include lvgl.h --force-fast-kern-format \
    --lv-font-name roamyboard_font_kana_40 -o "$out"
# Keep the header free of this machine's paths.
sed -i.bak -e "s|--font $font|--font NotoSansJP-Regular.otf|" -e "s|-o $out|-o font_kana_40.c|" "$out"
rm -f "$out.bak"
