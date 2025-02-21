ssh-choose: ssh_choose.c
	$(CC) -o ssh-choose ssh_choose.c -lncurses

install: ssh-choose
	cp ./ssh-choose ~/bin/

.PHONY: install