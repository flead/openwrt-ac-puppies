#!/bin/sh
name=status
dir=/usr/share/$name

try_stop_instance() {
	local name=$1
	if [ -e /var/run/$name.pid ] ; then
		kill $(cat /var/run/$name.pid) &> /dev/null
		rm /var/run/$name.pid &> /dev/null
	fi
}

[ x$1 = xstop ] && {
	try_stop_instance $name
	exit 0
}

[ x$1 = xstart ] || {
	echo "usage: $0 start|stop"
	exit 0
}

try_stop_instance $name

cd $dir
echo `uptime` "start $name" >> /tmp/log/lua.error
lua $dir/main.lua 2>>/tmp/log/lua.error &

pid=$!
echo -n "$pid" > /var/run/$name.pid

wait