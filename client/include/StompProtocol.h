#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <mutex>
#include "event.h" 

// Struct to hold the state of a specific game
struct GameState {
    std::string team_a;
    std::string team_b;
    std::map<std::string, std::string> general_stats;
    std::map<std::string, std::string> team_a_stats;
    std::map<std::string, std::string> team_b_stats;
    
    // Map user -> list of events reported by them (required for summary command)
    std::map<std::string, std::vector<Event>> reports; 

    GameState() = default;
    GameState(std::string a, std::string b) : team_a(a), team_b(b) {}
};

class StompProtocol {
private:
    std::string currentUserName;
    int subscriptionIdCounter;
    int receiptIdCounter;
    bool isConnected;

    // Maps topic name to subscription ID
    std::map<std::string, int> channelToSubId;
    std::map<int, std::string> subIdToChannel;

    // Stores game data for the Summary command
    std::map<std::string, GameState> games;

    // Maps receipt-id to the action it confirms
    std::map<int, std::string> pendingReceipts;

public:
    StompProtocol();

    // Processes user input and returns a vector of frames to send
    std::vector<std::string> processUserInput(std::string line);

    // Processes a received frame. Returns true if connection should terminate.
    bool processServerFrame(std::string frame);

    // Getters / Setters
    bool getIsConnected() const { return isConnected; }
    void setConnected(bool status) { isConnected = status; }
    std::string getCurrentUser() const { return currentUserName; }

private:
    // Command Handlers
    std::string handleLogin(const std::vector<std::string>& args);
    std::string handleJoin(const std::vector<std::string>& args);
    std::string handleExit(const std::vector<std::string>& args);
    std::vector<std::string> handleReport(const std::vector<std::string>& args);
    void handleSummary(const std::vector<std::string>& args);
    std::string handleLogout(const std::vector<std::string>& args);

    // Server Frame Handlers
    void handleServerMessage(const std::string& body, const std::map<std::string, std::string>& headers);

    // Helpers
    std::string buildFrame(std::string command, std::map<std::string, std::string> headers, std::string body);
    void updateGameStats(std::string gameName, const Event& event, std::string reporter);
};