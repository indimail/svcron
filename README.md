


This is a version of 'cron' that is known to run on most systems.  It
is functionally based on the SysV cron, which means that each user can have
their own crontab file (all crontab files are stored in a read-protected
directory, usually /var/cron/tabs).  No direct support is provided for
'at'; you can continue to run 'atrun' from the crontab as you have been
doing.  If you don't have atrun (i.e., System V) you are in trouble.

A messages is logged each time a command is executed; also, the files
"allow" and "deny" in /var/cron can be used to control access to the
"crontab" command (which installs crontabs).  It hasn't been tested on
SysV, although some effort has gone into making the port an easy one.

To use this: Sorry, folks, there is no cutesy 'Configure' script.  You'll
have to go edit a couple of files... So, here's the checklist:

	Read all the FEATURES, INSTALL, and CONVERSION files
	Edit config.h
	Edit Makefile
		(both of these files have instructions inside; note that
		 some things in config.h are definable in Makefile and are
		 therefore surrounded by #ifndef...#endif)
	'make'
	'su' and 'make install'
		(you may have to install the man pages by hand)
	kill your existing cron process
		(actually you can run your existing cron if you want, but why?)
	build new crontabs using /usr/lib/{crontab,crontab.local}
		(either put them all in "root"'s crontab, or divide it up
		 and rip out all the 'su' commands, collapse the lengthy
		 lists into ranges with steps -- basically, this step is
		 as much work as you want to make it)
	start up the new cron
		(must be done as root)
	watch it. test it with 'crontab -r' and watch the daemon track your
		changes.
	if you like it, change your /etc/{rc,rc.local} to use it instead of
		the old one.

$Id: README,v 1.6 2004/01/23 19:03:32 vixie Exp $
