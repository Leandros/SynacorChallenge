CC 			= gcc
LDFLAGS 	=
CFLAGS 		= -pedantic -std=c99 -Wall -DNDEBUG

RM 			= rm
RMFLAGS 	= -f

OUT 		= vm
SRCS 		= main.c
OBJS 		= $(SRCS:.c=.o)

.PHONY: clean


vm: $(OBJS)
	$(CC) $(CFLAGS) -o $(OUT) $<

clean:
	$(RM) $(RMFLAGS) $(OUT)
	$(RM) $(RMFLAGS) $(OBJS)
