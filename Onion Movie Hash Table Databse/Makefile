WARNINGS := -Wall -pedantic -Wextra -Waggregate-return -Wcast-align \
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

INCS := -I include
LIBS := -L lib/32 -L lib/64 -lonion_static -lrt -lm

SRCS   := block5.c hash_table.c film.c database.c cJSON.c
OBJS := $(addsuffix .o,$(basename $(SRCS)))

CFLAGS := -std=gnu99 -O -g $(WARNINGS) $(INCS) $(LIBS) -pthread

CC     := gcc
TARGET := block5
ZIP_FILE := t03g05_block_5.zip

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

run: all
	./block5

.PHONY: clean zip
clean:
	$(RM) $(OBJS) $(TARGET) $(ZIP_FILE)
zip: clean
	zip -r $(ZIP_FILE) Makefile hash_table.c hash_table.h film.h film.c cJSON.h cJSON.c database.h database.c filme.csv block5.c include lib test.py test.sh README
