import socket
import threading
import tkinter as tk
from tkinter.scrolledtext import ScrolledText
from datetime import datetime

PORT = 7654
LOGFILE = "tiny64_logs.txt"

def start_udp_listener(callback, file_callback, status_callback):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", PORT))

    count = 0
    while True:
        data, addr = sock.recvfrom(2048)
        try:
            text = data.decode("ascii", errors="replace")
        except:
            text = str(data)

        line = f"[{addr[0]}] {text}"
        count += 1

        callback(line)
        file_callback(line)
        status_callback(count)

def main():
    root = tk.Tk()
    root.title("Tiny64 Live Log Viewer")
    root.geometry("900x600")

    # --- MAIN TEXT AREA ---
    log_box = ScrolledText(root, font=("Consolas", 11))
    log_box.pack(fill="both", expand=True, padx=5, pady=5)

    # --- BUTTON BAR ---
    button_frame = tk.Frame(root)
    button_frame.pack(fill="x", pady=3)

    # --- STATUS BAR ---
    status_var = tk.StringVar()
    status_var.set("Waiting for packets...")
    status_bar = tk.Label(root, textvariable=status_var, anchor="w")
    status_bar.pack(fill="x")

    # --- CALLBACKS ---
    def add_log(msg):
        log_box.insert("end", msg + "\n")
        log_box.see("end")

    def save_to_file(msg):
        with open(LOGFILE, "a", encoding="utf-8") as f:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            f.write(f"{timestamp} {msg}\n")

    def manual_save():
        text = log_box.get("1.0", "end")
        with open(LOGFILE, "w", encoding="utf-8") as f:
            f.write(text)
        status_var.set("Saved to file.")

    def clear_logs():
        log_box.delete("1.0", "end")
        status_var.set("Logs cleared.")

    def copy_all():
        root.clipboard_clear()
        root.clipboard_append(log_box.get("1.0", "end"))
        root.update()
        status_var.set("Copied to clipboard.")

    def update_status(count):
        now = datetime.now().strftime("%H:%M:%S")
        status_var.set(f"Packets received: {count}   |   Last update: {now}")

    # --- BUTTONS ---
    tk.Button(button_frame, text="Copy", width=12, command=copy_all).pack(side="left", padx=5)
    tk.Button(button_frame, text="Save to File", width=12, command=manual_save).pack(side="left", padx=5)
    tk.Button(button_frame, text="Clear", width=12, command=clear_logs).pack(side="left", padx=5)

    # --- THREAD ---
    thread = threading.Thread(
        target=start_udp_listener,
        args=(add_log, save_to_file, update_status),
        daemon=True
    )
    thread.start()

    root.mainloop()

if __name__ == "__main__":
    main()
