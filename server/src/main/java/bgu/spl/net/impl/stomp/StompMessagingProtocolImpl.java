package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol; 
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

import java.util.HashMap;
import java.util.Map;

import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.impl.data.User;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> { 

    private boolean shouldTerminate = false;
    private int connectionId;
    private Connections<String> connections;
    private final double STOMP_VERSION = 1.2;
    private boolean isLoggedIn = false;

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = (ConnectionsImpl<String>) connections;
    }
    
    @Override
public void process(String message) {
    if (message == null || message.isEmpty()) return;

    String[] lines = message.split("\n");  // Split message into lines
    String command = lines[0].trim(); //clean command line

    if (!isLoggedIn && !command.equals("CONNECT")) {
            sendError("User not logged in", "You must log in first.", null);
            return;
        }

    switch (command) {
        case "CONNECT":
            connect(lines);
            break;
        case "SEND":
            send(lines);
            break;
        case "SUBSCRIBE":
            subscribe(lines);
            break;
        case "UNSUBSCRIBE":
            unsubscribe(lines);
            break;
        case "DISCONNECT":
            disconnect(lines);
            break;
        default:
            System.out.println("Unknown command: " + command);
            break;
    }
}

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    public void connect(String[] lines){
        Map<String, String> headers = parseHeaders(lines);
        String acceptVersion = headers.get("accept-version");
        String host = headers.get("host");
        String login = headers.get("login");
        String passcode = headers.get("passcode");
        if (acceptVersion != null && acceptVersion.contains(String.valueOf(STOMP_VERSION)) && host != null && login != null && passcode != null) {
        LoginStatus status = Database.getInstance().login(connectionId, login, passcode);

        switch (status) {
        case LOGGED_IN_SUCCESSFULLY:
        case ADDED_NEW_USER:
            isLoggedIn = true;
            String response = "CONNECTED\nversion:" + STOMP_VERSION + "\n";
            connections.send(connectionId, response);
            break;

        case WRONG_PASSWORD:
            sendError("Wrong password", "Password does not match", null);
            break;

        case ALREADY_LOGGED_IN:
            sendError("User already logged in", "User is already logged in", null);
            break;
            
        case CLIENT_ALREADY_CONNECTED:
             break;
    }

        
            
        }
        else {
            sendError("Malformed Frame", "Invalid CONNECT frame parameters", null);
        }
    }

    public void send(String[] lines){
        Map<String, String> headers = parseHeaders(lines);
        String destination = headers.get("destination");
        String body = headers.get("body");
        String receipt = headers.get("receipt"); 

        if(destination == null || body == null) {
             sendError("Malformed Frame", "Missing destination or body", receipt);
             return;
        }

        Map<Integer, Integer> subscribers =((ConnectionsImpl<String>) connections).getSubscribers(destination);

        if(subscribers == null || !subscribers.containsKey(connectionId)) {
            sendError("Not subscribed", "User is not subscribed to topic " + destination, receipt);
            return;
        }
        
        int messageId = ((ConnectionsImpl<String>) connections).generateMessageId();
        for(Map.Entry<Integer, Integer> entry : subscribers.entrySet()){
            int targetConnId = entry.getKey();
            int subId = entry.getValue();

            String msg = "MESSAGE\n" +
                            "subscription:" + subId + "\n" +
                            "message-id:" + messageId + "\n" +
                            "destination:" + destination + "\n" +
                            "\n" +
                            body;
            
            connections.send(targetConnId, msg);
        }
    handleReceipt(receipt);
    }


    
    public void subscribe(String[] lines){
        Map<String, String> headers = parseHeaders(lines);
        String destination = headers.get("destination");
        String idStr = headers.get("id");
        String receipt = headers.get("receipt");

        if (destination == null || idStr == null) {
            sendError("Malformed Frame", "Missing destination or id header", receipt);
            return;
        }

        try {
            int subId = Integer.parseInt(idStr);
            ((ConnectionsImpl<String>) connections).subscribe(connectionId, destination, subId);
            handleReceipt(receipt);
        } catch (NumberFormatException e) {
            sendError("Invalid ID", "Subscription ID must be a number", receipt);
        }
    }

    public void unsubscribe(String[] lines){
        Map<String, String> headers = parseHeaders(lines);
        String idStr = headers.get("id");
        String receipt = headers.get("receipt");

        if (idStr == null) {
            sendError("Malformed Frame", "Missing id header", receipt);
            return;
        }

        try {
            int subId = Integer.parseInt(idStr);
            ((ConnectionsImpl<String>) connections).unsubscribe(connectionId, subId);
            handleReceipt(receipt);
        } catch (NumberFormatException e) {
            sendError("Invalid ID", "Subscription ID must be a number", receipt);
        }
    }

    public void disconnect(String[] lines){
        Map<String, String> headers = parseHeaders(lines);
        String receipt = headers.get("receipt");
        
        Database.getInstance().logout(connectionId);
        handleReceipt(receipt);
        shouldTerminate = true;
        isLoggedIn = false;
        connections.disconnect(connectionId);
    }
    
    private void handleReceipt(String receiptId) {
        if (receiptId != null) {
            String receiptFrame = "RECEIPT\nreceipt-id:" + receiptId + "\n";
            connections.send(connectionId, receiptFrame);
        }
    }


    // Helper method to parse headers from message lines
    private Map<String, String> parseHeaders(String[] lines) {

        Map<String, String> headers = new HashMap<>();
        int bodyStartIndex = -1;
        for (int i = 1; i < lines.length; i++) {
            String line = lines[i];
            if (line.isEmpty() || line.equals("\r")){
                bodyStartIndex = i + 1;
                break;
            } 
            int colonIndex = line.indexOf(':');
            if (colonIndex != -1) {
                String key = line.substring(0, colonIndex).trim();
                String value = line.substring(colonIndex + 1).trim();
                headers.put(key, value);
            
            }      
        }

        String body = "";
        if (bodyStartIndex != -1 && bodyStartIndex < lines.length) {
            StringBuilder sb = new StringBuilder(); 
            for (int i = bodyStartIndex; i < lines.length; i++) {
                sb.append(lines[i]).append("\n");
            }   

            body = sb.toString();
            if (body.endsWith("\n")) {
                body = body.substring(0, body.length() - 1); 
            }
            
        }
        headers.put("body", body);
            
            return headers; 
        }

        private void sendError(String message, String description, String receiptId) {
            StringBuilder sb = new StringBuilder();
            sb.append("ERROR\n");
            if (receiptId != null) {
                sb.append("receipt-id:").append(receiptId).append("\n");
            }
            sb.append("message:").append(message).append("\n");
            sb.append("\n"); 
            if (description != null) {
                sb.append(description); 
            }
            
            connections.send(connectionId, sb.toString());
            shouldTerminate = true;
            connections.disconnect(connectionId);
    }
}