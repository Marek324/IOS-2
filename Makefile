CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
FILE = proj2

.PHONY: clean zip test

$(FILE): $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c

clean: 
	rm -rf $(FILE) $(FILE).zip

zip: $(FILE).c Makefile
	zip $(FILE).zip $(FILE).c Makefile

test: $(FILE)
	./$(FILE) 8 4 10 4 5 > $(FILE).out
	./kontrola-vystupu.sh < $(FILE).out
	rm -f $(FILE).out

