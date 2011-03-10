
all: sample

sample:
	gcc -std=gnu99 -Wall -Wextra -Werror -O0 -lm -o $@ $@.c

.PHONY : clean
clean:
	rm -f sample
