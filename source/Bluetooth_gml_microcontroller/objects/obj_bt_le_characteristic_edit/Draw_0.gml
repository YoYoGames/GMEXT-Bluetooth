/// @description Insert description here
// You can write your code in this editor

draw_set_valign(fa_top);
draw_set_halign(fa_left);
draw_text(x, y, $"Peripheral: {global.address}\n" + 
				$"Service: {global.serviceUuid}\n" + 
				$"Characteristic: {global.characteristic.uuid}\n\n\n\n\n" +
				$"Descriptors:");
