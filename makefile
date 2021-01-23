# executable # 
BIN_NAME = test
RELEASE_NAME = METARmap
LIBS = curl m pthread rt
SLIBS = ../rpi_ws281x/libws2811.a
SRC_EXT = c
SRC := $(wildcard *.c)
OBJS := $(SRC:%.c=%.o)

DYNLINKS := $(LIBS:%=-l%)

$(BIN_NAME):$(OBJS)
	gcc -o $(BIN_NAME) $(OBJS) $(SLIBS) $(DYNLINKS)

$(OBJS): $(SRC)
	gcc -c -I../rpi_ws281x $(SRC)


clean:
	rm $(OBJS)
	rm $(BIN_NAME)

run: $(BIN_NAME)
	sudo ./$(BIN_NAME) -c
	
clearrun: $(BIN_NAME)
	sudo ./$(BIN_NAME)
	
release: $(BIN_NAME)
	cp ./$(BIN_NAME) ./$(RELEASE_NAME)
