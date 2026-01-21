#include <stdlib.h>
#include <ConnectionHandler.h> 
#include <thread>
#include <iostream>
#include <vector>
#include "StompProtocol.h"

using namespace std;

void serverListener(ConnectionHandler* handler, StompProtocol* protocol, bool* shouldTerminate) {
    while (!(*shouldTerminate)) {
        string frame;
        
        // Use getFrameAscii which reads until the null delimiter
        if (!handler->getFrameAscii(frame, '\0')) {
             cout << "Disconnected from server." << endl;
             *shouldTerminate = true;
             break;
        }

        bool terminate = protocol->processServerFrame(frame);
        if (terminate) {
            *shouldTerminate = true;
            handler->close();
        }
    }
}

int main(int argc, char *argv[]) {
    StompProtocol protocol;
    bool shouldTerminate = false;
    ConnectionHandler* handler = nullptr;
    thread* listenerThread = nullptr;

    const short bufsize = 1024;
    char buf[bufsize];

    while (!shouldTerminate) {
        cin.getline(buf, bufsize);
        string line(buf);
        
        if (line.empty()) continue;

        if (line.substr(0, 5) == "login") {
            if (protocol.getIsConnected()) {
                cout << "The client is already logged in, log out before trying again" << endl;
                continue;
            }
            
            stringstream ss(line);
            string cmd, hostPort, user, pass;
            ss >> cmd >> hostPort >> user >> pass;
            
            size_t colon = hostPort.find(':');
            if (colon == string::npos) {
                cout << "Invalid host:port format" << endl;
                continue;
            }
            string host = hostPort.substr(0, colon);
            short port = (short)stoi(hostPort.substr(colon + 1));
            
            handler = new ConnectionHandler(host, port);
            if (!handler->connect()) {
                cout << "Could not connect to server" << endl;
                delete handler;
                handler = nullptr;
                continue;
            }
            
            listenerThread = new thread(serverListener, handler, &protocol, &shouldTerminate);
            
            vector<string> frames = protocol.processUserInput(line);
            if (!frames.empty()) {
                if (!handler->sendFrameAscii(frames[0], '\0')) {
                    cout << "Error sending login frame" << endl;
                    shouldTerminate = true;
                }
            }
            
        } else {
            if (!protocol.getIsConnected()) {
                cout << "Please login first" << endl;
                continue;
            }
            
            vector<string> frames = protocol.processUserInput(line);
            for (const string& frame : frames) {
                 if (handler && !handler->sendFrameAscii(frame, '\0')) {
                     cout << "Error sending frame" << endl;
                     shouldTerminate = true;
                     break;
                 }
            }
        }
    }

    if (listenerThread) {
        if (listenerThread->joinable()) listenerThread->join();
        delete listenerThread;
    }
    if (handler) delete handler;

    return 0;
}