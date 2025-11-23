#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
  int rssi;
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer webServer(80);

_Network _networks[32];
_Network _selectedNetwork;

// Attack modes
enum AttackMode {
  ATTACK_NONE = 0,
  ATTACK_DEAUTH,
  ATTACK_BEACON,
  ATTACK_PROBE,
  ATTACK_RICKROLL,
  ATTACK_FAKE_DNS,
  ATTACK_FAKE_HTTP,
  ATTACK_FAKE_CAPTIVE
};

AttackMode currentAttack = ATTACK_NONE;
bool attackActive = false;

void clearArray() {
  for (int i = 0; i < 32; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

String _correct = "";
String _tryPassword = "";
int connectedClients = 0;

// Dark theme CSS
const String DARK_CSS = R"(
* { margin: 0; padding: 0; box-sizing: border-box; }
body { 
    background: #0d1117; 
    color: #c9d1d9; 
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
    line-height: 1.6;
    min-height: 100vh;
}
.container { 
    max-width: 1200px; 
    margin: 0 auto; 
    padding: 20px; 
}
.header { 
    background: linear-gradient(135deg, #161b22, #0d1117);
    padding: 2rem; 
    border-radius: 15px;
    margin-bottom: 2rem;
    border: 1px solid #30363d;
    box-shadow: 0 8px 32px rgba(0,0,0,0.3);
}
.header h1 { 
    color: #ff6b6b; 
    font-size: 2.5rem; 
    margin-bottom: 0.5rem;
    text-shadow: 0 2px 4px rgba(0,0,0,0.5);
}
.subtitle { 
    color: #58a6ff; 
    font-size: 1.2rem; 
    opacity: 0.9;
}
.network-list { 
    background: #161b22; 
    border-radius: 10px; 
    padding: 1.5rem; 
    margin-bottom: 2rem;
    border: 1px solid #30363d;
}
.network-item { 
    background: #21262d; 
    margin: 0.5rem 0; 
    padding: 1rem; 
    border-radius: 8px;
    border-left: 4px solid #58a6ff;
    transition: all 0.3s ease;
}
.network-item:hover {
    background: #30363d;
    transform: translateX(5px);
}
.btn { 
    background: linear-gradient(135deg, #238636, #2ea043);
    color: white; 
    border: none; 
    padding: 12px 24px; 
    border-radius: 8px; 
    cursor: pointer;
    font-size: 1rem;
    font-weight: 600;
    transition: all 0.3s ease;
    margin: 0.3rem;
}
.btn:hover { 
    background: linear-gradient(135deg, #2ea043, #3fb950);
    transform: translateY(-2px);
    box-shadow: 0 4px 12px rgba(46, 160, 67, 0.4);
}
.btn-danger { 
    background: linear-gradient(135deg, #da3633, #f85149);
}
.btn-danger:hover { 
    background: linear-gradient(135deg, #f85149, #ff6d6d);
}
.btn-warning { 
    background: linear-gradient(135deg, #d29922, #e3b341);
}
.btn-warning:hover { 
    background: linear-gradient(135deg, #e3b341, #f2cc60);
}
.form-group { 
    margin-bottom: 1.5rem; 
}
.form-input { 
    width: 100%; 
    padding: 12px; 
    background: #21262d; 
    border: 1px solid #30363d; 
    border-radius: 8px; 
    color: #c9d1d9;
    font-size: 1rem;
    transition: all 0.3s ease;
}
.form-input:focus { 
    outline: none; 
    border-color: #58a6ff; 
    box-shadow: 0 0 0 2px rgba(88, 166, 255, 0.2);
}
.stats { 
    background: #161b22; 
    padding: 1rem; 
    border-radius: 10px; 
    margin: 1rem 0;
    border: 1px solid #30363d;
}
.attack-active { 
    animation: pulse 2s infinite; 
    border-color: #ff6b6b !important;
}
@keyframes pulse {
    0% { box-shadow: 0 0 0 0 rgba(255, 107, 107, 0.7); }
    70% { box-shadow: 0 0 0 10px rgba(255, 107, 107, 0); }
    100% { box-shadow: 0 0 0 0 rgba(255, 107, 107, 0); }
}
.success { color: #3fb950; }
.error { color: #f85149; }
.warning { color: #d29922; }
.info { color: #58a6ff; }
.grid { 
    display: grid; 
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); 
    gap: 1.5rem; 
    margin: 2rem 0;
}
.card { 
    background: #161b22; 
    padding: 1.5rem; 
    border-radius: 10px; 
    border: 1px solid #30363d;
    transition: all 0.3s ease;
}
.card:hover {
    transform: translateY(-5px);
    box-shadow: 0 8px 25px rgba(0,0,0,0.3);
}
)";

String header(String title) {
  String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HACKER Portal - )" + title + R"(</title>
    <style>)" + DARK_CSS + R"(</style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚠️ HACKER Portal</h1>
            <div class="subtitle">Advanced Network Security Toolkit</div>
        </div>
  )";
  return html;
}

String footer() {
  return R"(
        <div class="stats">
            <div class="grid">
                <div class="card">
                    <h3>📡 Network Info</h3>
                    <p>SSID: HACKER</p>
                    <p>Clients: )" + String(connectedClients) + R"(</p>
                </div>
                <div class="card">
                    <h3>⚡ Attack Status</h3>
                    <p>Mode: )" + String(attackActive ? "ACTIVE" : "INACTIVE") + R"(</p>
                    <p>Type: )" + String(currentAttack) + R"(</p>
                </div>
            </div>
        </div>
        <div style="text-align: center; margin-top: 2rem; opacity: 0.7;">
            <p>© 2024 HACKER Security Suite | Advanced Penetration Testing Tool</p>
        </div>
    </div>
    <script>
        function updateStats() {
            fetch('/stats').then(r => r.json()).then(data => {
                document.querySelectorAll('.stats').forEach(el => {
                    el.querySelector('p:nth-child(3)').textContent = 'Clients: ' + data.clients;
                    el.querySelector('p:nth-child(4)').textContent = 'Mode: ' + (data.attackActive ? 'ACTIVE' : 'INACTIVE');
                });
            });
        }
        setInterval(updateStats, 5000);
    </script>
</body>
</html>
  )";
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("HACKER", "12345678");
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleMain);
  webServer.on("/attack", handleAttack);
  webServer.on("/stats", handleStats);
  webServer.on("/admin", handleAdmin);
  webServer.on("/creds", handleCreds);
  webServer.onNotFound(handleMain);
  
  webServer.begin();
  Serial.println("HACKER Portal Started!");
  Serial.println("SSID: HACKER");
  Serial.println("Password: 12345678");
}

void performScan() {
  int n = WiFi.scanNetworks(false, true);
  clearArray();
  if (n > 0) {
    for (int i = 0; i < n && i < 32; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      network.rssi = WiFi.RSSI(i);
      network.ch = WiFi.channel(i);
      memcpy(network.bssid, WiFi.BSSID(i), 6);
      _networks[i] = network;
    }
  }
}

void startAttack(AttackMode mode) {
  if (attackActive) stopAttack();
  
  currentAttack = mode;
  attackActive = true;
  
  switch(mode) {
    case ATTACK_DEAUTH:
      Serial.println("Starting Deauth Attack...");
      break;
    case ATTACK_BEACON:
      Serial.println("Starting Beacon Spam...");
      break;
    case ATTACK_PROBE:
      Serial.println("Starting Probe Flood...");
      break;
    case ATTACK_RICKROLL:
      Serial.println("Starting Rickroll Attack...");
      break;
    case ATTACK_FAKE_DNS:
      Serial.println("Starting Fake DNS...");
      break;
    case ATTACK_FAKE_HTTP:
      Serial.println("Starting Fake HTTP...");
      break;
    case ATTACK_FAKE_CAPTIVE:
      Serial.println("Starting Captive Portal...");
      break;
    default:
      break;
  }
}

void stopAttack() {
  attackActive = false;
  currentAttack = ATTACK_NONE;
  Serial.println("All attacks stopped");
}

String getAttackName(AttackMode mode) {
  switch(mode) {
    case ATTACK_DEAUTH: return "Deauth Attack";
    case ATTACK_BEACON: return "Beacon Spam";
    case ATTACK_PROBE: return "Probe Flood";
    case ATTACK_RICKROLL: return "Rickroll Redirect";
    case ATTACK_FAKE_DNS: return "Fake DNS";
    case ATTACK_FAKE_HTTP: return "Fake HTTP";
    case ATTACK_FAKE_CAPTIVE: return "Captive Portal";
    default: return "None";
  }
}

void handleMain() {
  String html = header("Dashboard");
  html += R"(
    <div class="grid">
        <div class="card">
            <h3>🎯 Quick Actions</h3>
            <button class="btn" onclick="location.href='/attack'">⚔️ Attack Panel</button>
            <button class="btn" onclick="location.href='/admin'">🔧 Admin Panel</button>
            <button class="btn" onclick="location.href='/creds'">🔑 Captured Data</button>
        </div>
        <div class="card">
            <h3>📊 System Status</h3>
            <p>Heap: )" + String(ESP.getFreeHeap()) + R"( bytes</p>
            <p>Uptime: )" + String(millis() / 1000) + R"(s</p>
            <p>Networks: )" + String(WiFi.scanComplete()) + R"(</p>
        </div>
    </div>
  )";
  html += footer();
  webServer.send(200, "text/html", html);
}

void handleAttack() {
  if (webServer.hasArg("start")) {
    int attackNum = webServer.arg("start").toInt();
    startAttack((AttackMode)attackNum);
  } else if (webServer.hasArg("stop")) {
    stopAttack();
  }

  String html = header("Attack Panel");
  html += R"(
    <div class="network-list">
        <h2>⚔️ Attack Modules</h2>
        <div class="grid">
  )";

  // Attack buttons
  String attacks[] = {
    "Deauth Attack", "Beacon Spam", "Probe Flood", 
    "Rickroll Redirect", "Fake DNS", "Fake HTTP", "Captive Portal"
  };

  for (int i = 1; i <= 7; i++) {
    html += R"(
        <div class="card">
            <h3>)" + attacks[i-1] + R"(</h3>
            <button class="btn )" + (attackActive && currentAttack == i ? "btn-danger" : "btn-warning") + R"(" 
                    onclick="location.href='/attack?start=)" + String(i) + R"(')">
                )" + (attackActive && currentAttack == i ? "🛑 Stop" : "🚀 Start") + R"(
            </button>
        </div>
    )";
  }

  html += R"(
        </div>
        )" + (attackActive ? 
        "<div class='attack-active card'><h3 class='error'>⚠️ Attack Active: " + getAttackName(currentAttack) + "</h3></div>" 
        : "") + R"(
    </div>
  )";
  
  html += footer();
  webServer.send(200, "text/html", html);
}

void handleAdmin() {
  performScan();
  
  String html = header("Network Scanner");
  html += R"(
    <div class="network-list">
        <h2>📡 Available Networks</h2>
  )";

  for (int i = 0; i < 32; i++) {
    if (_networks[i].ssid == "") continue;
    
    String strength = "🔴";
    if (_networks[i].rssi > -60) strength = "🟢";
    else if (_networks[i].rssi > -80) strength = "🟡";
    
    html += R"(
        <div class="network-item">
            <div style="display: flex; justify-content: between; align-items: center;">
                <div>
                    <strong>)" + _networks[i].ssid + R"(</strong>
                    <div style="font-size: 0.9rem; opacity: 0.8;">
                        BSSID: )" + bytesToStr(_networks[i].bssid, 6) + R"( | 
                        Channel: )" + String(_networks[i].ch) + R"( | 
                        RSSI: )" + String(_networks[i].rssi) + R"(
                    </div>
                </div>
                <div>)" + strength + R"(</div>
            </div>
        </div>
    )";
  }
  
  html += "</div>";
  html += footer();
  webServer.send(200, "text/html", html);
}

