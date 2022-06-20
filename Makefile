CFLAGS?=-O3
CFLAGS+=-MMD -MP
LDFLAGS?=
STATIC_LIB=libtev.a
LIB_SRC=tev.c
TEST_SRC=test.c $(LIB_SRC)

LIB_MAP=map/libmap.a
LIB_HEAP=cHeap/libheap.a
LIBS=$(LIB_HEAP) $(LIB_MAP)

all:test lib

test:$(patsubst %.c,%.o,$(TEST_SRC)) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^

lib:$(STATIC_LIB)

$(STATIC_LIB):$(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	for lib in $(LIBS); do \
		$(AR) -x $$lib; \
	done
	$(AR) -rcs -o $@ *.o

$(LIB_HEAP):
	$(MAKE) -C cHeap lib

$(LIB_MAP):
	$(MAKE) -C map lib

%.o:%.c
	$(CC) $(CFLAGS) -c $<

-include $(TEST_SRC:.c=.d)

clean:
	$(MAKE) -C cHeap clean
	$(MAKE) -C map clean
	rm -f *.o *.d test $(STATIC_LIB) 
