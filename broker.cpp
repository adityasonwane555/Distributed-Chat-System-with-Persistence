#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <set>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<SOCKET> servers;
mutex mtx;

set<string> globalUsers;
mutex user_mtx;

void broadcast(const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(mtx);
    for (SOCKET s : servers) {
        if (s != sender) {
            send(s, msg.c_str(), msg.length(), 0);
        }
    }
}

void handleServer(SOCKET s) {
    char buffer[1024];

    while (true) {
        int bytes = recv(s, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        string msg(buffer, bytes);

        if (msg.find("USER_JOIN:") == 0) {
            string name = msg.substr(10);

            lock_guard<mutex> lock(user_mtx);

            if (globalUsers.count(name)) {
                string res = "USER_EXISTS:" + name;
                send(s, res.c_str(), res.length(), 0);
            } else {
                globalUsers.insert(name);
                string res = "USER_OK:" + name;
                send(s, res.c_str(), res.length(), 0);
            }
        }

        else if (msg.find("USER_LEAVE:") == 0) {
            string name = msg.substr(11);

            lock_guard<mutex> lock(user_mtx);
            globalUsers.erase(name);
        }

        else {
            broadcast(msg, s);
        }
    }

    {
        lock_guard<mutex> lock(mtx);
        servers.erase(remove(servers.begin(), servers.end(), s), servers.end());
    }

    closesocket(s);
}

int main() {
    WSADATA wsa;
    SOCKET listenSock, clientSock;
    sockaddr_in addr;
    int addrlen = sizeof(addr);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    listenSock = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 10);

    cout << "Broker running on port 9000" << endl;

    while (true) {
        clientSock = accept(listenSock, (sockaddr*)&addr, &addrlen);

        {
            lock_guard<mutex> lock(mtx);
            servers.push_back(clientSock);
        }

        thread(handleServer, clientSock).detach();
    }
}