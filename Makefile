CFLAGS += -fPIC -I/usr/local/include/lua53 -I/usr/local/include -g
LFLAGS += -shared -L/usr/local/lib -pthread
LIBS   += -lmdbx 

all: mdbx.so

mdbx.so: luamdbx.c
	$(CC) luamdbx.c -o mdbx.so $(CFLAGS) $(LFLAGS) $(LIBS)

clean:
	rm -f mdbx.so
