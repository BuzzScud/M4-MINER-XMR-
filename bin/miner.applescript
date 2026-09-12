on run
	set ui to "/Users/christiantavarez/Desktop/PROJECTS/XMR MINER/bin/miner-ui.sh"
	tell application "Terminal"
		activate
		set t to do script "clear; exec " & quoted form of ui
		try
			set number of columns of front window to 147
			set number of rows of front window to 58
		end try
	end tell
end run
