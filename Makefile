CC = occ
CFLAGS = -w-1 -O-1

TEST_OBJS = test.a json.a jsonutil.a
TEST_PROG = test

DISKBROWSER_OBJS = diskbrowser.a
DISKBROWSER_RSRC = diskbrowser.rez
DISKBROWSER_PROG = DiskBrowser

PROGS = $(TEST_PROG) $(DISKBROWSER_PROG)

.PHONY: default
default: $(PROGS)

$(TEST_PROG): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(DISKBROWSER_PROG): $(DISKBROWSER_OBJS) $(DISKBROWSER_RSRC)
	$(CC) $(CFLAGS) -o $@ $(DISKBROWSER_OBJS)
	occ $(DISKBROWSER_RSRC) -o $@
	iix chtyp -t ldf -a 1 $@

.PHONY: clean
clean:
	rm -f $(PROGS) *.a *.o *.root *.A *.O *.ROOT

%.a: %.c *.h
	$(CC) $(CFLAGS) -c $<

%.a: %.asm
	$(CC) $(CFLAGS) -c $<
