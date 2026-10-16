// Space sends a text record; the connection frames it, so it cannot
// misalign the position records on the same stream.
if (conn != 0 && instance_exists(conn_inst))
{
    conn_inst.send_text("Hello");
}
