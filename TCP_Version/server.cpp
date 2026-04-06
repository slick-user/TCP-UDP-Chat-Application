#include <csignal>
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <deque>
#include <fstream>
#include <ctime>
#include <arpa/inet.h>
#include <thread>
#include <mutex>

#include "helper.h"

using namespace std;

bool Authenticate(int sock, user*& u);
void add_to_history(const string& sender,
                    const string& message, bool is_private, const string& recipient);

void sendPrivateMsg(const string& sender,
                    const string& recipient, const string& message);

// File Transfers
string base64_encode(const string& input);

void show_help(int sock);

// SUPPORT FOR MULTIPLE CONCURRENT CONNECTIONS (DONE)
// BROADCASTING TO ALL USERS (DONE)
// PRIVATE MESSAGING (DONE)
// FILE TRANSFERS (DONE)
// LOG USERS CONNECTIONS/DISCONNECTIONS WITH TIMESTAMPS (DONE)
// USER STATUS (ONLINE|AWAY|BUSY) (DONE)
// MESSAGE HISTORY (DON'T SHOW OTHERS PRIVATE MESSAGES) (DONE)

/* COMMANDS
  /msg <username> "Message" (DONE)
  /file <filepath> (optional) <username> (DONE)
  /status <username> (DONE)
  /history (optional) <username> (DONE)
*/ 

// double ended queue data structure for the message history
deque<Message> message_history;

vector<user> users;
mutex users_mutex;
mutex history_mutex;

// global state - no longer needed with simplified architecture

// Authenticate
bool Authenticate(int sock, user*& u) {
  string buffer;

  sendString(sock, "Login\nInput Username: ");
  buffer = recvString(sock);

  lock_guard<mutex> lock(users_mutex);
  for (auto &User : users) {
    if (User.username == buffer) {
      string us = buffer;
      sendString(sock, "Input Password:");
      buffer = recvString(sock);
      string p = buffer;

      if (User.password == buffer) {
        sendString(sock, "Successfully Logged in!");
        User.logged = true;
        User.tcp_socket = sock;  // Store the socket in user struct
        u = &User;
        return true;
      } else {
        sendString(sock, "Wrong Password!");
        return false;
      }
    }
  }

  sendString(sock, "No Such User Exists!");
  return false;
}

void add_to_history(const string& sender, const string& message,
                    bool is_private = false, const string& recipient = "") {
  Message msg;
  msg.sender = sender;
  msg.content = message;
  msg.timestamp = time(0);
  msg.is_private = is_private;
  msg.recipient = recipient;
  
  lock_guard<mutex> lock(history_mutex);
  message_history.push_back(msg);
  if (message_history.size() > 100) {
      message_history.pop_front();
  }
}

void sendPrivateMsg(const string& sender, const string& recipient, const string& message) {
  int recipient_sock = -1;
  int sender_sock = -1;
  
  lock_guard<mutex> lock(users_mutex);
  for (auto& user : users) {
    if (user.logged && user.username == recipient) recipient_sock = user.tcp_socket; 
    if (user.logged && user.username == sender) sender_sock = user.tcp_socket;
  }
  
  if (recipient_sock != -1) {
    string private_msg = "[PRIVATE] " + sender + ": " + message;
    sendString(sender_sock, private_msg);
    sendString(recipient_sock, private_msg);
    add_to_history(sender, message, true, recipient);
  } 
  else {
    if (sender_sock != -1) {
      sendString(sender_sock, "[SERVER] User " + recipient + " is not online");
    }
  }
}
  

