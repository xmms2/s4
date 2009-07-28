src = be.c strtable.c intpair.c
obj = $(addsuffix .o, $(basename $(src)))

all: $(backfile)

$(backfile): $(obj)
	ld -r $(obj) -o $(backfile)

$(obj): %.o : %.c
	gcc -c $(CFLAGS) $< -o $@

clean:
	rm *.o
