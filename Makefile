# Makefile to build monte_pi_sprng program
# --- macros
CC=cc
CFLAGS=  -O3 
OBJECTS= snake.o 
LIBS += -lpthread


# --- targets
all:    snake
snake:   $(OBJECTS) 
	$(CC)  -o snake $(OBJECTS) $(LIBS)
	
snake.o: 
	$(CC) $(CFLAGS) -c snake.c 


# --- remove binary and executable files
clean:
	rm -f snake $(OBJECTS)
