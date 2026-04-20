#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define PORT 8082

map<string, vector<SOCKET>> rooms;
mutex rooms_mutex;

map<string, SOCKET> users;
mutex users_mutex;

map<string, bool> userCheckResult;
mutex userCheckMutex;

SOCKET broker_sock;

void saveMessage(const string& room, const string& msg) {
    ofstream file(room + ".txt", ios::app);
    if (file.is_open()) {
        file << msg << endl;
    }
}

void sendHistory(SOCKET client, const string& room) {
    ifstream file(room + ".txt");
    if (!file.is_open()) return;

    string line;
    while (getline(file, line)) {
        send(client, line.c_str(), line.length(), 0);
    }
}

void sendToBroker(const string& msg) {
    send(broker_sock, msg.c_str(), msg.length(), 0);
}

void receiveFromBroker() {
    char buffer[1024];

    while (true) {
        int bytes = recv(broker_sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        string msg(buffer, bytes);

        if (msg.find("USER_OK:") == 0) {
            string name = msg.substr(8);
            lock_guard<mutex> lock(userCheckMutex);
            userCheckResult[name] = true;
        }

        else if (msg.find("USER_EXISTS:") == 0) {
            string name = msg.substr(12);
            lock_guard<mutex> lock(userCheckMutex);
            userCheckResult[name] = false;
        }

        else {
            size_t pos = msg.find('|');
            if (pos == string::npos) continue;

            string room = msg.substr(5, pos - 5);
            string actualMsg = msg.substr(pos + 1);

            saveMessage(room, actualMsg);

            lock_guard<mutex> lock(rooms_mutex);

            if (rooms.find(room) == rooms.end()) continue;

            for (SOCKET client : rooms[room]) {
                send(client, actualMsg.c_str(), actualMsg.length(), 0);
            }
        }
    }
}

void broadcastLocal(const string& room, const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(rooms_mutex);

    for (SOCKET client : rooms[room]) {
        if (client != sender) {
            send(client, msg.c_str(), msg.length(), 0);
        }
    }
}

void handleClient(SOCKET client_socket) {
    char buffer[1024];
    string username = "";
    string currentRoom = "";

    while (true) {
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        string msg(buffer, bytes);

        if (msg.find("JOIN:") == 0) {
            string requestedName = msg.substr(5);

            {
                lock_guard<mutex> lock(userCheckMutex);
                userCheckResult.erase(requestedName);
            }

            string brokerMsg = "USER_JOIN:" + requestedName;
            sendToBroker(brokerMsg);

            while (true) {
                {
                    lock_guard<mutex> lock(userCheckMutex);

                    if (userCheckResult.find(requestedName) != userCheckResult.end()) {
                        bool ok = userCheckResult[requestedName];
                        userCheckResult.erase(requestedName);

                        if (!ok) {
                            string error = "ERROR:Username already taken";
                            send(client_socket, error.c_str(), error.length(), 0);
                            break;
                        }

                        username = requestedName;

                        {
                            lock_guard<mutex> lock2(users_mutex);
                            users[username] = client_socket;
                        }

                        string response = "OK";
                        send(client_socket, response.c_str(), response.length(), 0);
                        break;
                    }
                }

                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }

        else if (msg.find("ROOM:") == 0) {
            string room = msg.substr(5);

            {
                lock_guard<mutex> lock(rooms_mutex);
                rooms[room].push_back(client_socket);
            }

            currentRoom = room;

            sendHistory(client_socket, room);
        }

        else if (msg.find("MSG:") == 0) {

            if (username.empty()) {
                string err = "ERROR:Login required";
                send(client_socket, err.c_str(), err.length(), 0);
                continue;
            }

            string text = msg.substr(4);
            string formatted = username + ": " + text;

            cout << "[" << PORT << "] " << formatted << endl;

            saveMessage(currentRoom, formatted);

            string brokerMsg = "ROOM:" + currentRoom + "|" + formatted;
            sendToBroker(brokerMsg);

            broadcastLocal(currentRoom, formatted, client_socket);
        }

        else if (msg == "EXIT") {
            break;
        }
    }

    if (!username.empty()) {
        string leaveMsg = "USER_LEAVE:" + username;
        sendToBroker(leaveMsg);
    }

    {
        lock_guard<mutex> lock(users_mutex);
        users.erase(username);
    }

    {
        lock_guard<mutex> lock(rooms_mutex);
        for (auto& pair : rooms) {
            auto& vec = pair.second;
            vec.erase(remove(vec.begin(), vec.end(), client_socket), vec.end());
        }
    }

    closesocket(client_socket);
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, client_socket;
    sockaddr_in address;
    int addrlen = sizeof(address);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    broker_sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in broker_addr;
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &broker_addr.sin_addr);

    connect(broker_sock, (sockaddr*)&broker_addr, sizeof(broker_addr));

    thread(receiveFromBroker).detach();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    cout << "Server running on port " << PORT << endl;

    while (true) {
        client_socket = accept(server_fd, (sockaddr*)&address, &addrlen);
        thread(handleClient, client_socket).detach();
    }
}