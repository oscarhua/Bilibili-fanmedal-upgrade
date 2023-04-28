CC=g++ -g -Wall -std=c++17 -Werror -Wpedantic -Wextra -Wconversion

# List of source files for your file server
FS_SOURCES=test.cpp bilibili.cpp https.cpp

# Generate the names of the file server's object files
FS_OBJS=${FS_SOURCES:.cpp=.o}

all: bili

# Compile the file server and tag this compilation
bili: ${FS_OBJS}
	${CC} -o $@ $^ -lpthread -lssl -lcrypto -ldl

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f ${FS_OBJS} bili