string base64_encode(const string& input) {
  const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  string encoded;
  int val = 0, valb = -6;
  
  for (unsigned char c : input) {
      val = (val << 8) + c;
      valb += 8;
      while (valb >= 0) {
          encoded.push_back(chars[(val >> valb) & 0x3F]);
          valb -= 6;
      }
  }
  
  if (valb > -6) {
      encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  
  while (encoded.size() % 4) {
      encoded.push_back('=');
  }
  
  return encoded;
}
 
void send_file(const string& sender, const string& recipient, const string& filepath) {
  int recipient_sock = -1;
  int sender_sock = -1;

  lock_guard<mutex> lock(users_mutex);
  for (auto& user : users) {
    if (user.logged && user.username == recipient) recipient_sock = user.tcp_socket; 
    if (user.logged && user.username == sender) sender_sock = user.tcp_socket;
  }

  if (recipient_sock  == -1) {
    if (sender_sock != -1)
      sendString(sender_sock, "[SERVER] User " + recipient + "is not online");
  }
  
  // send file in chunks
  ifstream file(filepath, ios::binary);
  if (!file.is_open()) {
    if (sender_sock != -1)
      sendString(sender_sock, "[SENDER] Could not open file: " + filepath);
    return;
  }
    
  string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
  string encoded = base64_encode(content);
    
  sendString(recipient_sock, "[FILE_START] " + sender + " is sending a file");
        
  // Send in chunks
  const int chunk_size = 1000;
  for (size_t i = 0; i < encoded.size(); i += chunk_size) {
    string chunk = encoded.substr(i, chunk_size);
    sendString(recipient_sock, "[FILE_CHUNK] " + chunk);
    usleep(1000);
  }
  sendString(recipient_sock, "[FILE_END]");

  if (sender_sock != -1)
    sendString(sender_sock, "[SERVER] File sent to " + recipient);
}

void show_help(int sock) {
  string help = "=== Commands ===\n"
                "/msg <username> <message> - Send private message\n"
                "/file <username> <filepath> - Send file\n"
                "/status <status> - Update your status (online/away/busy)\n"
                "/history [count] - Show message history\n"
                "/help - Show this help message\n";
  sendString(sock, help);
}

void show_message_history(int sock, int count, user& u) {
  string response = "=== Message History ===\n";

  lock_guard<mutex> lock(history_mutex);
  int start = max(0, (int)message_history.size() - count);
  for (int i = start; i < message_history.size(); i++) {
      const auto& msg = message_history[i];
      if (msg.is_private && (u.username == msg.sender || u.username == msg.recipient)) {
        response += "[PRIVATE] " + msg.sender + " -> " + msg.recipient + ": " + msg.content + "\n";
      } else {
        if (msg.is_private) continue;    // JUST SKIP THE MESSAGE
        response += msg.sender + ": " + msg.content + "\n";
      }
  }
  sendString(sock, response);
}

// Child chat loop -> sends messages to parent via pipe
// need to add the functionality to allow for inputting commands
void Chat(int sock, user& u) {
  string buffer;

  while (1) {
    buffer = recvString(sock);
    if (buffer.empty()) break; // client disconnected

    u.last_activity = time(0);

    // reading of commands
    if (buffer[0] == '/') {
      Command cmd = parseCommand(buffer);

      // PRIVATE MESSAGING
      if (cmd.type == "/msg") {
        if (cmd.args.size() >= 2) {
          string recipient = cmd.args[0];
          string message = cmd.args[1];
          for (int i=2; i < cmd.args.size(); i++) {
            message += " " + cmd.args[i];
          }
          cout << message << endl;
          sendPrivateMsg(u.username, recipient, message);
        }
      } else if (cmd.type == "/file") {
        if (cmd.args.size() >= 2) {
          string recipient = cmd.args[0];
          string filepath = cmd.args[1];
          send_file(u.username, recipient, filepath);
        }
      } else if (cmd.type == "/status") {
        // STATUS UPDATE - Send to UDP server
        if (cmd.args.size() >= 1) {
          string status_msg = "STATUS:" + u.username + ":" + cmd.args[0];
          // Send to UDP server via UDP socket
          struct sockaddr_in udp_addr;
          udp_addr.sin_family = AF_INET;
          udp_addr.sin_port = htons(8081);
          udp_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Send to localhost
          
          int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
          sendto(udp_sock, status_msg.c_str(), status_msg.size(), 0,
                 (struct sockaddr*)&udp_addr, sizeof(udp_addr));
          close(udp_sock);
        }
      } else if (cmd.type == "/history") {
        // MESSAGE HISTORY (TCP)
        int count = 10;
        if (cmd.args.size() >= 1) {
          count = stoi(cmd.args[0]);
        }
        show_message_history(sock, count, u);
      } else if (cmd.type == "/help") {
        show_help(sock);
      } else if (cmd.type == "/exit") {
        sendString(sock, "[SERVER] Goodbye " + u.username + "!");
        break;
      }
      continue;
    }

    // Broadcast Message
    string n = u.username + ": " + buffer;
    // Send public message to UDP server for broadcasting
    struct sockaddr_in udp_addr;
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(8081);
    udp_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Send to localhost
    
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(udp_sock, n.c_str(), n.size(), 0,
           (struct sockaddr*)&udp_addr, sizeof(udp_addr));
    close(udp_sock);
    
    add_to_history(u.username, buffer);
  }

  lock_guard<mutex> lock(users_mutex);
  u.logged = false;
  log("[TCP] " + u.username + " disconnected");
  // cout << "[TCP] " << u.username << " disconnected" << endl;
}

int main() {

  users.push_back({"user", "1234", false, "online", 0, {}, 0, 0});
  users.push_back({"hello", "hi", false, "online", 0, {}, 0, 0});
  users.push_back({"azlan", "khan", false, "online", 0, {}, 0, 0});

// INITIALIZATION
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(8080);
  server_address.sin_addr.s_addr = INADDR_ANY;

  bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
  listen(server_socket, 5);

  cout << "TCP Server running on port 8080..." << endl;
  
  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
    if (client_socket < 0) continue;

    thread([client_socket]() {
      user* u = nullptr;
      if (!Authenticate(client_socket, u)) {
        close(client_socket);
        return;
      }
      log("[TCP] " + u->username + " connected"); 
      // cout << "[TCP] " << u->username << " connected" << endl;

      Chat(client_socket, *u);

      close(client_socket);
    }).detach();
  }

  close(server_socket);
  return 0;
}
