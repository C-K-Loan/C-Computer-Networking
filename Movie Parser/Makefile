CFLAGS ?= -g -std=gnu11

ZIP_FILE := t03g05_block_3_2.zip
OBJS := client.o
TARGET := client

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

clean:
	$(RM) client client.o $(ZIP_FILE)

zip: clean
	zip $(ZIP_FILE) Makefile client.c README
