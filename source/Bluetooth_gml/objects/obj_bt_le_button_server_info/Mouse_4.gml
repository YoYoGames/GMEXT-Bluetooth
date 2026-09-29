if (locked) exit;
if (!instance_exists(owner)) exit;

owner.change_info_value();
