#include <cctype>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <deque>
#include <string>
#include <iostream>
#include <algorithm>

#include "../TCP_Version/helper.h"

using namespace std;

// Global variables needed for UDP server
vector<user> users;
deque<Message> message_history;
int server_socket;

void broadcastUDP(const string& message) {
  for (auto& user : users) {
    if (user.logged) {
      sendto(server_socket, message.c_str(), message.size(), 0,
                   (struct sockaddr*)&user.udp_addr, user.udp_addr_len);
    }
  }
} 

void update_user_status(const string& username, const string& status) {

    string status_lower = status;
    transform(status_lower.begin(), status_lower.end(), status_lower.begin(), ::tolower);
  
    for (auto& user : users) {
        if (user.username == username) {
          if (status_lower == "away" || status_lower == "online" || status_lower == "busy" ) {
            user.status = status;
            user.last_activity = time(0);
            
            // Broadcast status change via UDP
            string status_msg = "[STATUS] " + username + " is now " + status;
            broadcastUDP(status_msg);
            break;
          }
          else {
            string err_msg = "[ERROR] Invalid Status cannot set status to " + status;
            sendto(server_socket, err_msg.c_str(), err_msg.size(), 0,
                   (struct sockaddr*)&user.udp_addr, user.udp_addr_len);            
          }
        }
    }
}

int main() {
  char buffer[1025];

  struct sockaddr_in server_address, client_address;
  socklen_t client_len = sizeof(client_address);

  // Step 1: Create a UDP socket
  server_socket = socket(AF_INET, SOCK_DGRAM, 0);

  // Step 2: Define server address
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(8081); // Port 8081
  server_address.sin_addr.s_addr = INADDR_ANY; // Listen on any interface

  // Initialize users
  users.push_back({"user", "1234", false, "online", 0, {}, 0, 0});
  users.push_back({"hello", "hi", false, "online", 0, {}, 0, 0});
  users.push_back({"azlan", "khan", false, "online", 0, {}, 0, 0});
  
  cout << "UDP Server running on port 8081..." << endl;

  // Step 3: Bind the socket to the IP and port
  bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));

    while (true) {
      int n = recvfrom(server_socket, buffer, sizeof(buffer), 0,
                      (struct sockaddr*)&client_address, &client_len);
      
      if (n > 0) {
          buffer[n] = '\0';
          string msg(buffer);
          
          // Handle different UDP message types
          if (msg.find("STATUS:") == 0) {
              // Handle status updates from TCP server
              size_t first_colon = msg.find(':', 7);
              string username = msg.substr(7, first_colon - 7);
              string status = msg.substr(first_colon + 1);
              
              update_user_status(username, status);
              broadcastUDP(msg);
          } else if (msg.find("CONNECT:") == 0) {
              // Handle UDP client registration
              string username = msg.substr(8);
              for (auto& user : users) {
                  if (user.username == username) {
                      user.udp_addr = client_address;
                      user.udp_addr_len = client_len;
                      user.logged = true;
                      log("[UDP] Registered " + username + " for broadcasts");
                      break;
                  }
              }
          } else {
              // Regular public chat message
              broadcastUDP(msg);
          }
      }
  }
    

  // Step 6: Close the socket
  close(server_socket);

  return 0;
}
