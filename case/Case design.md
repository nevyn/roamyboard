So what I'm thinking is that I will need a hard enclosure for the electronics, because a flexible keyboard is very hard to type on (that's an earlier failed experiment of mine). So I want to basically build a top plate with roughly the same layout as lily58, but curved (I'm thinking flat sections with a slight bend in between each column). And then I can make a bottom case with the same curvature to mount battery, controller etc in.

# Version 3
![[Modular sketch.png]]
Before even finishing version 2, Bengan wanted in on the project, but focused on just a single half and for gaming.
That made me thinking: What if roamyboard is made out of modular columns? That you can snap together to make it as narrow or wide as you want. By making the edges sloped, you create curvature when snapping them together. Each column could then also have its own PCB.

This iteration is designed in CadQuery and the file can be found in roamy-v3.py. Read more in [[Modular Design (v3)]].

## 3.0
![[proto 3.0.png]]
Learnings:
- [x] Print standing up, not laying down. The supports going into the module are terrible.
- [x]  Make the T socket smaller. The tolerances won't allow it to go in
- [x]  Keys are too wide apart. Make the groove negative, and remove some margins on the main body

## 3.1
![[proto 3.1.png]]
![[proto 3.1 irl.png]]
* Standing up print helped a lot!
* 0.275 top Z distance helped a lot to make supports easy to remove
* My clearance math is bad so I immediately rewrote it for 3.2, so we'll see if it actually works then too
* Forgot to make the keys less wide apart
## 3.2
![[proto 3.2.png]]
* Nicer math for clearance
## 3.3
![[proto 3.3.png]]
* Curvature!

Learnings:
- [ ] The right wall (by the t socket) is needlessly thick due to the prism. Can I make it thinner? Move the prism "inside" the body? Cut out the inner hollow _after_ joining with the prism and shift it to the right?

Todo:
- [ ] Design the brain module
- [ ] Design the terminator module
- [ ] Learn KiCad and design the PCBs :O
# Version 2
Curvature. How much is the right curvature? I could math it. Or wing it. Still working in the same OnShape file. Here's V1 compared to wingin' it for v2:
![[curvature1.png]]
![[curvature2.png]]

- [x] Wider curvature
- [x] Narrower between keys
- [x] Make it more variable driven
- [ ] Another column to fit the microcontroller, screen and battery
- [ ] Use the profile of actual switch to make a fitting that hooks into the switch
	- [ ] Sink the top of the switch into the part
	- [ ] Make it snug
- [ ] Close up the two unused holes
- [ ] Fix screw posts
- [ ] On the bottom case, add something for fastening to the harness
- [ ] Try making the base an actual curve and just the key seats square! 
	- [ ] Probably means remaking the model from scratch.
	- [ ] Can I project a sketch onto a shape to pattern the holes?

## Version 1
We're [designing `roamyboard` in OnShape](https://cad.onshape.com/documents/851b4428f77cf5984ffec433/w/909caff9b78525c7946543d4/e/0bc4b4ece8e92a1460095204).

![[roamyboard_v1.png]]
See [[Parts]] to see what key caps etc were used.

Learnings:
* Slightly higher curvature; it's too tight on my leg
* Slightly narrower between keys
* A good position on the leg is out towards the side, roughly 45° leaning, not centered on leg. So right below/atop the pocket.
* The plate is actually too thick for the keys to anchor properly! Use the key model as a prop to make a perfect little hole for the keys.
* One more column for the screen basically
* Column offsets? The leftmost and rightmost columns aren't very easy to reach... I hope this is fixed by just moving the keys closer together :S
* I'm going to have to order more buttons and caps I think
* Interestingly, it needs to be located at pocket height when standing up, and just above the knee when stitting down. So it'll need an adjustment system to move it up and down the leg
* I forgot to order "homing" keycaps, for finding the home row by touch.



## Tools 
Raj recommends doing it manually in OnShape. 

There are also generators:
```embed
title: "Cosmos Keyboard"
image: "https://ryanis.cool/cosmos/alien.svg"
description: "Custom-Build A Keyboard Fit To You"
url: "https://ryanis.cool/cosmos/"
```

```embed
title: "Ergogen"
image: "https://github.com/madebyperce.png"
description: "Web UI for the ergogen tool"
url: "https://ergogen.xyz/#"
```

## Process
So we're making a top case with holes for switches, similar to Choczard:
![[choczard grid.png]]

... but with the rough layout of lily58:
[![Lily58Lite-Pic](https://user-images.githubusercontent.com/6285554/84393842-13960900-ac37-11ea-811e-65db2948ca73.jpg)](https://user-images.githubusercontent.com/6285554/84393842-13960900-ac37-11ea-811e-65db2948ca73.jpg)
... so what we're looking at on each half **is a 6x4 grid, plus an extra row of 4-ish thumb keys**.

Here's Gergo for reference:
[![Ortholinear Keyboard Poll | Drop](https://massdrop-s3.imgix.net/img_poll/1573329558104.086624807538320047948280-md_img.jpg?auto=format&fm=jpg&fit=fill&w=400&h=400&bg=FFFFFF&dpr=1&q=70)![Ortholinear Keyboard Poll | Drop](https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcTB-r2tiqPlEdZ_vJLD98Y08Sk4slpzM84dsg&s)](https://www.google.com/url?sa=i&url=https%3A%2F%2Fdrop.com%2Fvote%2FOrtholinear-Keyboard3&psig=AOvVaw1hnNe-H3Z-tfUHhFxrLA26&ust=1739209569608000&source=images&cd=vfe&opi=89978449&ved=0CBQQjRxqFwoTCNDH4N-St4sDFQAAAAAdAAAAABAE)

As for metrics, Raj says:
> I’d recommend not trying to design in every little detail. Maybe start with a rough shape with holes for switches. The choc keys are 14x14 square. Should be 1mm deep wall for the clips to work

