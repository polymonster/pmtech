create_app("pmtech_editor", "pmtech_editor", script_path())

-- win32 needs to export a lib for the live lib to link against
if platform == "win32" then
	configuration {"Debug"}
		prebuildcommands
		{
			"py -3 ../../../tools/pmbuild_ext/libdef.py ../../../core/put/lib/win32/debug/put.lib ../../../core/pen/lib/win32/debug/pen.lib -o pmtech_d.def",
		}
		linkoptions {
		  "/DEF:\"pmtech_d.def"
		}
	configuration {"Release"}
		prebuildcommands
		{
			"py -3 ../../../tools/pmbuild_ext/libdef.py ../../../core/put/lib/win32/release/put.lib ../../../core/pen/lib/win32/release/pen.lib -o pmtech.def"
		}
		linkoptions {
		  "/DEF:\"pmtech.def"
		}
end
	