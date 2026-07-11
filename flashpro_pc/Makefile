CC=gcc
FLAGS=-std=gnu99 -Wall -O3
LIB=

NAME=flashpro

.PHONY:
	clean all

${NAME}: ${NAME}.c command.c util.c string.c error.c state.c
	${CC} ${FLAGS} -o ${NAME} ${NAME}.c command.c util.c string.c error.c state.c ${LIB}

clean:
	rm ${NAME}
