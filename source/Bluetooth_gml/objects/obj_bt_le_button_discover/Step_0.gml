// Discovery runs across several async callbacks; block re-entry until done.
locked = instance_exists(owner) && owner.discovery_in_progress;

event_inherited();

text = locked ? "DISCOVERING..." : "DISCOVER GATT";
