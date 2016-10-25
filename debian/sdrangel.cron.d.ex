#
# Regular cron jobs for the sdrangel package
#
0 4	* * *	root	[ -x /usr/bin/sdrangel_maintenance ] && /usr/bin/sdrangel_maintenance
