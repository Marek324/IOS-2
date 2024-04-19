CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
FILE = proj2

.PHONY: clean zip

$(FILE): $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c
clean: 
	rm -f $(FILE).o $(FILE)

zip: $(FILE).c Makefile
	zip $(FILE).zip $(FILE).c Makefile
