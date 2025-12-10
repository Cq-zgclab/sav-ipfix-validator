# Makefile for C Hackathon SAV IPFIX Project
# Dynamic IPv4/IPv6 support with five-tuple

CC = gcc
CFLAGS = -Wall -Wextra -g -I./include -I/usr/local/include/fixbuf $(shell pkg-config --cflags glib-2.0)
LDFLAGS = -L/usr/local/lib -lfixbuf $(shell pkg-config --libs glib-2.0)

# Source files
SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)

# Executables
TARGETS = $(SRC_DIR)/generate_test_data $(SRC_DIR)/sav_record_parser

.PHONY: all clean test

all: $(TARGETS)

$(SRC_DIR)/generate_test_data: $(SRC_DIR)/generate_test_data.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(SRC_DIR)/sav_record_parser: $(SRC_DIR)/sav_record_parser.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -ljansson

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o $(TARGETS)
	rm -f test_data/*.ipfix
	rm -f web/data.json

test: $(SRC_DIR)/generate_test_data
	./$(SRC_DIR)/generate_test_data test_data/sample_sav.ipfix
	ipfixDump --in test_data/sample_sav.ipfix

help:
	@echo "Makefile for C Hackathon SAV IPFIX"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build all executables"
	@echo "  clean   - Remove build artifacts"
	@echo "  test    - Generate test data and display with ipfixDump"
	@echo "  help    - Show this help message"
