all:	
	${MAKE} -C sensd
	${MAKE} -C seltag
clean:
	${MAKE} -C sensd clean
	${MAKE} -C seltag clean
install:
	${MAKE} -C sensd install
	${MAKE} -C seltag install
