default: build

build: clean
	gcc -o bin/clotemp src/main.c lib/cJSON.c -Ilib -lcurl

clean:
	rm -f bin/clotemp

demo: build
	./bin/clotemp