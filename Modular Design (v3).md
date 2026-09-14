> 💡 This is an overview of a design based on independent columns with a column interconnect. The [[Case design]] uses it in v3, and details of how it works electrically is detailed in [[Electronics]].

In this Modular Design, the roundedness of the keyboard comes from each key column actually being a discrete physical module, that joins together with each other at an angle; creating an arc when all the modules are joined together.

It's also a ridiculously modular design, since this means you can practically mash together as many columns as you want in your keyboard.


![[Modular sketch.png]]
- Each column is its own module, so that the user can choose how wide to make their keyboard by connecting multiple together.
	- Each column module has 6 keys, creating a full-height keyboard with F keys. (We could also make 4 or 5 key height columns and I think everything should still work)
	- The rightmost column module is the brains (see [[Electronics]] for details)
		- It also has the rightmost fastener for the harness
	- The leftmost column is the terminator (see [[Electronics]])
		- It also has the leftmost fastener for the harness
	- Modules are not intended to be connected/disconnected while powered.
- Each column module joins to the next by clicking straight on. Three 1×3 pin headers (male on one side, sockets on the other) at the top, middle, and bottom of the board edge carry the 9-signal interconnect (see [[Electronics]]). 
	- The enclosure chassis reinforces the mechanical joint so the solder joints don't carry the load
	- The pins are bent to the joint angle so they enter the next module's sockets along its board plane:
	  ![[module-joint.svg]]
	- _Earlier iterations explored a Joycon-style slide-rail and pogo-pin connectors, but both were abandoned: pogo pins of the right pitch were prohibitively expensive, and the slide-rail geometry couldn't coexist with the PCB's keyswitch courtyards._