void handleStats() {
  String json = "{";
  json += "\"clients\":" + String(connectedClients) + ",";
  json += "\"attackActive\":" + String(attackActive ? "true" : "false") + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap());
  json += "}";
  webServer.send(200, "application/json", json);
}

void handleCreds() {
  String html = header("Captured Data");
  html += R"(
    <div class="card">
        <h3>🔑 Captured Credentials</h3>
        <div style="background: #21262d; padding: 1rem; border-radius: 8px; margin: 1rem 0;">
            <pre style="color: #c9d1d9;">)" + _correct + R"(</pre>
        </div>
    </div>
  )";
  html += footer();
  webServer.send(200, "text/html", html);
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += "0";
    str += String(b[i], HEX);
    if (i < size - 1) str += ":";
  }
  return str;
}

unsigned long lastScan = 0;
unsigned long lastClientCheck = 0;
unsigned long lastAttack = 0;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // Update client count
  if (millis() - lastClientCheck >= 5000) {
    connectedClients = WiFi.softAPgetStationNum();
    lastClientCheck = millis();
  }

  // Perform attacks
  if (attackActive && millis() - lastAttack >= 1000) {
    switch(currentAttack) {
      case ATTACK_DEAUTH:
        // Deauth attack implementation
        break;
      case ATTACK_BEACON:
        // Beacon spam implementation
        break;
      // Add other attack implementations
      default:
        break;
    }
    lastAttack = millis();
  }

  // Network scanning
  if (millis() - lastScan >= 10000) {
    performScan();
    lastScan = millis();
  }
}
