main: main.c
	gcc -o main main.c

install: main
	cp main /usr/local/bin/c3extractor

clean:
	rm main
