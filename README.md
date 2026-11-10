# GT Proxy

[![License](https://img.shields.io/badge/license-MIT-orange.svg)](LICENSE)

A **Growtopia proxy** for Windows. It sits between the game client and the real game server, forwards all traffic,
and can read, change or block packets on the way.

An updated version of [ENet-Proxy](https://github.com/alxjann/ENet-Proxy).

> The proxy currently runs as a **console application**. The ImGui interface in `Source/UI` is not built at the
> moment; all output goes to the console.

---

### 🔗 Connection

- **server_data.php redirect**: serves `server_data.php` on ports 80 / 443, fetches the real data and points the
  client at the proxy (`127.0.0.1:19000`).
- **Real server lookup**: finds the real game server even while the hosts file points the game domains at this
  machine, using DNS-over-HTTPS (Cloudflare / Google).
- **Automatic HTTPS certificate**: downloads [mkcert](https://github.com/FiloSottile/mkcert) on first run and
  creates `cert.pem` / `key.pem` next to the executable.
- **Sub-server switching**: follows `OnSendToServer`, so the client stays on the proxy when it moves between
  servers.

---

## 🛠️ Building

Requirements: **Windows**, **Visual Studio 2022**, and **CMake 3.20+**.

Run `Build/compile.bat`, or manually type:

```
cd Build
cmake -S .. -B Proxy
cmake --build Proxy --config Release
```

| Output      | Location |
| ----------- | -------- |
| `Proxy.exe` | `Bin/`   |

---

## 🔗 Connecting Growtopia with the Proxy

1. Add these lines to your hosts file (`C:\Windows\System32\drivers\etc\hosts`, edit as administrator):

```
127.0.0.1 www.growtopia1.com
127.0.0.1 growtopia1.com

127.0.0.1 www.growtopia2.com
127.0.0.1 growtopia2.com
```

2. Run `Bin/Proxy.exe` and start Growtopia.
3. Press **Ctrl+C** in the console to stop the proxy. Remove the hosts lines to play without it.

---

## Note

**This project is for educational purposes only. The author is not responsible for any misuse. Use at your own risk.**
