if (locked) exit;
if (!instance_exists(owner)) exit;

owner.send_demo_notification();
