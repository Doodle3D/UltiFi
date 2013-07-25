#!/usr/bin/lua

package.path = "/UltiFi/www/cgi-bin/UltiFi/?.lua"
require "util"

print("Content-type: text/html")
print("")

id = getParam('id')

runTemplate("header", {title = printerName(id)})
runTemplate("printer", {id = id})

print('<h2>SD card files</h2>')
print('<div data-role="controlgroup">')
for idx, filename in ipairs(fileList(id)) do
	print('<a href="exec_gcode.lua?id='..id..'&gcode=M23 '..filename..'|M24" data-role="button">'..filename..'</a>')
end
print('</div>')

runTemplate("footer")
