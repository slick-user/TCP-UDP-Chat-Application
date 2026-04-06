#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <iostream>

using namespace std;

struct user {
  string username;
  string password;
  bool logged = false;
  string status;
  int tcp_socket;
  struct sockaddr_in udp_addr;
  socklen_t udp_addr_len;
  time_t last_activity;
};

struct Message {
  string sender;
  string content;
  time_t timestamp;
  bool is_private;
  string recipient;
};

string recvString(int sock) {
    char buffer[1024]; 
    int n = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) return ""; // connection closed or error
    buffer[n] = '\0';
    return std::string(buffer);
}

bool sendString(int sock, const string& msg) {
  int n = send(sock, msg.c_str(), msg.size(), 0);
  return n == (int)msg.size(); // return true if entire message sent 
}

struct Command {
    string type;
    vector<string> args;
};
 
Command parseCommand(const string& input) {
    Command cmd;
    istringstream iss(input);
    string token;
    
    iss >> cmd.type;
    while (iss >> token) {
        cmd.args.push_back(token);
    }
    return cmd;
}

static void log(const string& msg) {
  time_t now = time(0);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
  cout << "[" << buf << "] " << msg << endl;
}
