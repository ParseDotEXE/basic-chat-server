# COMP 206 - Fall 2024 - Homework 7

In this assignment, you'll build on the basic "chat" example we did in class to
actually enable **multiple participants** to chat together.

This will be challenging.

## Basic idea of a more realistic chat application

In class, the client sent strings to the server that simply displayed them on
the terminal.
Now, we'll have *multiple* clients connected to the same server.
The server will relay incoming messages from each client to all the other
clients.
This way, the clients can chat among each other.

## Recap: server-side

Recall the basic structure of the server program from class:
- Use `socket()` to allocate a file descriptor for the server's network
  connection.
    - Use `AF_INET` that we'll be using the Internet Protocol at the network layer.
    - Use `SOCK_STREAM` to specify that we'll use TCP.
- Use `bind()` to associate the socket to a particular address and port number.
- Use `listen()` to instruct the OS to begin accepting incoming connections.
	- These connections are held in an OS-level queue until our application
	  uses...
- `accept()` to pull a connection from the OS-level queue into our application.
	- This returns a _file descriptor,_ i.e. an integer that represents an
	  open file.
	- In this case, it is not a real file on disk in durable storage!
	  This file is an abstraction. It represents our connection, across the
	  internet, to the client that connected to us.
	- Writes to this file are actually sent across the internet to the
	  client process.

Then we used `fdopen()` to turn that client file descriptor into a `FILE*` that
we could then use with the ordinary stdio functions we're used to: fgets,
fprintf, etc.

## Recap: client-side

- Use `socket()` to allocate a file descriptor for the network connection.
- Prepare a `struct sockaddr_in` to hold the address we'll connect to.
- Use `connect()` to establish the connection to the server.

## Blocking IO: a problem

The call to `accept()` behaves very differently depending on whether there is a
pending connection or not.
- When there is a pending connection at the OS level, `accept()` returns right
  away.
- When there is no pending connection at the OS level, `accept()` **blocks.**
  That is, our program "freezes" at the point of the `accept()` call until a
  new connection arrives.

Moreover, `fgets()` has the same behaviour. If there is no data to read from
the given stream, then fgets blocks until data becomes available.
- With files on disk, we never witnessed this blocking behaviour, since reads
  from disk happen pretty much instantly .
- When using `fgets()` to read from stdin, however, we would see this:
  the `fgets()` call would block until the user finishes typing a line and
  presses enter.

These blocking operations present a serious challenge to our chat application,
as we would like the server program to accept new connections while
simultaneously monitoring all the connected clients to see who's sent a message
so that it might relay it to the others.

In other words, we have to deal with *concurrency,* the art of making computers
do multiple things at once.

## A solution: polling

The solution to this problem that you'll explore in this assignment is called
*polling.*

The idea is simple: check whether data is available before trying to read.
(Or check whether an incoming connection exists before trying to accept it.)

So the pseudocode for the server program that uses polling looks like:

```
loop forever:
	if there's an incoming connection:
		accept it
	for each connected client:
		if that client sent a message:
			redistribute it to the other clients
	wait a little bit
```

The client program will have the same concern; it must simultaneously:
- read from stdin to get a message from the user
- read from the socket to get any messages coming from the server

So its polling-based pseudocode will look like:

```
loop forever:
	if there's data on stdin:
		send it over the socket
	if there's data on the socket:
		print it to stdout
	wait a little bit
```

Notice that each of these loop ends with "wait a little bit".

CPUs are *very fast* so its important to include a wait in these kinds of
polling loops, lest we consume all available compute resources just spinning in
circles checking whether there's data to process.

## How to use polling in C

First, we need to enable the `O_NONBLOCK` setting on the file descriptors that
we want to poll. This is done using the `fcntl()` (file control) function.

For example, here's how we can set stdin to be nonblocking:

```c
#include <stdio.h>
#include <fcntl.h>

int main()
{
	fcntl(0, F_SETFL, O_NONBLOCK);
	//    ^  ^        ^ nonblocking
	//    |  +-- set flag
	//    +-- on file descriptor 0 (stdin)
}
```

Note that `fcntl()` is a low-level function. It deals in the raw integers of
file descriptors, rather than in the higher-level C library `FILE*` data
structures.

After this `fcntl()` call, `fgets(buf, sizeof(buf), stdin)` will return
_immediately_ instead of blocking if there is no data to read on stdin.

- If there was no data to read, then `fgets()` will _fail._
- It signals failure by returning `NULL`, just as we saw when it reaches
  end-of-file.
- It specifies the kind of failure by setting the global variable `errno`
  (error number).
- If the failure was that there was no data to read for a nonblocking
  operation, then `errno` is set to one of `EAGAIN` or `EWOULDBLOCK`.
  (These are special constants.) You need to check for both.

## Your task

Implement the functions given as prototypes in `server.c`.

Implement the body of `main` in `client.c` following the comments.

## Evaluation (3pts total)

- 1pt: at least one client can connect to the server, and messages it sends are received by the
  server
- 1pt: multiple clients can connect to the server, and their messages are relayed to the other
  clients
- 1pt: when the room is full, the server says so to the incoming client and closes their
  connection.
