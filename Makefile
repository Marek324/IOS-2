CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
FILE = proj2

.PHONY: clean zip memtest test 

$(FILE): $(FILE).c
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c

clean: 
	rm -rf $(FILE) $(FILE).zip

zip: $(FILE).c Makefile
	zip $(FILE).zip $(FILE).c Makefile

memtest:
	rm -rf $(FILE)
	$(CC) $(CFLAGS) -g -o $(FILE) $(FILE).c
	valgrind -s --leak-check=full --show-leak-kinds=all ./$(FILE) 5 4 10 4 5
	rm -rf $(FILE)
	$(CC) $(CFLAGS) -o $(FILE) $(FILE).c


test: $(FILE)
	./$(FILE) 8 4 10 4 5
	./kontrola-vystupu.sh < $(FILE).out
	rm -f $(FILE).out

