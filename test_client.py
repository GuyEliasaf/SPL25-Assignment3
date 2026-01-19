import socket
import time

HOST = '127.0.0.1'
PORT = 7777

def send_frame(sock, data):
    print(f"Sending:\n{data}")
    sock.sendall(data.encode('utf-8') + b'\0')
    time.sleep(0.5) # המתנה קטנה לתשובה
    response = sock.recv(1024)
    print("\nReceived:")
    print(response.decode('utf-8'))

try:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        
        # 1. בדיקת התחברות (CONNECT)
        connect_frame = (
            "CONNECT\n"
            "accept-version:1.2\n"
            "host:stomp.cs.bgu.ac.il\n"
            "login:meni\n"
            "passcode:films\n"
            "\n"
        )
        send_frame(s, connect_frame)

        # 2. בדיקת שליחת קובץ (SEND)
        # שים לב שאנחנו שומרים על החיבור פתוח!
        send_frame_msg = (
            "SEND\n"
            "destination:/topic/a\n"
            "file:games.txt\n"
            "\n"
            "Hello World"
        )
        send_frame(s, send_frame_msg)

except ConnectionRefusedError:
    print("Error: Could not connect to Java Server. Is it running on port 7777?")