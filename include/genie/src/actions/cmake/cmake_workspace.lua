--
-- _cmake.lua
-- Define the CMake action(s).
-- Copyright (c) 2015 Miodrag Milanovic
--

function premake.cmake.workspace(sln)
	if (sln.location ~= _WORKING_DIR) then
		local name = string.format("%s/CMakeLists.txt", _WORKING_DIR)
		local f, err = io.open(name, "wb")
		if (not f) then
			error(err, 0)
		end
		f:write('# CMakeLists autogenerated by GENie\n')
		f:write('cmake_minimum_required(VERSION 2.8.4)\n')
		f:write('\n')
		f:write('add_subdirectory('.. path.getrelative(_WORKING_DIR, sln.location) ..')\n')
		f:close()
	end
	_p('# CMakeLists autogenerated by GENie')
	_p('cmake_minimum_required(VERSION 2.8.4)')
	_p('')
	for i,prj in ipairs(sln.projects) do
		local name = premake.esc(prj.name)
		_p('add_subdirectory(%s)', name)
	end
end