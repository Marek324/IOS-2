CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
LDFLAGS = -pthread -lrt
FILE = proj2

.PHONY: clean zip

$(FILE): $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c $(LDFLAGS)

clean: 
	rm -rf $(FILE) $(FILE).zip

zip: $(FILE).c Makefile
	zip $(FILE).zip $(FILE).c Makefile

