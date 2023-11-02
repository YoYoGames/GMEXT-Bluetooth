
function get_service_uuid()
{
	switch(os_type)
	{
		case os_android: return "443EB72C-1477-11EE-BE56-0242AC120002"
		case os_ios: return "443EBA4C-1477-11EE-BE56-0242AC120002"
		case os_macosx: return "443EBC4A-1477-11EE-BE56-0242AC120002"
		case os_windows: return "1439652E-177C-11EE-BE56-0242AC120002"
	}
}

function get_characteristic_write_uuid()
{
	switch(os_type)
	{
		case os_android: return "443EBFEC-1477-11EE-BE56-0242AC120002"
		case os_ios: return "443EC14A-1477-11EE-BE56-0242AC120002"
		case os_macosx: return "443ED770-1477-11EE-BE56-0242AC120002"
		case os_windows: return "143967AE-177C-11EE-BE56-0242AC120002"
	}
}

function get_characteristic_notify_uuid()
{
	switch(os_type)
	{
		case os_android: return "443ED9F0-1477-11EE-BE56-0242AC120002"
		case os_ios: return "443EDB58-1477-11EE-BE56-0242AC120002"
		case os_macosx: return "443EDCA2-1477-11EE-BE56-0242AC120002"
		case os_windows: return "1439690C-177C-11EE-BE56-0242AC120002"
	}
}

