
if(async_load[?"id"] == request)
if(async_load[?"status"])
if(async_load[?"result"] != "")
{
    var data = async_load[?"result"];

	var buff = buffer_create(string_length(data)+1,buffer_fixed,1)
	buffer_write(buff,buffer_string,data)
	arduino_custom_write(buff,0,string_length(data)+1)
	buffer_delete(buff)
}
