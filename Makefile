IDIR =../include
CC=gcc
CFLAGS=-I$(IDIR) -O 

ODIR=.
LDIR =../lib

LIBS=-lpthread -lanl

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = lptstatusleds.o 
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

prefix=/usr/local

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lptstatusleds: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
	
install:
	service lptstatusleds stop
	install -m 0755 lptstatusleds $(prefix)/sbin
	install -m 0755 lptstatusleds.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable lptstatusleds
	service lptstatusleds start

uninstall:
	service lptstatusleds stop
	systemctl daemon-reload
	rm -f $(prefix)/sbin/lptstatusleds
	rm -f /etc/systemd/system/lptstatusleds.service
