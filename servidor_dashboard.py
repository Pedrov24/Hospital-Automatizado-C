from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import socket
import webbrowser


HOST = "127.0.0.1"
START_PORT = 8000
PROJECT_DIR = Path(__file__).resolve().parent
DASHBOARD_DIR = PROJECT_DIR / "hospital-dashboard"


def find_free_port(start_port):
    port = start_port
    while True:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            try:
                sock.bind((HOST, port))
                return port
            except OSError:
                port += 1


def main():
    if not DASHBOARD_DIR.exists():
        raise SystemExit("Pasta hospital-dashboard nao encontrada.")

    port = find_free_port(START_PORT)
    url = f"http://{HOST}:{port}"
    handler = partial(SimpleHTTPRequestHandler, directory=str(DASHBOARD_DIR))

    with ThreadingHTTPServer((HOST, port), handler) as server:
        print("Dashboard do Hospital Automatizado")
        print(f"Servidor iniciado em: {url}")
        print("Pressione Ctrl+C para encerrar.")

        if os.environ.get("HOSPITAL_NO_BROWSER") != "1":
            webbrowser.open(url)

        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nServidor encerrado.")


if __name__ == "__main__":
    main()
