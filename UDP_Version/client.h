#pragma once

#include <string>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "../TCP_Version/helper.h"

using namespace std;

struct sockaddr_in udp_address;

int UDPConnect(const string& username) {
  // Socket connection
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  udp_address.sin_family = AF_INET;
  udp_address.sin_port = htons(8081);
  udp_address.sin_addr.s_addr = INADDR_ANY;

  string connect_msg = "CONNECT:" + username;
  sendto(sock, connect_msg.c_str(), connect_msg.size(), 0,
         (sockaddr*)&udp_address, sizeof(udp_address));

  return sock;
}

void recvUDP(int sock, const string& username) {
  char buffer[1024];
  sockaddr_in sender;
  socklen_t len = sizeof(sender);

  while (1) {
    int n = recvfrom(sock, buffer, sizeof(buffer)-1, 0,
                     (sockaddr*)&sender, &len);
      
    buffer[n] = '\0';
    const char* pos = strchr(buffer, ':');
    if (pos != nullptr) {
      size_t p = pos - buffer;
      const string user(buffer, p);   // I did not know we can do this
      
      if (username != user) {
        cout << "[UDP] " << buffer << endl;
      }
    }
  }
}

// Notfiication on type 1: Join | 2: Leave
void Notify(int sock, const string& username, int type) {
  string Msg;

  switch(type) {
    case 1:
      Msg = "[NOTIFICATION] " + username + " Joined";
      sendto(sock, Msg.c_str(), Msg.size(),
             0, (struct sockaddr*) &udp_address, sizeof(udp_address));
      break;

    case 2:
      Msg = "[NOTIFICATION] " + username + " Left";
      sendto(sock, Msg.c_str(), Msg.size(),
             0, (struct sockaddr*) &udp_address, sizeof(udp_address));
      break;
  }
}
