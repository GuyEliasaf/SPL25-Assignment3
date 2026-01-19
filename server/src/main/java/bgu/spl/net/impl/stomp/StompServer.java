package bgu.spl.net.impl.stomp;

import bgu.spl.net.impl.data.Database;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: StompServer <port> <tpc|reactor>");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String mode = args[1];

        Runtime.getRuntime().addShutdownHook(new Thread(() -> { //addShutdownHook similar to finally
        System.out.println("Server is shutting down. Generating Report:");
        Database.getInstance().printReport();
        }));

        bgu.spl.net.srv.Server<String> server;

        if (mode.equals("tpc")) {
            server = bgu.spl.net.srv.Server.threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(), // protocol factory
                    () -> new StompEncoderDecoder() // message encoder decoder factory
            );
        } else if (mode.equals("reactor")) {
            server = bgu.spl.net.srv.Server.reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(), // protocol factory
                    () -> new StompEncoderDecoder() // message encoder decoder factory
            );
        } else {
            System.out.println("Invalid mode: " + mode + " (must be 'tpc' or 'reactor')");
            return;
        }

        server.serve();
    }
}
