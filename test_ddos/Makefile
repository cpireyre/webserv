.PHONY: ddos
ddos: ddos.c
	cc -Wall -Wextra -Werror $^ -o ddos
	./ddos 127.0.0.1 8010

.PHONY: slow
slow: slow.c
	cc -Wall -Wextra -Werror $^ -o slow
	./slow 127.0.0.1 8010
