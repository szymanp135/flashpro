CC=gcc
FLAGS=-std=gnu99 -Wall -O3
LIB=

NAME=flashpro

.PHONY:
	clean all

${NAME}: ${NAME}.c
	${CC} ${FLAGS} -o ${NAME} ${NAME}.c ${LIB}

clean:
	rm ${NAME}
