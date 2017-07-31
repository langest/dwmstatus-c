dwmStatus: dwmstatus.c
	gcc -o dwmstatus dwmstatus.c -Wall -std=c99 -lX11

debug:
	gcc -o dwmstatus dwmstatus.c -DDEBUG -Wall -std=c99 -lX11

clean:
	rm dwmstatus
