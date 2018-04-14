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

ZIP_FILE := t03g05_block_6.zip
TARGET := client
CC := gcc
CFLAGS := -std=gnu11

$(TARGET):
	$(CC) -o $@ $(WARNINGS) $(CFLAGS) $@.c

run: $(TARGET)
	./$(TARGET)

zip: clean
	zip $(ZIP_FILE) Makefile README $(TARGET).c

clean:
	$(RM) $(TARGET) $(ZIP_FILE)

.PHONY: clean zip client run
