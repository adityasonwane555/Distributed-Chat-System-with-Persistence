#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<int> servers = {8081, 8082};
int current = 0;

int main() {
    WSADATA wsa;
    SOCKET listenSock, clientSock;
    sockaddr_in addr;
    int addrlen = sizeof(addr);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    listenSock = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 10);

    cout << "Load balancer running on port 8080" << endl;

    while (true) {
        clientSock = accept(listenSock, (sockaddr*)&addr, &addrlen);

        int port = servers[current];
        current = (current + 1) % servers.size();

        string response = "SERVER:127.0.0.1:" + to_string(port);
        send(clientSock, response.c_str(), response.length(), 0);

        closesocket(clientSock);
    }
}