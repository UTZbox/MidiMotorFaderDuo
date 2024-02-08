MidiMotorFaderDuo

Two Motorized Fader connected via USB-Midi for Controlling QLAB® Light-Controlls
Based on Teensy LC or 3.x

// Introduction:
This code let you build a Dual Motor-Fader Controller with 7-Segment Display connected by USB-Midi
Additional you can ad an MAX-485, to Output DMX as well.

// What can the Device do?
You can control the virtual Light-Faders in QLAB® by the Hardware Faders.
Any Changes of the virtual Light-Faders changes the position of the Hardware Faders.
So the Control is bidirectional.

// Funtions:
The first fader is fixed to Channel 1
The Channel of the second Fader can be switched from 1 to 32 by two push buttons.
So it is possible to control up to 32 Light-Faders

The 7-Segment Display shows the selected Channel and the Value of the Fader. 
In Addition you can ad a MAX-485 Chip to send out DMX512 as well.

// Notes:
Make sure your faders has an connected slider to detect while it is touched or not. If not the fader want to stay in his position.
The positioning of the faders is done by a simpified feedback loop to make it quick responsive and protect the motor from overhating.
Because the psition feedback of the motor faders is analog, make sure you have a stabile power supply.
Or using condensator to flatten it.
