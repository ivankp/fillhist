.PHONY: all clean

CC = gcc

# CFLAGS := -Wall -O3 -flto -fmax-errors=3 -Iinclude
CFLAGS := -Wall -O0 -g -fmax-errors=3 -Iinclude

PYTHON_INCLUDE := $(shell \
  python3 -c 'import sysconfig; print(sysconfig.get_paths()["include"])')

LIB := fillhist

$(LIB).so: CFLAGS += -isystem $(PYTHON_INCLUDE) -fPIC -fwrapv -ffast-math
$(LIB).so: LDFLAGS += -shared

all: $(LIB).so

$(LIB).so: %.so: %.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -fv $(LIB).so

