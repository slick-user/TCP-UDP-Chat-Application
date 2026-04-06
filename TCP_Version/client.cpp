#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <fstream>

#include "helper.h"
#include "../UDP_Version/client.h"

using namespace std;

string username;
string password;

bool receiving_file = false;
ofstream file_output_stream;

// These Threads are for TCP recv and sending of packets
void receiveMessages(int sock);
void sendMessages(int sock);

void Authenticate(int sock);

// Threads allow for the sending and recieving to happen non-blocking (does not have to wait for a message to send a message)

// Thread to receive messages from server
void receiveMessages(int sock) {
    while (1) {
        string msg = recvString(sock);
        if (msg.empty()) break;

        size_t pos = msg.find(':');
        string sender = msg.substr(0, pos);

        if (sender != username)
            cout << msg << endl;
    }
}

// Thread to send messages to server
void sendMessages(int sock) {
    string input;

    while (1) {
        getline(cin, input);
        if (input.empty()) continue;

        sendString(sock, input);
    }
}

void Authenticate(int sock) {
    // Initial handshake
    cout << recvString(sock) << endl; // "Login Input Username:"
    
    string input;
    getline(cin, input);
    sendString(sock, input);

    cout << recvString(sock) << endl; // "Input Password:"

    username = input;
    
    getline(cin, input);
    sendString(sock, input);

    password = input;
    
    cout << recvString(sock) << endl; // login result
}

int main() {

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in tcp_address;
    tcp_address.sin_family = AF_INET;
    tcp_address.sin_port = htons(8080);
    tcp_address.sin_addr.s_addr = INADDR_ANY;

    connect(sock, (struct sockaddr*)&tcp_address, sizeof(tcp_address));

    Authenticate(sock);

    // Connect the UDP To ensure broadcast messaging
    int UDPsock = UDPConnect(username);

    // On Connection or on leaving notfications should trigger (joined, left)
    // Client Joined the Chat
    Notify(UDPsock,  username, 1);
    
    // Start chat threads (TCP)
    thread tcp_recv_thread(receiveMessages, sock);
    thread tcp_send_thread(sendMessages,    sock);

    // chat threads (UDP)
    thread udp_recv_thread(recvUDP, UDPsock, username);

    tcp_recv_thread.join();
    tcp_send_thread.join();
    udp_recv_thread.join();

    Notify(UDPsock, username, 2);
    close(sock);
    close(UDPsock);
    return 0;
}
