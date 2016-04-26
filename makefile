send_data.o: send_data.c

	gcc  -Wall -pedantic -std=c99  send_data.c -lcurl  `mysql_config --cflags --libs` -o send_data

clean:

	rm -r send_data
