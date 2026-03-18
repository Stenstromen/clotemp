default: build

build: clean
	gcc -o bin/clotemp src/main.c lib/cJSON.c -Ilib -lcurl

clean:
	rm -f bin/clotemp

demo: build
	./bin/clotemp

init: build
	./bin/clotemp init

install: build
	sudo cp bin/clotemp /usr/local/bin/clotemp