package bgu.spl.net.srv;

import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

public class ConnectionsImpl<T> implements Connections<T> {

    // <connectionId, ConnectionHandler<T>> to manage active connections
    private final Map<Integer, ConnectionHandler<T>> handlers = new ConcurrentHashMap<>();

    // <channel, <connectionId, subscriberId>> managing subscriptions
    private final Map<String, Map<Integer, Integer>> channelToSubscribers = new ConcurrentHashMap<>();

    // <connectionId, <subscriberId, channel> managing client subscriptions
    private final Map<Integer, Map<Integer, String>> clientToChannels = new ConcurrentHashMap<>();

    private AtomicInteger msgCurrentId = new AtomicInteger(0);

    
    public ConnectionsImpl() {
        
    }

    @Override
    public boolean send(int connectionId, T msg){
        if (!handlers.containsKey(connectionId))
            return false;
        msgCurrentId.addAndGet(1);
        handlers.get(connectionId).send(msg);
        return true;
    }

    @Override
    public void send(String channel, T msg){
        if(!channelToSubscribers.containsKey(channel))
            return;
        for (Integer connectionId : channelToSubscribers.get(channel).keySet()) {
            handlers.get(connectionId).send(msg);
        }
    }

    @Override
    public void disconnect(int connectionId){
        // Remove from handlers
        handlers.remove(connectionId);

        // Remove from channelToSubscribers
        Map<Integer, String> userSubs = clientToChannels.remove(connectionId);
        if (clientToChannels.containsKey(connectionId)) {
            for (String channel : userSubs.values()) {
                if (channelToSubscribers.containsKey(channel)) {
                    channelToSubscribers.get(channel).remove(connectionId);
                    if (channelToSubscribers.get(channel).isEmpty()) { //if no subscribers left, remove the channel
                        channelToSubscribers.remove(channel);
                    }
                }
            }
            // Remove from clientToChannels
            clientToChannels.remove(connectionId);
        }
    }

    public void addConnection(int connectionId, ConnectionHandler<T> handler) {
        handlers.put(connectionId, handler);
    }

    public void subscribe(int connectionId, String channel, int subscriberId) {
        channelToSubscribers.putIfAbsent(channel, new ConcurrentHashMap<>());
        channelToSubscribers.get(channel).put(connectionId, subscriberId);

        clientToChannels.putIfAbsent(connectionId, new ConcurrentHashMap<>());
        clientToChannels.get(connectionId).put(subscriberId, channel);
    }

    public void unsubscribe(int connectionId, int subscriberId){
        Map<Integer, String> userSubs = clientToChannels.get(connectionId);

        if(userSubs != null) {
            String channel = userSubs.remove(subscriberId);

            if (channel !=null) {
                Map<Integer, Integer> channelSubs = channelToSubscribers.get(channel);
                if (channelSubs != null) {
                    channelSubs.remove(connectionId);
                    if (channelSubs.isEmpty()) {
                        channelToSubscribers.remove(channel);
                    }
                }
            }
        }
    }

    public Map<Integer, Integer> getSubscribers(String channel) {
        return channelToSubscribers.get(channel);
    }

    public int generateMessageId(){
        return msgCurrentId.incrementAndGet();
    }

    
}
