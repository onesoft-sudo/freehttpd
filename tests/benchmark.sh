#!/bin/sh

../build/bin/freehttpd > freehttpd.log 2>&1 &

pid=$!

trap 'kill -0 $pid > /dev/null 2>&1 && kill -TERM $pid' EXIT
trap "exit 1" INT TERM

echo "Starting stress test in 2 seconds..."
sleep 2

c="$(nproc)"

if [ "$c" -gt 24 ]; then
    c=16
fi

ab -c "$c" -n 60000 -m GET http://localhost:8080/

if [ $? -ne 0 ]; then
    echo "Apache Benchmark failed"
    exit 1
fi

echo "Apache Benchmark completed successfully"

kill -INT $pid
wait $pid

if [ $? -ne 0 ]; then
    echo "freehttpd encountered errors"
    exit 1
fi

echo "Benchmark completed successfully"
echo "Check freehttpd.log for details"
