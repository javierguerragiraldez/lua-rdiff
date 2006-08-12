--
-- lua_rdiff
-- (c) 2006 Javier Guerra G.
-- $Id: test.lua,v 1.1.1.2 2006-08-12 16:33:34 jguerra Exp $
--

require "lua_rdiff"
assert (rdiff)

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


--test = "AllString"
--test = "FileString"
test = "AllFile"

aFilename = "test/fileA.txt"
bFilename = "test/fileB.txt"



if test == "AllString" then

	fA = assert (io.open (aFilename, "rb"))
	strA = fA:read ("*a")
	fA:close()
	
	fB = assert (io.open (bFilename, "rb"))
	strB = fB:read ("*a")
	fB:close()
	
	print ("A",strA:len())
	print ("B",strB:len())
	
	sig = assert (rdiff.signature (strA))
	print ("sig", sig:len(), sig)
	
	del = assert (rdiff.delta (sig, strB))
	print ("delta", del:len(), del)
	
	rec = assert (rdiff.patch (del, strA))
	print ("reconst", rec:len(), rec)


elseif test == "FileString" then

	fA = assert (io.open (aFilename, "rb"))
	fsourceA = fsrc (fA)
	sig = assert (rdiff.signature (fsourceA))
	print ("sig", sig:len(), sig)
	fA:close()
	
	fB = assert (io.open (bFilename, "rb"))
	fsourceB = fsrc (fB)
	del = assert (rdiff.delta (sig, fsourceB))
	print ("delta", del:len(), del)
	fB:close()
	
	fA = io.open (aFilename, "rb")
	fsourceA = fsrc (fA)
	rec = assert (rdiff.patch (del, fsourceA))
	print ("reconst", rec:len(), rec)
	fA:close()

elseif test == "AllFile" then

	-- CREATE SIGNATURE FILE
	fA = io.open (aFilename, "rb")
	fsig = io.open ("test/signature", "wb")
	r = assert (rdiff.signature (fsrc (fA), fsink (fsig)))
	print ("signature:", type(r), r)
	fA:close()
	fsig:close()
	
	
	-- CREATE DELTA B-A FILE
	fsig = io.open ("test/signature", "rb")
	fB = io.open (bFilename, "rb")
	fdelta = io.open ("test/delta", "wb")
	r = assert (rdiff.delta (fsrc (fsig), fsrc (fB), fsink(fdelta)))
	print ("delta:", type(r), r)
	
	fsig:close()
	fB:close ()
	fdelta:close()
	
	
	-- RECREATE B BASED ON A
	fdelta = io.open ("test/delta", "rb")
	fA = io.open (aFilename, "rb")
	fnew = io.open ("test/new.txt", "wb")
	r = assert (rdiff.patch (fsrc(fdelta), fsrc(fA), fsink(fnew)))
	print ("patch:", type(r), r)
	
	fnew:close()
	fA:close()
	fdelta:close ()
	
end