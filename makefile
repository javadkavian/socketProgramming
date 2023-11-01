CC     = gcc
CFLAGS =

PATH_SRC = ./src
PATH_INC = $(PATH_SRC)/include
PATH_OBJ = ./obj
PATH_LOG = ./log

OUT_CUSTOMER = customer
OUT_RESTAURANT = restaurant
OUT_SUPPLYER = supplyer
#----------------------------------------------
vpath %.c $(PATH_SRC)
vpath %.h $(PATH_INC)

OBJS_CUSTOMER = customer.o log.o user.o cJSON.o
OBJS_RESTAURANT = restaurant.o log.o user.o cJSON.o
OBJS_SUPPLYER = supplyer.o log.o user.o cJSON.o
#-----------------------------------------------

all: $(PATH_OBJ) $(PATH_LOG) $(OUT_CUSTOMER) $(OUT_RESTAURANT) $(OUT_SUPPLYER)

$(OUT_RESTAURANT): $(addprefix $(PATH_OBJ)/, $(OBJS_RESTAURANT))
	$(CC) $(CFLAGS) -o $@ $^


$(OUT_CUSTOMER): $(addprefix $(PATH_OBJ)/, $(OBJS_CUSTOMER))
	$(CC) $(CFLAGS) -o $@ $^



$(OUT_SUPPLYER): $(addprefix $(PATH_OBJ)/, $(OBJS_SUPPLYER))
	$(CC) $(CFLAGS) -o $@ $^

$(PATH_OBJ)/customer.o: customer.c log.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PATH_OBJ)/restaurant.o: restaurant.c log.h cJSON.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PATH_OBJ)/supplyer.o: supplyer.c log.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PATH_OBJ)/log.o: log.c log.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PATH_OBJ)/user.o: user.c user.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(PATH_OBJ)/cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c -o $@ $<	


$(PATH_OBJ): ; mkdir -p $@
$(PATH_LOG): ; mkdir -p $@

.PHONY: all clean

clean:
	rm -rf $(PATH_OBJ) $(PATH_LOG) $(OUT_CUSTOMER) $(OUT_RESTAURANT) $(OUT_SUPPLYER) > /dev/null 2>&1