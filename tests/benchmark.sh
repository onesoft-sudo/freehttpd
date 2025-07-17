#!/bin/sh
#
# This file is part of OSN freehttpd.
#
# Copyright (C) 2025  OSN Developers.
#
# OSN freehttpd is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OSN freehttpd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.

if [ -z "$BINDIR" ]; then
    BINDIR=../build/bin
fi

"$BINDIR/freehttpd" > freehttpd.log 2>&1 &

pid=$!

trap 'kill -0 $pid > /dev/null 2>&1 && kill -9 $pid && ps aux | grep freehttpd | awk "{ print \$2 }" | xargs kill -9' EXIT
trap "exit 1" INT TERM

echo "Starting stress test in 2 seconds..."
sleep 2

siege -b -c 50 -t 10s --no-parser http://127.0.0.1:8080/

if [ $? -ne 0 ]; then
    echo "Benchmark failed"
    exit 1
fi

echo "Benchmark completed successfully"

kill -9 $pid
ps aux | grep freehttpd | awk '{ print $2 }' | xargs kill -9

echo "Benchmark completed successfully"
echo "Check freehttpd.log for details"
