
CC     = cc
#CFLAGS = -g -w
CFLAGS = -O2 -Wall
INC    = -framework IOKit
PREFIX = /usr/local
EXEC   = keep-cool
LAUNCHD = /Library/LaunchDaemons
PLIST  = m.c.m.keepcool.plist

build : $(EXEC)

clean : 
	rm $(EXEC)
	rm -fr $(EXEC).dSYM
	rm -f $(PLIST)

$(EXEC) : keep-cool.c
	$(CC) $(CFLAGS) $(INC) -o $@ $?

$(PLIST) : $(EXEC)
	@echo "Generating deafult plist file"
	./$(EXEC) -g

install : $(EXEC) $(PLIST)
	install -d $(PREFIX)/sbin
	install $(EXEC) $(PREFIX)/sbin
	install -m 0644 $(PLIST) $(LAUNCHD)/$(PLIST)
	launchctl load -w $(LAUNCHD)/$(PLIST)

uninstall : $(EXEC)
	launchctl unload -w $(LAUNCHD)/$(PLIST)
	rm -f $(LAUNCHD)/$(PLIST)
	rm -f $(PREFIX)/sbin/$(EXEC)
