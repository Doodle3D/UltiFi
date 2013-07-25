#!/usr/bin/lua

package.path = "/UltiFi/www/cgi-bin/UltiFi/?.lua"
require "util"

print("Content-type: text/html")
print("")

id = getParam('id')
gcode = getParam('gcode')

if id == 'all' then
	for _, id in ipairs(printerList()) do
		f = io.open('/tmp/UltiFi/ttyACM'..id..'/command.in', 'w')
		f:write(gcode)
		f:close()
	end
else
	f = io.open('/tmp/UltiFi/ttyACM'..id..'/command.in', 'w')
	f:write(gcode)
	f:close()
end

runTemplate("header")
print('<meta http-equiv="refresh" content="0; URL=printer.lua?id='..id..'" />')
print('<p>Executed: '..gcode..'</p>')
print('<a href="printer.lua?id='..id..'" data-role="button" data-icon="arrow-l">Back</a>')
runTemplate("footer")
