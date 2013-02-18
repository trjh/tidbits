tidbits
=======

Misc homebrew code

timer.pl -- set up and display one or more multiple timers; say to be used
	    with an online strategy game to track when attacks will complete
	    or other actions will happen

timerwatch.pl -- this program runs continuously, watching the ~/.timers file
	    created by timer.pl, and beeping when any timer included reaches
	    zero.

md5allupdate.pl -- if you've used 'find' to create a list of md5 checksums on
	    a filesystem (or portion thereof), this program will read that md5
	    list, and then build a new list of checksums for the same
	    filesystem -- recomputing the md5 only for files that have changed
	    or been added since the input list was created.
