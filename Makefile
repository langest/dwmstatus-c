LIBS = -lX11 -lasound
FLAGS = -g -std=c99 -pedantic -Wall -O0

dwmStatus: dwmstatus.c
	gcc -o dwmstatus dwmstatus.c $(FLAGS) $(LIBS)

debug:
	gcc -o dwmstatus dwmstatus.c $(FLAGS) $(LIBS) -DDEBUG

clean:
	rm dwmstatus
