#include "StompProtocol.h"
#include <sstream>
#include <fstream>
#include <algorithm>

using namespace std;

StompProtocol::StompProtocol() 
    : currentUserName(""), subscriptionIdCounter(0), receiptIdCounter(0), isConnected(false) {}

// --- Public Methods ---

vector<string> StompProtocol::processUserInput(string line) {
    stringstream ss(line);
    string command;
    ss >> command;
    
    vector<string> args;
    string arg;
    while (ss >> arg) args.push_back(arg);

    vector<string> framesToSend;

    if (command == "login") {
        framesToSend.push_back(handleLogin(args));
    } else if (!isConnected) {
        cout << "Please login first" << endl;
    } else if (command == "join") {
        framesToSend.push_back(handleJoin(args));
    } else if (command == "exit") {
        framesToSend.push_back(handleExit(args));
    } else if (command == "report") {
        return handleReport(args); 
    } else if (command == "summary") {
        handleSummary(args); 
    } else if (command == "logout") {
        framesToSend.push_back(handleLogout(args));
    } else {
        cout << "Unknown command" << endl;
    }

    return framesToSend;
}

bool StompProtocol::processServerFrame(string frame) {
    stringstream ss(frame);
    string command;
    getline(ss, command); 

    map<string, string> headers;
    string line;
    while (getline(ss, line) && line != "") {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colonPos = line.find(':');
        if (colonPos != string::npos) {
            headers[line.substr(0, colonPos)] = line.substr(colonPos + 1);
        }
    }

    string body = "";
    char c;
    while (ss.get(c)) { 
        if (c != '\0') body += c; 
    }

    if (command == "CONNECTED") {
        isConnected = true;
        cout << "Login successful" << endl;
    } 
    else if (command == "ERROR") {
        cout << "Error received from server: " << endl;
        if (headers.count("message")) cout << headers["message"] << endl;
        cout << body << endl;
        isConnected = false;
        return true; 
    } 
    else if (command == "RECEIPT") {
        if (headers.count("receipt-id")) {
            int rId = stoi(headers["receipt-id"]);
            if (pendingReceipts.count(rId)) {
                string action = pendingReceipts[rId];
                if (action == "logout") {
                    return true; 
                } else {
                    cout << action << endl; 
                    pendingReceipts.erase(rId);
                }
            }
        }
    } 
    else if (command == "MESSAGE") {
        handleServerMessage(body, headers);
    }

    return false; 
}

// --- Private Handlers ---

string StompProtocol::handleLogin(const vector<string>& args) {
    if (isConnected) {
        cout << "The client is already logged in, log out before trying again" << endl;
        return "";
    }
    if (args.size() < 3) {
        cout << "Usage: login {host:port} {username} {password}" << endl;
        return "";
    }
    
    currentUserName = args[1];
    string password = args[2];

    map<string, string> headers;
    headers["accept-version"] = "1.2";
    headers["host"] = "stomp.cs.bgu.ac.il";
    headers["login"] = currentUserName;
    headers["passcode"] = password;

    return buildFrame("CONNECT", headers, "");
}

string StompProtocol::handleJoin(const vector<string>& args) {
    if (args.empty()) return "";
    string gameName = args[0];

    int id = subscriptionIdCounter++;
    int receipt = receiptIdCounter++;

    channelToSubId[gameName] = id;
    subIdToChannel[id] = gameName;
    
    pendingReceipts[receipt] = "Joined channel " + gameName;

    if (games.find(gameName) == games.end()) {
        games[gameName] = GameState("Team A", "Team B"); 
    }

    map<string, string> headers;
    headers["destination"] = "/" + gameName;
    headers["id"] = to_string(id);
    headers["receipt"] = to_string(receipt);

    return buildFrame("SUBSCRIBE", headers, "");
}

string StompProtocol::handleExit(const vector<string>& args) {
    if (args.empty()) return "";
    string gameName = args[0];

    if (channelToSubId.find(gameName) == channelToSubId.end()) {
        cout << "Not subscribed to " << gameName << endl;
        return "";
    }

    int id = channelToSubId[gameName];
    int receipt = receiptIdCounter++;

    channelToSubId.erase(gameName);
    subIdToChannel.erase(id);
    
    pendingReceipts[receipt] = "Exited channel " + gameName;

    map<string, string> headers;
    headers["id"] = to_string(id);
    headers["receipt"] = to_string(receipt);

    return buildFrame("UNSUBSCRIBE", headers, "");
}

string StompProtocol::handleLogout(const vector<string>& args) {
    int receipt = receiptIdCounter++;
    pendingReceipts[receipt] = "logout";

    map<string, string> headers;
    headers["receipt"] = to_string(receipt);

    return buildFrame("DISCONNECT", headers, "");
}

