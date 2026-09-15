
include ../config.mk

APP  = gmrun
# ls *.c | sed 's%\.c%.o%g' | tr '\n' ' ' >> Makefile
OBJS = completion.o compline.o config_prefs.o history.o main.o util.o

#CPPFLAGS += -I..
CFLAGS += -I..

CFLAGS += $(IUP_CFLAGS)
LIBS   += $(IUP_LIBS)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(APP) $(LDFLAGS) $(LIBS)

strip: $(APP)
	$(STRIP) $(APP)

clean:
	rm -f *.o *~ $(APP) $(APP).exe

distclean: clean

install: $(APP)
	mkdir -p $(DESTDIR)$(bindir)
	install -c $(APP) $(DESTDIR)$(bindir)

install-strip: strip install

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(APP)

