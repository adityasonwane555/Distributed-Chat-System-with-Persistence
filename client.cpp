#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

SOCKET sock;
bool running = true;

void receiveMessages() {
    char buffer[1024];

    while (running) {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            running = false;
            break;
        }

        cout << "\n" << string(buffer, bytes) << endl;
        cout << "> ";
        cout.flush();
    }
}

int main() {
    WSADATA wsa;
    sockaddr_in addr;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    while (true) {
        SOCKET lb_sock = socket(AF_INET, SOCK_STREAM, 0);

        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(lb_sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            cout << "Load balancer not running" << endl;
            system("pause");
            continue;
        }

        char buffer[1024];
        int bytes = recv(lb_sock, buffer, sizeof(buffer), 0);

        if (bytes <= 0) {
            cout << "Invalid response from load balancer" << endl;
            closesocket(lb_sock);
            continue;
        }

        string response(buffer, bytes);
        closesocket(lb_sock);

        size_t pos = response.find_last_of(':');
        if (pos == string::npos) {
            cout << "Invalid format" << endl;
            continue;
        }

        int port = stoi(response.substr(pos + 1));

        sock = socket(AF_INET, SOCK_STREAM, 0);

        addr.sin_port = htons(port);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            cout << "Server not running on port " << port << endl;
            continue;
        }

        cout << "Connected to server on port " << port << endl;

        string name;
        while (true) {
            cout << "Enter username: ";
            getline(cin, name);

            string joinMsg = "JOIN:" + name;
            send(sock, joinMsg.c_str(), joinMsg.length(), 0);

            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(sock, buffer, sizeof(buffer), 0);

            if (bytes <= 0) {
                cout << "Server error" << endl;
                break;
            }

            string res(buffer, bytes);

            if (res == "OK") break;

            cout << res << endl;
        }

        string room;
        cout << "Enter room: ";
        getline(cin, room);

        string roomMsg = "ROOM:" + room;
        send(sock, roomMsg.c_str(), roomMsg.length(), 0);

        thread t(receiveMessages);

        string msg;
        while (running) {
            cout << "> ";
            getline(cin, msg);

            if (msg == "exit") {
                send(sock, "EXIT", 4, 0);
                running = false;
                break;
            }

            string sendMsg = "MSG:" + msg;
            send(sock, sendMsg.c_str(), sendMsg.length(), 0);
        }

        closesocket(sock);
        WSACleanup();

        t.join();
        break;
    }

    system("pause");
    return 0;
}