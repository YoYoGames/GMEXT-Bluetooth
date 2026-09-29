if (locked) exit;
if (!instance_exists(owner)) exit;

owner.discover_all();
