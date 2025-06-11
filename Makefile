CC = gcc

CFLAGS = -Wall -Wextra -g -lreadline

TARGET = shelly

SRCS = cmpsh.c

OBJS = $(SRCS:.c=.o)

LIBS = -lreadline

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)