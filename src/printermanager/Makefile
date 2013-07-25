SRC := printermanager.c
OBJ := printermanager.o
EXEC := ManageConnection.bin
CFLAGS += -Wall

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(EXEC)
	rm -f $(OBJ)
