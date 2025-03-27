#!/bin/bash
./webserv web.conf &
until nc -z localhost 4242 2>/dev/null; do
  sleep 0.1
done
echo -e "Hello, world!" | nc localhost 4242
pkill webserv
