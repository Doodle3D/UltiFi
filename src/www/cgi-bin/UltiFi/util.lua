
function runTemplate(filename, data)
	f = io.open("/UltiFi/www/template/" .. filename .. ".html", "r")
	while true do
		line = f:read()
		if line == nil then break end
		
		while true do
			s = string.find(line, '{')
			if s == nil then break end
			e = string.find(line, '}', s)
			
			key = string.sub(line, s + 1, e - 1)
			if data == nil or data[key] == nil then
				value = ''
			else
				value = data[key]
			end
			line = string.sub(line, 0, s - 1) .. value .. string.sub(line, e + 1)
		end
		
		print(line)
	end
	f:close()
end

function getParam(id)
	q = os.getenv('QUERY_STRING')
	for k, v in string.gmatch(q, '(%w+)=([^&]+)') do
		if k == id then
			v = string.gsub(v, '%%20', ' ')
			v = string.gsub(v, '|', '\n')
			return v
		end
	end
	return ''
end

function printerComp(id1, id2)
	return printerName(id1) < printerName(id2)
end

function printerList()
	list = {}
	for n = 0, 30 do
		f = io.open('/tmp/UltiFi/ttyACM' .. n)
		if f ~= nil then
			list[#list+1] = n
			f:close()
		end
	end
	table.sort(list, printerComp)
	return list
end

function printerName(id)
	f = io.open('/tmp/UltiFi/ttyACM'..id..'/sdlist.out')
	if f ~= nil then
		for line in f:lines() do
			lline = string.lower(line)
			if string.find(lline, '/_id/') ~= nil then
				f:close()
				return string.sub(line, 6, -3)
			end
		end
		f:close()
	end
	return 'ID:' .. id
end

function printerTemp(id)
	f = io.open('/tmp/UltiFi/ttyACM'..id..'/temp.out')
	if f ~= nil then
		line = f:read()
		f:close()
		if line == nil then line = '' end
		n = string.find(line, 'T:')
		line = string.sub(line, n + 2)
		n = string.find(line, ' ')
		return line.sub(line, 0, n - 1)
	end
	return '?'
end

function printerProgress(id)
	f = io.open('/tmp/UltiFi/ttyACM'..id..'/progress.out')
	if f ~= nil then
		line = f:read()
		f:close()
		if line == nil then line = '' end
		n = string.find(line, 'SD printing byte ')
		if n ~= nil then
			line = string.sub(line, n + 17)
			n = string.find(line, '/')
			pos = tonumber(line.sub(line, 0, n - 1))
			size = tonumber(string.sub(line, n + 1))
			if pos ~= size then
				return string.format('%0.1f', (pos * 100 / size)) .. '%'
			end
		end
	end
	return 'Done'
end

function fileList(id)
	list = {}
	if id == 'all' then
		--If we have all printers selected, show the SDlist of the first printer
		f = io.open('/tmp/UltiFi/ttyACM0/sdlist.out')
	else
		f = io.open('/tmp/UltiFi/ttyACM'..id..'/sdlist.out')
	end
	if f ~= nil then
		for line in f:lines() do
			--Need to put the filename in lowercase, because the printer sends upper case filenames, but requires lowercase filenames in the M23
			line = string.lower(line)
			if string.find(line, '/_id/') == nil then
				list[#list+1] = line
			end
		end
		f:close()
	end
	table.sort(list)
	return list
end
