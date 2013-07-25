#!/usr/bin/lua

package.path = "/UltiFi/www/cgi-bin/UltiFi/?.lua"
require "util"

print("Content-type: text/html")
print("")

printers = printerList()
runTemplate("header", {title = #printers .. ' printers'})
print('<ul data-role="listview" data-inset="true">')
for _, id in ipairs(printers) do
	print('<li data-icon="arrow-r"><a href="printer.lua?id='..id..'">Printer '..printerName(id)..' - '..printerTemp(id)..'&deg; - '..printerProgress(id)..'</a></li>')
end
print('</ul>')
if #printers > 1 then
	print('<ul data-role="listview" data-inset="true">')
	print('<li data-icon="arrow-r"><a href="printer.lua?id=all">All printers</a></li>')
	print('</ul>')
elseif #printers < 1 then
	print('<div data-role="content" data-theme="e" data-content-theme="e"><h2>No printers found. Please connect 1 or more printers to the USB port.</h2></div>')
end
runTemplate("footer")
