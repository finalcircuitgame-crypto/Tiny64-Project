import socket
import struct
import threading
import sys

"""
Tiny64 Raw Ethernet Listener for QEMU Socket Mode
Receives raw Ethernet frames, responds to ARP, and prints UDP payloads.

Run with: python listener.py
Then run QEMU with: -netdev socket,id=n0,udp=127.0.0.1:60000,localaddr=0.0.0.0:60001
"""

# Configuration - must match QEMU's socket netdev settings
# QEMU: -netdev socket,udp=127.0.0.1:60000,localaddr=0.0.0.0:60001
LISTEN_PORT = 60000       # QEMU sends TO this port (udp=...60000)
SEND_PORT = 60001         # We send ARP replies TO this port (localaddr=...60001)
OUR_MAC = bytes([0x52, 0x55, 0x0a, 0x00, 0x02, 0x02])  # Virtual MAC for our "gateway"
OUR_IP = bytes([10, 0, 2, 2])  # 10.0.2.2 - what the kernel thinks is the gateway

ETH_TYPE_ARP = 0x0806
ETH_TYPE_IPV4 = 0x0800
ARP_OP_REQUEST = 1
ARP_OP_REPLY = 2
IP_PROTO_UDP = 17

def parse_ethernet(data):
    """Parse Ethernet header, returns (dest_mac, src_mac, ethertype, payload)"""
    if len(data) < 14:
        return None
    dest_mac = data[0:6]
    src_mac = data[6:12]
    ethertype = struct.unpack(">H", data[12:14])[0]
    payload = data[14:]
    return dest_mac, src_mac, ethertype, payload

def parse_arp(data):
    """Parse ARP packet"""
    if len(data) < 28:
        return None
    hw_type, proto_type, hw_len, proto_len, opcode = struct.unpack(">HHBBH", data[0:8])
    sender_mac = data[8:14]
    sender_ip = data[14:18]
    target_mac = data[18:24]
    target_ip = data[24:28]
    return {
        'opcode': opcode,
        'sender_mac': sender_mac,
        'sender_ip': sender_ip,
        'target_mac': target_mac,
        'target_ip': target_ip
    }

def parse_ipv4(data):
    """Parse IPv4 header, returns (protocol, src_ip, dest_ip, payload)"""
    if len(data) < 20:
        return None
    ihl = (data[0] & 0x0F) * 4
    protocol = data[9]
    src_ip = data[12:16]
    dest_ip = data[16:20]
    payload = data[ihl:]
    return protocol, src_ip, dest_ip, payload

def parse_udp(data):
    """Parse UDP header, returns (src_port, dest_port, payload)"""
    if len(data) < 8:
        return None
    src_port, dest_port, length, checksum = struct.unpack(">HHHH", data[0:8])
    payload = data[8:8+length-8]
    return src_port, dest_port, payload

def mac_str(mac):
    return ':'.join(f'{b:02x}' for b in mac)

def ip_str(ip):
    return '.'.join(str(b) for b in ip)

def build_arp_reply(target_mac, target_ip, sender_mac, sender_ip):
    """Build ARP reply Ethernet frame"""
    # Ethernet header
    eth = target_mac + sender_mac + struct.pack(">H", ETH_TYPE_ARP)
    # ARP header
    arp = struct.pack(">HHBBH", 1, 0x0800, 6, 4, ARP_OP_REPLY)
    arp += sender_mac + sender_ip
    arp += target_mac + target_ip
    # Pad to minimum Ethernet frame size (60 bytes, 64 with FCS)
    frame = eth + arp
    frame += b'\x00' * (60 - len(frame))
    return frame

def main():
    print("="*60)
    print("Tiny64 Raw Ethernet Listener (QEMU Socket Mode)")
    print(f"Listening on UDP port {LISTEN_PORT} for raw Ethernet frames")
    print(f"Sending ARP replies to UDP port {SEND_PORT}")
    print(f"Our virtual MAC: {mac_str(OUR_MAC)}")
    print(f"Our virtual IP:  {ip_str(OUR_IP)}")
    print("="*60)
    
    # Socket for receiving from QEMU
    rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx_sock.bind(("0.0.0.0", LISTEN_PORT))
    
    # Socket for sending to QEMU (ARP replies etc)
    tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print(f"[*] Listener active, waiting for packets...\n")
    
    while True:
        try:
            data, addr = rx_sock.recvfrom(2048)
            
            eth = parse_ethernet(data)
            if not eth:
                continue
            
            dest_mac, src_mac, ethertype, payload = eth
            
            if ethertype == ETH_TYPE_ARP:
                arp = parse_arp(payload)
                if arp and arp['opcode'] == ARP_OP_REQUEST:
                    target_ip = arp['target_ip']
                    print(f"[ARP] Request: Who has {ip_str(target_ip)}? Tell {ip_str(arp['sender_ip'])}")
                    
                    # Respond to ARP for our IP
                    if target_ip == OUR_IP:
                        reply = build_arp_reply(
                            arp['sender_mac'], arp['sender_ip'],
                            OUR_MAC, OUR_IP
                        )
                        tx_sock.sendto(reply, ("127.0.0.1", SEND_PORT))
                        print(f"[ARP] Reply sent: {ip_str(OUR_IP)} is at {mac_str(OUR_MAC)}")
                        
            elif ethertype == ETH_TYPE_IPV4:
                ipv4 = parse_ipv4(payload)
                if not ipv4:
                    continue
                protocol, src_ip, dest_ip, ip_payload = ipv4
                
                if protocol == IP_PROTO_UDP:
                    udp = parse_udp(ip_payload)
                    if udp:
                        src_port, dest_port, udp_payload = udp
                        print(f"[UDP] {ip_str(src_ip)}:{src_port} -> {ip_str(dest_ip)}:{dest_port}")
                        try:
                            text = udp_payload.decode('utf-8', errors='replace')
                            print(f"      Data: \"{text}\"")
                        except:
                            print(f"      Data: {udp_payload.hex()}")
                            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"[!] Error: {e}")
    
    rx_sock.close()
    tx_sock.close()

if __name__ == "__main__":
    main()