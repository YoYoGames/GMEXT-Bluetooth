/// @description Insert description here
// You can write your code in this editor

// Inherit the parent event
event_inherited();

text = "Unsubscribe";
locked = (global.characteristic.properties & (16 | 32)) == 0;