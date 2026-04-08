advanced multi-client chat application built using socket programming written in C++. The System supports Simultaneous non-blocking communication using the TCP and UDP protocols reliably with speed and can handle different types of data. (messages, files, audio, video)

# Features
⦁	User Authentication
⦁	Private Messaging
⦁	File Sharing
⦁	User Status Management
⦁	Persistent Message History (last 100 messages)
⦁	Real Time Join/leave notifications over UDP

Both TCP and UDP were used in this application. UDP (port 8081) used when messages were to be sent quickly and TCP (port 8080) for reliable communication, both having separate servers that handle requests from clients and forward those requests. The Client has socket connections to both servers. 

Feature			        TCP 	  UDP
Connection Setup		Yes			No
Authentication		  Yes			No
Private Messages		Yes			No
File Transfers		  Yes			No
Broadcast Messages	No			Yes
Status Updates		  No			Yes
Join/Leave Alerts		No			Yes

The servers handle receiving and sending messages via a multithreading approach. Each client socket is also recorded as a separate thread allowing for handling of multiple clients concurrently.

The Client also uses multithreading in a similar fashion so that read and write functions can occur simultaneously instead of one after the other.

Client-Side Workflow
1.	Connect to TCP server on port 8080
2.	Authenticate with username and password
3.	Register UDP address with UDP server (CONNECT: message)
4.	Send join notification over UDP
5.	Start TCP send/receive threads and UDP receive thread
6.	Use commands (/msg, /file, /status, /history, /help, /exit)
7.	On exit: send leave notification, close both sockets

Server-Side Workflow
1.	TCP server listens on port 8080, UDP server on port 8081
2.	Authenticate incoming TCP connections against user list
3.	Spawn a thread per client for concurrent handling
4.	Route messages: broadcast via UDP, private messages via TCP
5.	Log all connections and disconnections with timestamps
6.	Handle disconnections: mark user as logged out, release resources
