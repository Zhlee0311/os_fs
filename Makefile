CC = gcc 
CFLAGS = -Wall -Wextra -g
TARGET = myfs
SRC = myfs.c


$(TARGET) : $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -r $(TARGET)