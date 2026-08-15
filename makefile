# executable # 
BIN_NAME = test
RELEASE_NAME = METARmap
LIBS = curl m pthread rt
SLIBS = ../rpi_ws281x/libws2811.a
SRC_EXT = c
SRC := $(wildcard *.c)
OBJS := $(SRC:%.c=%.o)
DEPS := $(SRC:%.c=%.d)

DYNLINKS := $(LIBS:%=-l%)
CFLAGS := -I../rpi_ws281x -MMD -MP

$(BIN_NAME):$(OBJS)
	gcc -o $(BIN_NAME) $(OBJS) $(SLIBS) $(DYNLINKS)

%.o: %.c
	gcc $(CFLAGS) -c $< -x $@

-include $(DEPS)

.PHONY: clean run clearrun release
$(OBJS): $(SRC)
	gcc -c -I../rpi_ws281x $(SRC)


clean:
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(BIN_NAME)

run: $(BIN_NAME)
	sudo ./$(BIN_NAME) -c

clearrun: $(BIN_NAME)
	sudo ./$(BIN_NAME)

release: $(BIN_NAME)
	cp ./$(BIN_NAME) ./$(RELEASE_NAME)
