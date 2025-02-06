# roamyboard, aka Pants Keyboard

A pants-mounted keyboard that I can use while walking and sitting. Original idea was to literally sew it into pants, but that’s too impractical; now I’m imagining it as a belt/strap you put on top of your pants to make it wearable.

Made by [Nevyn Bengtsson](mailto:hello@nevyn.dev) 2012-2025 (this is not the first iteration 😅).
# Inspiration

The general construction is based on ScottoChoczard, because it’s a straightforward hard-wired PCB-less design and thus doesn’t have to be flat, but can follow the curve of the leg.
```embed
title: "ScottoChoczard Handwired Keyboard Project"
image: "https://scottokeebs.com/cdn/shop/articles/scottochoczard.jpg?v=1730156756"
description: "The ScottoChoczard is a wireless split 3x6 ortholinear keyboard that uses Choc switches and large 750mah battery."
url: "https://scottokeebs.com/blogs/keyboards/scottochoczard-handwired-keyboard"
```

![](https://www.youtube.com/watch?v=LXfdxxXyiFE)

The hardware and layout is based on [![](https://typeractive.xyz/cdn/shop/files/favicon-typeractive_1_32x32.png?v=1640412698)Typeractive.xyzBuild - Wireless Corne and Lily58 Keyboard Kits](https://typeractive.xyz/pages/build/lily58). It’s wireless, based on the nice!nano just like the ScottoChoczard, but has a more conventional 4-row layout so I don’t have to go as deep in layers.
![[Lily58 example.png]]

# Parts
## Firmware and layout
To begin with, I'll just use the [stock Lily58 firmware](https://docs.typeractive.xyz/build-guides/lily58-wireless/firmware). Images are stuck in `firmware/`.

Later, I'll likely want to configure default keymap with a custom zmk build; I've thus put the [zmk config for lily58](https://github.com/typeractivexyz/lily58-wireless-view-zmk-config) in `zmk/`.

Read more in [[Layout]].

## Wiring

It'll probably end up looking something like ScottoChoczard:
![[choczard wiring.png]]
## Case

A hard shell 3d print of some sort. Read more in [[Case design]].

## Harness, fabrics etc

This case needs to attach to the user's body somehow. Read more in [[Harness design]].