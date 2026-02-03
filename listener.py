import socket
import threading
import sys

def listen_udp(ip, port):
    # UDP listening works without admin
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        # Use 0.0.0.0 to listen on all interfaces
        sock.bind(("0.0.0.0", port))
        print(f"[*] UDP Listener active on 0.0.0.0:{port}")
        while True:
            try:
                data, addr = sock.recvfrom(2048)
                try:
                    txt = data.decode('utf-8')
                except UnicodeDecodeError:
                    txt = data.decode('utf-8', errors='replace')
                hex_data = ' '.join(f'{b:02x}' for b in data)
                print(f"[UDP] From {addr}: {txt} (Hex: {hex_data})")
            except Exception as e:
                print(f"[!] UDP handler error: {e}")
    except Exception as e:
        print(f"[!] UDP Error: {e}")

def listen_tcp_standard(ip, port):
    # Standard TCP listening works without admin
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", port))
        sock.listen(5)
        print(f"[*] TCP Listener active on 0.0.0.0:{port} (Waiting for SYN...)")
        
        while True:
            # We use a short timeout so we can see when something hits the port even if handshake doesn't finish
            # Actually, standard sockets don't show the SYN until accept() returns.
            # To see the SYN, we'd need a raw socket, but let's try just printing when accept returns.
            client, addr = sock.accept()
            print(f"[TCP] Connection established from {addr}")
            client.close()
    except Exception as e:
        print(f"[!] TCP Error: {e}")

if __name__ == "__main__":
    UDP_PORT = 60001
    TCP_PORT = 5001
    
    # Start threads
    t1 = threading.Thread(target=listen_udp, args=("0.0.0.0", UDP_PORT), daemon=True)
    # t2 = threading.Thread(target=listen_tcp_standard, args=("0.0.0.0", TCP_PORT), daemon=True)
    
    t1.start()
    # t2.start()
    
    print("="*60)
    print("Tiny64 Network Listener (QEMU SLIRP Mode)")
    print(f"Listening for packets arriving on localhost port {UDP_PORT} (UDP)")
    print("="*60)
    
    try:
        # Keep main thread alive
        while True:
            import time
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nExiting...")
        sys.exit(0)