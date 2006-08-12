
require "lua_rdiff"

assert (rdiff)

function getfilesource (fname)
	local f = assert (io.open (fname, "rb"))

	return function (pos, len)
		if pos == nil and len == nil then
			return f:read (25)
		else
			f:seek ("set", pos)
			return f:read (len)
		end
	end
end

function getfilesink (fname)
	local f = assert (io.open (fname, "wb"))
	return function (data) f:write (data) end
end

function fsrc (f)
	return function (pos, len)
		if pos == nil and len == nil then
			return f:read (25)
		else
			f:seek ("set", pos)
			return f:read (len)
		end
	end
end

function fsink (f)
	return function (data) f:write (data) end
end




-- strA = io.open ("01.in"):read ("*a")
-- strB = io.open ("COPYING"):read ("*a")
-- 
-- print ("A",strA:len())
-- print ("B",strB:len())
-- 
-- sig = rdiff.signature (strA)
-- print ("sig", sig:len(), sig)
-- 
-- del = rdiff.delta (sig, strB)
-- print ("delta", del:len(), del)
-- 
-- rec = rdiff.patch (del, strA)
-- print ("reconst", rec:len(), rec)



-- fsourceA = getfilesource ("01.in")
-- fsourceB = getfilesource ("COPYING")
-- 
-- sig = rdiff.signature (fsourceA)
-- print ("sig", sig:len(), sig)
-- 
-- del = rdiff.delta (sig, fsourceB)
-- print ("delta", del:len(), del)
-- 
-- rec = rdiff.patch (del, fsourceA)
-- print ("reconst", rec:len(), rec)


-- -- CREATE SIGNATURE FILE
-- fA = io.open ("01.in", "rb")
-- fsig = io.open ("signature", "wb")
-- 
-- r = assert (rdiff.signature (fsrc (fA), fsink (fsig)))
-- print ("signature:", type(r), r)
-- 
-- fA:close()
-- fsig:close()
-- 
-- -- CREATE DELTA B-A FILE
-- fsig = io.open ("signature", "rb")
-- fB = io.open ("COPYING", "rb")
-- fdelta = io.open ("delta", "wb")
-- r = assert (rdiff.delta (fsrc (fsig), fsrc (fB), fsink(fdelta)))
-- print ("delta:", type(r), r)
-- 
-- fsig:close()
-- fB:close ()
-- fdelta:close()
-- 
-- 
-- -- RECREATE B BASED ON A
-- fdelta = io.open ("delta", "rb")
-- fA = io.open ("01.in", "rb")
-- fnew = io.open ("new", "wb")
-- r = assert (rdiff.patch (fsrc(fdelta), fsrc(fA), fsink(fnew)))
-- print ("patch:", type(r), r)
-- 
