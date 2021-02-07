all: master slave sender

master: master/Makefile
	cd master && make -j8

upload-master: master
	cd master && make upload && make monitor
	
slave: slave/Makefile
	cd slave && make -j8

sender: sender/Makefile
	cd sender && NODE_ID=100 make -j8

master/Makefile sender/Makefile slave/Makefile:
	cp $@-debian $@

clean:
	cd master && make clean
	cd slave && make clean
	cd sender && make clean
	rm -rf tests/build

test:
	cd tests && ./test.sh

.PHONY: master slave sender clean