vector<string> StompProtocol::handleReport(const vector<string>& args) {
    vector<string> frames;
    if (args.empty()) return frames;
    
    string file = args[0];
    names_and_events data;
    try {
        data = parseEventsFile(file); 
    } catch (...) {
        cout << "Error parsing file" << endl;
        return frames;
    }

    string gameName = data.team_a_name + "_" + data.team_b_name;
    
    if (games.find(gameName) == games.end()) {
        games[gameName] = GameState(data.team_a_name, data.team_b_name);
    } else {
        games[gameName].team_a = data.team_a_name;
        games[gameName].team_b = data.team_b_name;
    }

    bool isFirstEvent = true;
    for (const auto& event : data.events) {
        updateGameStats(gameName, event, currentUserName);

        map<string, string> headers;
        headers["destination"] = "/" + gameName;

        if (isFirstEvent) {
            headers["file"] = file;
            isFirstEvent = false;
        }
        
        string body = "user: " + currentUserName + "\n";
        body += "team a: " + data.team_a_name + "\n";
        body += "team b: " + data.team_b_name + "\n";
        body += "event name: " + event.get_name() + "\n";
        body += "time: " + to_string(event.get_time()) + "\n";
        
        body += "general game updates:\n";
        for (auto const& [key, val] : event.get_game_updates()) body += key + ": " + val + "\n";
        
        body += "team a updates:\n";
        for (auto const& [key, val] : event.get_team_a_updates()) body += key + ": " + val + "\n";
        
        body += "team b updates:\n";
        for (auto const& [key, val] : event.get_team_b_updates()) body += key + ": " + val + "\n";
        
        body += "description:\n" + event.get_discription();

        frames.push_back(buildFrame("SEND", headers, body));
    }
    return frames;
}

void StompProtocol::handleServerMessage(const string& body, const map<string, string>& headers) {
    string gameName = "";
    if (headers.count("destination")) {
        gameName = headers.at("destination");
        if (gameName.size()>0 && gameName[0] == '/') gameName = gameName.substr(1);
    }

    // Extract user from body manually
    string user = "unknown";
    stringstream ss(body);
    string line;
    if (getline(ss, line)) {
        size_t colon = line.find("user:");
        if (colon != string::npos) {
            user = line.substr(colon + 5);
            // Trim whitespace
            size_t first = user.find_first_not_of(' ');
            if (first != string::npos) user = user.substr(first);
            if (!user.empty() && user.back() == '\r') user.pop_back();
        }
    }

    // Use Event constructor that parses the body
    Event event(body);
    
    if (gameName.empty()) {
        gameName = event.get_team_a_name() + "_" + event.get_team_b_name();
    }

    updateGameStats(gameName, event, user);
    cout << "Game update received for " << gameName << " from " << user << ":" << endl;
    cout << event.get_discription() << endl; 
    cout << "----------------------------------------" << endl;
}

void StompProtocol::updateGameStats(string gameName, const Event& event, string reporter) {
    if (games.find(gameName) == games.end()) {
        games[gameName] = GameState(event.get_team_a_name(), event.get_team_b_name());
    }
    
    GameState& game = games[gameName];
    
    // Save report under specific user
    game.reports[reporter].push_back(event);
    
    // Update general stats
    for (auto const& [k, v] : event.get_game_updates()) game.general_stats[k] = v;
    for (auto const& [k, v] : event.get_team_a_updates()) game.team_a_stats[k] = v;
    for (auto const& [k, v] : event.get_team_b_updates()) game.team_b_stats[k] = v;
}

void StompProtocol::handleSummary(const vector<string>& args) {
    if (args.size() < 3) {
        cout << "Usage: summary {game_name} {user} {file}" << endl;
        return;
    }
    string gameName = args[0];
    string user = args[1];
    string file = args[2];

    if (games.find(gameName) == games.end()) {
        cout << "Game not found." << endl;
        return;
    }

    GameState& game = games[gameName];
    
    if (game.reports.find(user) == game.reports.end()) {
        cout << "No reports found from user " << user << " for this game." << endl;
    }

    ofstream out(file);
    if (!out) {
        cout << "Error opening file " << file << endl;
        return;
    }

    out << game.team_a << " vs " << game.team_b << endl;
    out << "Game stats:" << endl;
    out << "General stats:" << endl;
    for (auto const& [k, v] : game.general_stats) out << k << ": " << v << endl;
    
    out << game.team_a << " stats:" << endl;
    for (auto const& [k, v] : game.team_a_stats) out << k << ": " << v << endl;
    
    out << game.team_b << " stats:" << endl;
    for (auto const& [k, v] : game.team_b_stats) out << k << ": " << v << endl;
    
    out << "Game event reports:" << endl;
    
    if (game.reports.count(user)) {
        for (const Event& e : game.reports[user]) {
            out << e.get_time() << " - " << e.get_name() << ":" << endl;
            out << e.get_discription() << endl << endl;
        }
    }
    
    out.close();
    cout << "Summary created in " << file << endl;
}

string StompProtocol::buildFrame(string command, map<string, string> headers, string body) {
    stringstream ss;
    ss << command << "\n";
    for (auto const& [key, val] : headers) {
        ss << key << ":" << val << "\n";
    }
    ss << "\n"; 
    ss << body;
    ss << '\0'; 
    return ss.str();
}