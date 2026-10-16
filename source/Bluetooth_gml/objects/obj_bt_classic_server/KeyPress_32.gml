// Space sends a text record; the connection frames it, so it cannot
// misalign the position records on the same stream.
if (global.classic_conn != 0 && instance_exists(global.classic_conn_inst))
{
    global.classic_conn_inst.send_text("Hello");
}
