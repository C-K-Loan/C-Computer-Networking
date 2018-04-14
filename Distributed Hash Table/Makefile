WARNINGS := -Wall -pedantic -pedantic-errors -Wextra -Waggregate-return -Wcast-align \
	-Wcast-qual  -Wchar-subscripts  -Wcomment -Wconversion \
	-Wdisabled-optimization -Wfloat-equal  -Wformat  -Wformat=2 \
	-Wformat-nonliteral -Wformat-security  -Wformat-y2k \
	-Wimplicit  -Wimport  -Winit-self  -Winline -Winvalid-pch   \
	-Wunsafe-loop-optimizations  -Wlong-long -Wmissing-braces \
	-Wmissing-field-initializers -Wmissing-format-attribute   \
	-Wmissing-include-dirs -Wmissing-noreturn -Wpacked  -Wpadded \
	-Wparentheses  -Wpointer-arith -Wredundant-decls -Wreturn-type \
	-Wsequence-point  -Wshadow -Wsign-compare  -Wstack-protector \
	-Wstrict-aliasing -Wstrict-aliasing=2 -Wswitch  -Wswitch-default \
	-Wswitch-enum -Wtrigraphs  -Wuninitialized -Wunknown-pragmas \
	-Wunreachable-code -Wunused -Wunused-function  -Wunused-label \
	-Wunused-parameter -Wunused-value  -Wunused-variable  -Wvariadic-macros \
	-Wvolatile-register-var -Wwrite-strings

CFLAGS := -std=gnu99 -O -g
CC     := gcc

SRC_DIRS := ./
SRCS := hash_table.c server.c
OBJS := $(addsuffix .o,$(basename $(SRCS)))
TARGET := server
ZIP_FILE := t03g05_block_3_1.zip

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) $(WARNINGS)

test_server:
	gcc -g -o $@ $@.c

.PHONY: clean zip
clean:
	$(RM) $(OBJS) $(TARGET) $(ZIP_FILE)
zip: clean
	zip $(ZIP_FILE) Makefile hash_table.c hash_table.h server.c README
