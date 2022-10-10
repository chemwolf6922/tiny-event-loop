CFLAGS?=-O3
override CFLAGS+=-MMD -MP
LDFLAGS?=
STATIC_LIB=libtev.a
LIB_SRC=tev.c
TEST_SRC=test.c
ALL_SRC=$(TEST_SRC) $(LIB_SRC)

LIB_MAP=map/libmap.a
LIB_HEAP=heap/libheap.a
LIBS=$(LIB_HEAP) $(LIB_MAP)

all:test lib

test:$(patsubst %.c,%.oo,$(TEST_SRC)) $(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^

lib:$(STATIC_LIB)

$(STATIC_LIB):$(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	for lib in $(LIBS); do \
		$(AR) -x $$lib; \
	done
	$(AR) -rcs -o $@ *.o

$(LIB_HEAP):
	$(MAKE) -C heap lib

$(LIB_MAP):
	$(MAKE) -C map lib

%.oo:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:%.c
	$(CC) $(CFLAGS) -c $<

-include $(TEST_SRC:.c=.d)

clean:
	$(MAKE) -C heap clean
	$(MAKE) -C map clean
	rm -f *.oo *.o *.d test $(STATIC_LIB) 
