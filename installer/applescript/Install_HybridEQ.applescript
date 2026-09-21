-- HybridEQ Installer (unsigned build helper)
-- Copies the VST3/AU/AAX plug-ins and the Standalone app from this folder
-- into the standard macOS locations, and removes the com.apple.quarantine
-- flag Gatekeeper stamps on anything downloaded/unzipped, since this build
-- is not yet notarized under an Apple Developer ID.

property pluginVST3 : "HybridEQ.vst3"
property pluginAU : "HybridEQ.component"
property pluginAAX : "HybridEQ.aaxplugin"
property appName : "HybridEQ.app"

on run
	set basePath to (path to me as text)
	set baseFolder to (POSIX path of ((basePath as alias) as text))
	-- baseFolder points at this .app; the payload sits under Payload/ next to it
	set scriptPOSIX to POSIX path of (path to me)
	set installerDir to do shell script "dirname " & quoted form of scriptPOSIX

	set vst3Src to installerDir & "/Plugins/VST3/" & pluginVST3
	set auSrc to installerDir & "/Plugins/Components/" & pluginAU
	set aaxSrc to installerDir & "/Plugins/AAX/" & pluginAAX
	set appSrc to installerDir & "/Standalone/" & appName

	set vst3Dst to "/Library/Audio/Plug-Ins/VST3/" & pluginVST3
	set auDst to "/Library/Audio/Plug-Ins/Components/" & pluginAU
	set aaxDst to "/Library/Application Support/Avid/Audio/Plug-Ins/" & pluginAAX
	set appDst to "/Applications/" & appName

	display dialog "This will install HybridEQ (VST3, Audio Unit, AAX, and the Standalone app) for all users on this Mac." & return & return & "Because this build isn't notarized with an Apple Developer ID yet, the installer also clears the macOS quarantine flag so Gatekeeper doesn't block it. You'll be asked for your Mac password to write into the system plug-in folders." buttons {"Cancel", "Install"} default button "Install" with icon note

	try
		set shellCmd to "" & ¬
			"set -e" & "
" & ¬
			"mkdir -p '/Library/Audio/Plug-Ins/VST3' '/Library/Audio/Plug-Ins/Components' '/Library/Application Support/Avid/Audio/Plug-Ins'" & "
" & ¬
			"rm -rf " & quoted form of vst3Dst & "
" & ¬
			"cp -R " & quoted form of vst3Src & " " & quoted form of vst3Dst & "
" & ¬
			"rm -rf " & quoted form of auDst & "
" & ¬
			"cp -R " & quoted form of auSrc & " " & quoted form of auDst & "
" & ¬
			"rm -rf " & quoted form of aaxDst & "
" & ¬
			"cp -R " & quoted form of aaxSrc & " " & quoted form of aaxDst & "
" & ¬
			"rm -rf " & quoted form of appDst & "
" & ¬
			"cp -R " & quoted form of appSrc & " " & quoted form of appDst & "
" & ¬
			"xattr -dr com.apple.quarantine " & quoted form of vst3Dst & " " & quoted form of auDst & " " & quoted form of aaxDst & " " & quoted form of appDst & " || true
" & ¬
			"killall -9 AudioComponentRegistrar 2>/dev/null || true"

		do shell script shellCmd with administrator privileges

		display dialog "HybridEQ was installed successfully:" & return & return & ¬
			"• VST3 -> /Library/Audio/Plug-Ins/VST3/" & return & ¬
			"• Audio Unit -> /Library/Audio/Plug-Ins/Components/" & return & ¬
			"• AAX -> /Library/Application Support/Avid/Audio/Plug-Ins/" & return & ¬
			"• Standalone app -> /Applications/" & return & return & ¬
			"Restart your DAW to load the plug-in." buttons {"OK"} default button "OK" with icon note
	on error errMsg number errNum
		if errNum is not -128 then
			display dialog "Installation failed:" & return & errMsg buttons {"OK"} default button "OK" with icon stop
		end if
	end try
end run
