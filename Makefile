LIBTEV_VERSION_MAJOR=1
LIBTEV_VERSION_MINOR=3
LIBTEV_VERSION_REVISION=4
LIBTEV_VERSION=$(LIBTEV_VERSION_MAJOR).$(LIBTEV_VERSION_MINOR).$(LIBTEV_VERSION_REVISION)

CFLAGS?=-O3
override CFLAGS+=-fPIC
_CFLAGS=$(CFLAGS)
_CFLAGS+=-MMD -MP
LDFLAGS?=

STATIC_LIB=libtev.a
SHARED_LIB=libtev.so
LIB_SRC=tev.c
TEST_SRC=test.c
ALL_SRC=$(TEST_SRC) $(LIB_SRC)

LIB_MAP=map/libmap.a
LIB_HEAP=heap/libheap.a
LIBS=$(LIB_HEAP) $(LIB_MAP)

all:test lib

test:$(patsubst %.c,%.oo,$(TEST_SRC)) $(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^

lib:$(STATIC_LIB) $(SHARED_LIB)

$(STATIC_LIB):$(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	for lib in $(LIBS); do \
		$(AR) -x $$lib; \
	done
	$(AR) -rcs $@ *.o

$(SHARED_LIB):$(patsubst %.c,%.o,$(LIB_SRC)) $(LIBS)
	for lib in $(LIBS); do \
		$(AR) -x $$lib; \
	done
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@.$(LIBTEV_VERSION_MAJOR) -o $@.$(LIBTEV_VERSION) *.o
	ln -sf $@.$(LIBTEV_VERSION) $@.$(LIBTEV_VERSION_MAJOR)
	ln -sf $@.$(LIBTEV_VERSION) $@

$(LIB_HEAP):
	$(MAKE) -C heap lib CFLAGS="$(CFLAGS)"

$(LIB_MAP):
	$(MAKE) -C map lib CFLAGS="$(CFLAGS)"

%.oo:%.c
	$(CC) $(_CFLAGS) -o $@ -c $<

%.o:%.c
	$(CC) $(_CFLAGS) -c $<

-include $(ALL_SRC:.c=.d)

clean:
	$(MAKE) -C heap clean
	$(MAKE) -C map clean
	rm -f *.oo *.o *.d test $(STATIC_LIB) $(SHARED_LIB)*
