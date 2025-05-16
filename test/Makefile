# These rules assume `./webserv test.conf` is running

.PHONY: ddos
ddos: ddos.c
	cc -Wall -Wextra -Werror $^ -o ddos
	./ddos 127.0.0.1 8010

.PHONY: slow
slow: slow.c
	cc -Wall -Wextra -Werror $^ -o slow
	./slow 127.0.0.1 8010

.PHONY: slowloris
slowloris:
	perl slowloris.pl -dns 127.0.0.1 -port 8080

.PHONY: bench
bench: bench.c
	cc -Wall -Wextra -Werror $^ -o bench
	./bench 127.0.0.1 8080
