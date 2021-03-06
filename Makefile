CC = occ
CFLAGS = -w-1 -O-1

JSONTEST_OBJS = jsontest.a json.a jsonutil.a
JSONTEST_PROG = jsontest

HTTPTEST_OBJS = httptest.a hostname.a http.a readtcp.a seturl.a strcasecmp.a tcpconnection.a urlparser.a
HTTPTEST_PROG = httptest

DISKBROWSER_OBJS = diskbrowser.a browserevents.a browserwindow.a browserutil.a diskmount.a disksearch.a hostname.a http.a json.a jsonutil.a readtcp.a seturl.a strcasecmp.a tcpconnection.a urlparser.a netdiskerror.a asprintf.a
DISKBROWSER_RSRC = diskbrowser.rez
DISKBROWSER_PROG = DiskBrowser

PROGS = $(JSONTEST_PROG) $(HTTPTEST_PROG) $(DISKBROWSER_PROG)

.PHONY: default
default: $(PROGS)

$(JSONTEST_PROG): $(JSONTEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(HTTPTEST_PROG): $(HTTPTEST_OBJS)
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
