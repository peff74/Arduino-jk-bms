#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// parseDevice und InfoparseData
// webserver

// Enable or disable debugging output
#define DEBUG_ENABLED true

// Debugging macro
#if DEBUG_ENABLED
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#endif


// WiFi credentials
const char* ssid = "test";
const char* password = "test";
WebServer server(80);  // Webserver auf Port 80

uint32_t minFreeHeap = UINT32_MAX;
unsigned long lastHeapUpdate = 0;

unsigned long lastMillis = 0;
uint32_t totalSeconds = 0;

void calculateUptime() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - lastMillis;

  // Überlauf von millis() behandeln
  if (currentMillis < lastMillis) {
    // Ein Überlauf ist aufgetreten
    elapsedMillis = UINT32_MAX - lastMillis + currentMillis;
  }

  
 if (elapsedMillis >= 1000) {
    // Vergangene Sekunden addieren
    totalSeconds += elapsedMillis / 1000;

    // Aktuellen millis()-Wert speichern
    lastMillis = currentMillis;
  }
}

void monitorFreeHeap() {
  uint32_t currentFreeHeap = ESP.getFreeHeap();

  if (currentFreeHeap < minFreeHeap) {
    minFreeHeap = currentFreeHeap;
  }

  if (millis() - lastHeapUpdate >= 1000) {
    minFreeHeap = UINT32_MAX;
    lastHeapUpdate = millis();
  }
}

const char style_css[] PROGMEM = R"rawliteral(
body {
  background-color: black;
  color: #00ff00;
  font-family: monospace;
  margin: 20px;
}

table {
  border-collapse: collapse;
  margin: 15px 0;
  width: auto; 
}

th, td {
  border: 1px solid #00ff00;
  padding: 8px;
  text-align: left;
}

th {
  background-color: #002200;
}

.device-section {
  margin-bottom: 30px;
}

.sketch-info-section {
  margin-top: 20px; 
}

.switches {
  margin-top: 20px;
}

.switches h3 {
  margin-bottom: 10px;
}

.switches div {
  margin-bottom: 10px;
}

.switches label {
  display: inline-block;
  width: 100px;
}

.switches button {
  padding: 5px 10px;
  margin-right: 5px;
  background-color: #004400;
  color: #00ff00;
  border: 1px solid #00ff00;
  border-radius: 3px;
  cursor: pointer;
}

.switches button:hover {
  background-color: #006600;
}

#system-info-section {
  margin-top: 20px; 
  font-size: 14px;  
  color: #00ff00;   
}
)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
	<head>
		<title>JKBMS Monitoring</title>
		<meta name="google-site-verification" content="EGqfKUvHp9Alk2ZNC84wIsSdRvvepdSUYzAwVsicYjk" />
		<meta charset="UTF-8"> <!-- Ensure UTF-8 encoding for special characters -->
		<link rel="stylesheet" href="/style.css">
		<script>
			// Global variables to store data
			let bmsData = {};
			let sketchInfo = {};
			let freeHeap = "";
			let uptime = { seconds: 0, formatted: "" };
			
			// Function to send a command to the ESP
			async function sendCommand(mac, action, state) {
				try {
					const response = await fetch('/control', {
						method: 'POST',
						headers: {
							'Content-Type': 'application/json',
						},
						body: JSON.stringify({
							mac: mac,
							action: action,
							state: state,
						}),
					});
					
					if (response.ok) {
						console.log(`Command sent: ${action} ${state} for ${mac}`);
						} else {
						console.error('Failed to send command');
					}
					} catch (error) {
					console.error('Error sending command:', error);
				}
			}
			
			// Function to fetch sketch information
			async function fetchSketchInfo() {
				try {
					const response = await fetch('/sketchinfo');
					sketchInfo = await response.json();
					updateSketchInfo();
					} catch (error) {
					console.error('Error fetching sketch info:', error);
				}
			}
			
			// Function to fetch free heap information
			async function fetchFreeHeap() {
				try {
					const response = await fetch('/freeheap');
					const data = await response.json();
					freeHeap = data.free_heap;
					updateFreeHeap();
					} catch (error) {
					console.error('Error fetching free heap:', error);
				}
			}
			
			// Function to fetch uptime information
			async function fetchUptime() {
				try {
					const response = await fetch('/uptime');
					const data = await response.json();
					uptime.seconds = data.uptime_seconds; // Als Zahl empfangen
					uptime.formatted = data.uptime_formatted;
					updateUptime();
					} catch (error) {
					console.error('Error fetching uptime:', error);
				}
			}
			
			// Function to update sketch information on the page
			function updateSketchInfo() {
				updateElement('sketch-name', sketchInfo.sketch_name);
				updateElement('compile-date', sketchInfo.compile_date);
				updateElement('compile-time', sketchInfo.compile_time);
				updateElement('esp-core-version', sketchInfo.esp_core_version);
			}
			
			// Function to update free heap information on the page
			function updateFreeHeap() {
				updateElement('free-heap', `Free Heap: ${freeHeap}`);
			}
			
			// Function to update uptime information on the page
			function updateUptime() {
				updateElement('uptime', `Uptime: ${uptime.formatted}`);
			}
			
			// Function to fetch data from the server
			async function fetchData() {
				try {
					const response = await fetch('/data');
					bmsData = await response.json();
					updatePage();
					} catch (error) {
					console.error('Error fetching data:', error);
				}
			}
			
			// Function to update the page with new data
			function updatePage() {
				for (const [mac, device] of Object.entries(bmsData)) {
					// General information
					updateElement(`${mac}-battery-voltage`, `${device.battery_voltage.toFixed(2)} V`);
					updateElement(`${mac}-battery-power`, `${device.battery_power.toFixed(2)} W`);
					updateElement(`${mac}-charge-current`, `${device.charge_current.toFixed(2)} A`);
					updateElement(`${mac}-percent-remain`, `${device.percent_remain} %`);
					updateElement(`${mac}-capacity-remain`, `${device.capacity_remain.toFixed(2)} Ah`);
					updateElement(`${mac}-nominal-capacity`, `${device.nominal_capacity.toFixed(2)} Ah`);
					updateElement(`${mac}-cycle-count`, `${device.cycle_count}`);
					updateElement(`${mac}-cycle-capacity`, `${device.cycle_capacity.toFixed(2)} Ah`);
					updateElement(`${mac}-uptime`, `${device.uptime}`);
					
					// Temperatures
					updateElement(`${mac}-battery-t1`, `${device.battery_t1.toFixed(1)} °C`);
					updateElement(`${mac}-battery-t2`, `${device.battery_t2.toFixed(1)} °C`);
					updateElement(`${mac}-mos-temp`, `${device.mos_temp.toFixed(1)} °C`);
					
					// Cell voltages
					for (let j = 0; j < device.cell_voltages.length; j++) {
						updateElement(`${mac}-cell-${j + 1}`, `${device.cell_voltages[j].toFixed(3)} V`);
					}
					updateElement(`${mac}-average-cell-voltage`, `${device.average_cell_voltage.toFixed(3)} V`);
					updateElement(`${mac}-delta-cell-voltage`, `${device.delta_cell_voltage.toFixed(3)} V`);
					
					// Wire resistances
					for (let j = 0; j < device.wire_resist.length; j++) {
						updateElement(`${mac}-wire-resist-${j + 1}`, `${device.wire_resist[j].toFixed(3)} Ω`);
					}
					
					// Status flags
					updateElement(`${mac}-charge`, `${device.charge ? 'Active' : 'Inactive'}`);
					updateElement(`${mac}-discharge`, `${device.discharge ? 'Active' : 'Inactive'}`);
					updateElement(`${mac}-balance`, `${device.balance ? 'Active' : 'Inactive'}`);
					updateElement(`${mac}-balancing-action`, `${device.balancing_action}`);
					updateElement(`${mac}-balance-curr`, `${device.balance_curr.toFixed(3)} A`);
					
					// BMS settings
					updateElement(`${mac}-cell-count`, `${device.cell_count}`);
					updateElement(`${mac}-total-battery-capacity`, `${device.total_battery_capacity.toFixed(2)} Ah`);
					updateElement(`${mac}-balance-trigger-voltage`, `${device.balance_trigger_voltage.toFixed(3)} V`);
					updateElement(`${mac}-balance-starting-voltage`, `${device.balance_starting_voltage.toFixed(3)} V`);
					updateElement(`${mac}-max-charge-current`, `${device.max_charge_current.toFixed(2)} A`);
					updateElement(`${mac}-max-discharge-current`, `${device.max_discharge_current.toFixed(2)} A`);
					updateElement(`${mac}-max-balance-current`, `${device.max_balance_current.toFixed(2)} A`);
					updateElement(`${mac}-cell-undervoltage-protection`, `${device.cell_undervoltage_protection.toFixed(3)} V`);
					updateElement(`${mac}-cell-undervoltage-recovery`, `${device.cell_undervoltage_recovery.toFixed(3)} V`);
					updateElement(`${mac}-cell-overvoltage-protection`, `${device.cell_overvoltage_protection.toFixed(3)} V`);
					updateElement(`${mac}-cell-overvoltage-recovery`, `${device.cell_overvoltage_recovery.toFixed(3)} V`);
					updateElement(`${mac}-power-off-voltage`, `${device.power_off_voltage.toFixed(3)} V`);
					updateElement(`${mac}-charge-overcurrent-protection-delay`, `${device.charge_overcurrent_protection_delay} s`);
					updateElement(`${mac}-charge-overcurrent-protection-recovery-time`, `${device.charge_overcurrent_protection_recovery_time} s`);
					updateElement(`${mac}-discharge-overcurrent-protection-delay`, `${device.discharge_overcurrent_protection_delay} s`);
					updateElement(`${mac}-discharge-overcurrent-protection-recovery-time`, `${device.discharge_overcurrent_protection_recovery_time} s`);
					updateElement(`${mac}-short-circuit-protection-recovery-time`, `${device.short_circuit_protection_recovery_time} s`);
					updateElement(`${mac}-charge-overtemperature-protection`, `${device.charge_overtemperature_protection.toFixed(1)} °C`);
					updateElement(`${mac}-charge-overtemperature-protection-recovery`, `${device.charge_overtemperature_protection_recovery.toFixed(1)} °C`);
					updateElement(`${mac}-discharge-overtemperature-protection`, `${device.discharge_overtemperature_protection.toFixed(1)} °C`);
					updateElement(`${mac}-discharge-overtemperature-protection-recovery`, `${device.discharge_overtemperature_protection_recovery.toFixed(1)} °C`);
					updateElement(`${mac}-charge-undertemperature-protection`, `${device.charge_undertemperature_protection.toFixed(1)} °C`);
					updateElement(`${mac}-charge-undertemperature-protection-recovery`, `${device.charge_undertemperature_protection_recovery.toFixed(1)} °C`);
					updateElement(`${mac}-power-tube-overtemperature-protection`, `${device.power_tube_overtemperature_protection.toFixed(1)} °C`);
					updateElement(`${mac}-power-tube-overtemperature-protection-recovery`, `${device.power_tube_overtemperature_protection_recovery.toFixed(1)} °C`);
					updateElement(`${mac}-short-circuit-protection-delay`, `${device.short_circuit_protection_delay} µs`);
				}
			}
			
			// Helper function to update a DOM element
			function updateElement(id, value) {
				const element = document.getElementById(id);
				if (element) {
					element.textContent = value;
				}
			}
			
			// Function to create the initial page structure
			function createPage() {
				const contentDiv = document.getElementById('content');
				contentDiv.innerHTML = '';  // Clear old content
				
				// Add BMS data sections (as before)
				for (const [mac, device] of Object.entries(bmsData)) {
					const deviceSection = document.createElement('div');
					deviceSection.className = 'device-section';
					deviceSection.innerHTML = `
					<div class="device-section">
				<h2>BMS: ${mac}</h2>
				
				<h3>General Information</h3>
				<table>
					<tr><th>Parameter</th><th>Value</th></tr>
					<tr><td>Battery Voltage</td><td id="${mac}-battery-voltage">${device.battery_voltage.toFixed(2)} V</td></tr>
					<tr><td>Battery Power</td><td id="${mac}-battery-power">${device.battery_power.toFixed(2)} W</td></tr>
					<tr><td>Charge Current</td><td id="${mac}-charge-current">${device.charge_current.toFixed(2)} A</td></tr>
					<tr><td>State of Charge</td><td id="${mac}-percent-remain">${device.percent_remain} %</td></tr>
					<tr><td>Capacity Remain</td><td id="${mac}-capacity-remain">${device.capacity_remain.toFixed(2)} Ah</td></tr>
					<tr><td>Nominal Capacity</td><td id="${mac}-nominal-capacity">${device.nominal_capacity.toFixed(2)} Ah</td></tr>
					<tr><td>Cycle Count</td><td id="${mac}-cycle-count">${device.cycle_count}</td></tr>
					<tr><td>Cycle Capacity</td><td id="${mac}-cycle-capacity">${device.cycle_capacity.toFixed(2)} Ah</td></tr>
					<tr><td>Uptime</td><td id="${mac}-uptime">${device.uptime}</td></tr>
				</table>
				
				<h3>Temperatures</h3>
				<table>
					<tr><th>Parameter</th><th>Value</th></tr>
					<tr><td>Battery T1</td><td id="${mac}-battery-t1">${device.battery_t1.toFixed(1)} °C</td></tr>
					<tr><td>Battery T2</td><td id="${mac}-battery-t2">${device.battery_t2.toFixed(1)} °C</td></tr>
					<tr><td>MOS Temperature</td><td id="${mac}-mos-temp">${device.mos_temp.toFixed(1)} °C</td></tr>
				</table>
				
				<h3>Cell Voltages</h3>
				<table>
					<tr><th>Cell</th><th>Voltage</th></tr>
					${device.cell_voltages.map((voltage, index) => `
					<tr><td>Cell ${index + 1}</td><td id="${mac}-cell-${index + 1}">${voltage.toFixed(3)} V</td></tr>
					`).join('')}
					<tr><td>Average Cell Voltage</td><td id="${mac}-average-cell-voltage">${device.average_cell_voltage.toFixed(3)} V</td></tr>
					<tr><td>Delta Cell Voltage</td><td id="${mac}-delta-cell-voltage">${device.delta_cell_voltage.toFixed(3)} V</td></tr>
				</table>
				
				<h3>Wire Resistances</h3>
				<table>
					<tr><th>Cell</th><th>Resistance</th></tr>
					${device.wire_resist.map((resist, index) => `
					<tr><td>Cell ${index + 1}</td><td id="${mac}-wire-resist-${index + 1}">${resist.toFixed(3)} Ω</td></tr>
					`).join('')}
				</table>
				
				<h3>Status Flags</h3>
				<table>
					<tr><th>Parameter</th><th>Value</th></tr>
					<tr><td>Charge</td><td id="${mac}-charge">${device.charge ? 'Active' : 'Inactive'}</td></tr>
					<tr><td>Discharge</td><td id="${mac}-discharge">${device.discharge ? 'Active' : 'Inactive'}</td></tr>
					<tr><td>Balance</td><td id="${mac}-balance">${device.balance ? 'Active' : 'Inactive'}</td></tr>
					<tr><td>Balancing Action</td><td id="${mac}-balancing-action">${device.balancing_action}</td></tr>
					<tr><td>Balance Current</td><td id="${mac}-balance-curr">${device.balance_curr.toFixed(3)} A</td></tr>
				</table>
				
				<div class="switches">
					<h3>Control</h3>
					<div>
						<label>Charging:</label>
						<button id="${mac}-charging-on" onclick="sendCommand('${mac}', 'charging', 'on')">ON</button>
						<button id="${mac}-charging-off" onclick="sendCommand('${mac}', 'charging', 'off')">OFF</button>
					</div>
					<div>
						<label>Discharging:</label>
						<button id="${mac}-discharging-on" onclick="sendCommand('${mac}', 'discharging', 'on')">ON</button>
						<button id="${mac}-discharging-off" onclick="sendCommand('${mac}', 'discharging', 'off')">OFF</button>
					</div>
					<div>
						<label>Balancing:</label>
						<button id="${mac}-balancing-on" onclick="sendCommand('${mac}', 'balancing', 'on')">ON</button>
						<button id="${mac}-balancing-off" onclick="sendCommand('${mac}', 'balancing', 'off')">OFF</button>
					</div>
				</div>
			</div>
			
			<h3>BMS Settings</h3>
			<table>
				<tr><th>Parameter</th><th>Value</th></tr>
				<tr><td>Cell Count</td><td id="${mac}-cell-count">${device.cell_count}</td></tr>
				<tr><td>Total Battery Capacity</td><td id="${mac}-total-battery-capacity">${device.total_battery_capacity.toFixed(2)} Ah</td></tr>
				<tr><td>Balance Trigger Voltage</td><td id="${mac}-balance-trigger-voltage">${device.balance_trigger_voltage.toFixed(3)} V</td></tr>
				<tr><td>Balance Starting Voltage</td><td id="${mac}-balance-starting-voltage">${device.balance_starting_voltage.toFixed(3)} V</td></tr>
				<tr><td>Max Charge Current</td><td id="${mac}-max-charge-current">${device.max_charge_current.toFixed(2)} A</td></tr>
				<tr><td>Max Discharge Current</td><td id="${mac}-max-discharge-current">${device.max_discharge_current.toFixed(2)} A</td></tr>
				<tr><td>Max Balance Current</td><td id="${mac}-max-balance-current">${device.max_balance_current.toFixed(2)} A</td></tr>
				<tr><td>Cell Undervoltage Protection</td><td id="${mac}-cell-undervoltage-protection">${device.cell_undervoltage_protection.toFixed(3)} V</td></tr>
				<tr><td>Cell Undervoltage Recovery</td><td id="${mac}-cell-undervoltage-recovery">${device.cell_undervoltage_recovery.toFixed(3)} V</td></tr>
				<tr><td>Cell Overvoltage Protection</td><td id="${mac}-cell-overvoltage-protection">${device.cell_overvoltage_protection.toFixed(3)} V</td></tr>
				<tr><td>Cell Overvoltage Recovery</td><td id="${mac}-cell-overvoltage-recovery">${device.cell_overvoltage_recovery.toFixed(3)} V</td></tr>
				<tr><td>Power Off Voltage</td><td id="${mac}-power-off-voltage">${device.power_off_voltage.toFixed(3)} V</td></tr>
				<tr><td>Charge Overcurrent Protection Delay</td><td id="${mac}-charge-overcurrent-protection-delay">${device.charge_overcurrent_protection_delay} s</td></tr>
				<tr><td>Charge Overcurrent Protection Recovery Time</td><td id="${mac}-charge-overcurrent-protection-recovery-time">${device.charge_overcurrent_protection_recovery_time} s</td></tr>
				<tr><td>Discharge Overcurrent Protection Delay</td><td id="${mac}-discharge-overcurrent-protection-delay">${device.discharge_overcurrent_protection_delay} s</td></tr>
				<tr><td>Discharge Overcurrent Protection Recovery Time</td><td id="${mac}-discharge-overcurrent-protection-recovery-time">${device.discharge_overcurrent_protection_recovery_time} s</td></tr>
				<tr><td>Short Circuit Protection Recovery Time</td><td id="${mac}-short-circuit-protection-recovery-time">${device.short_circuit_protection_recovery_time} s</td></tr>
				<tr><td>Charge Overtemperature Protection</td><td id="${mac}-charge-overtemperature-protection">${device.charge_overtemperature_protection.toFixed(1)} °C</td></tr>
				<tr><td>Charge Overtemperature Protection Recovery</td><td id="${mac}-charge-overtemperature-protection-recovery">${device.charge_overtemperature_protection_recovery.toFixed(1)} °C</td></tr>
				<tr><td>Discharge Overtemperature Protection</td><td id="${mac}-discharge-overtemperature-protection">${device.discharge_overtemperature_protection.toFixed(1)} °C</td></tr>
				<tr><td>Discharge Overtemperature Protection Recovery</td><td id="${mac}-discharge-overtemperature-protection-recovery">${device.discharge_overtemperature_protection_recovery.toFixed(1)} °C</td></tr>
				<tr><td>Charge Undertemperature Protection</td><td id="${mac}-charge-undertemperature-protection">${device.charge_undertemperature_protection.toFixed(1)} °C</td></tr>
				<tr><td>Charge Undertemperature Protection Recovery</td><td id="${mac}-charge-undertemperature-protection-recovery">${device.charge_undertemperature_protection_recovery.toFixed(1)} °C</td></tr>
				<tr><td>Power Tube Overtemperature Protection</td><td id="${mac}-power-tube-overtemperature-protection">${device.power_tube_overtemperature_protection.toFixed(1)} °C</td></tr>
				<tr><td>Power Tube Overtemperature Protection Recovery</td><td id="${mac}-power-tube-overtemperature-protection-recovery">${device.power_tube_overtemperature_protection_recovery.toFixed(1)} °C</td></tr>
				<tr><td>Short Circuit Protection Delay</td><td id="${mac}-short-circuit-protection-delay">${device.short_circuit_protection_delay} µs</td></tr>
			</table>
			`;
			contentDiv.appendChild(deviceSection);
			}
			
			// Add sketch information section
			const sketchInfoSection = document.createElement('div');
			sketchInfoSection.className = 'sketch-info-section';
			sketchInfoSection.innerHTML = `
			<h3>Sketch Information</h3>
			<table>
				<tr><th>Parameter</th><th>Value</th></tr>
				<tr><td>Sketch Name</td><td id="sketch-name">${sketchInfo.sketch_name}</td></tr>
				<tr><td>Compile Date</td><td id="compile-date">${sketchInfo.compile_date}</td></tr>
				<tr><td>Compile Time</td><td id="compile-time">${sketchInfo.compile_time}</td></tr>
				<tr><td>ESP Core Version</td><td id="esp-core-version">${sketchInfo.esp_core_version}</td></tr>
			</table>
			`;
			contentDiv.appendChild(sketchInfoSection);
			
			// Add free heap and uptime information at the bottom
			const systemInfoSection = document.createElement('div');
			systemInfoSection.id = 'system-info-section';
			systemInfoSection.innerHTML = `
			<p id="free-heap">Free Heap: ${freeHeap}</p>
			<p id="uptime">Uptime: ${uptime.formatted}</p>
			`;
			contentDiv.appendChild(systemInfoSection);
			}
			
			// Initialize the page and start fetching data
			async function initialize() {
			await fetchSketchInfo();  // Fetch sketch info once
			await fetchData();        // Fetch initial BMS data
			await fetchFreeHeap();    // Fetch initial free heap data
			await fetchUptime();      // Fetch initial uptime data
			createPage();             // Create the page structure
			setInterval(fetchData, 5000);  // Update BMS data every 5 seconds
			setInterval(fetchFreeHeap, 1000);  // Update free heap every 1 second
			setInterval(fetchUptime, 1000);  // Update uptime every 1 second
			}
			
			// Start the process
			initialize();
		</script>
	</head>
	<body>
		<h1>JKBMS Status</h1>
		<div id="content"></div>
	</body>
</html>
)rawliteral";

// lesbare Anzeige der Speichergrößen
void formatBytes(size_t bytes, char* buffer, size_t bufferSize) {
  if (bytes < 1024) {
    snprintf_P(buffer, bufferSize, PSTR("%zu%s"), bytes, PSTR(" Byte"));
  } else if (bytes < 1048576) {
    dtostrf(static_cast<float>(bytes) / 1024.0, 6, 2, buffer);
    strcat_P(buffer, PSTR(" KB"));
  } else {
    dtostrf(static_cast<float>(bytes) / 1048576.0, 6, 2, buffer);
    strcat_P(buffer, PSTR(" MB"));
  }
}


void getCoreVersion(char* version) {
  // Schreibe die Version in den char-Array
  sprintf(version, "%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
}

void getSketchName(char* sketchName) {
  const char* filename = __FILE__;
  int slashIndex = 0;
  for (int i = 0; i < strlen(filename); i++) {
    char c = filename[i];
    if (c == '\\' || c == '/') slashIndex = i + 1;
  }
  int nameLength = 0;
  for (int i = slashIndex; i < strlen(filename); i++) {
    char c = filename[i];
    if (c == '.') break;
    sketchName[nameLength++] = c;
  }
  sketchName[nameLength] = '\0';
}

String getSketchInfo() {
  char coreVersion[20];
  getCoreVersion(coreVersion);

  char sketchName[50];
  getSketchName(sketchName);

  // Compilation date and time
  String compileDate = __DATE__;
  String compileTime = __TIME__;

  // Create JSON object
  JsonDocument doc;
  doc["sketch_name"] = sketchName;
  doc["compile_date"] = compileDate;
  doc["compile_time"] = compileTime;
  doc["esp_core_version"] = coreVersion;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  return jsonResponse;
}

String formatUptime(uint32_t totalSeconds) {
  uint32_t days = totalSeconds / 86400;
  uint32_t hours = (totalSeconds % 86400) / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;

  char buffer[50];
  snprintf(buffer, sizeof(buffer), "%lu days, %02lu:%02lu:%02lu", days, hours, minutes, seconds);
  return String(buffer);
}

//********************************************
// JKBMS Class Definition
//********************************************
class JKBMS {
public:
  JKBMS(const std::string& mac)
    : targetMAC(mac) {}

  // BLE Components
  NimBLERemoteCharacteristic* pChr = nullptr;
  const NimBLEAdvertisedDevice* advDevice = nullptr;
  bool doConnect = false;
  bool connected = false;
  uint32_t lastNotifyTime = 0;
  std::string targetMAC;

  // Data Processing
  byte receivedBytes[320];
  int frame = 0;
  bool received_start = false;
  bool received_complete = false;
  bool new_data = false;
  int ignoreNotifyCount = 0;

  // BMS Data Fields
  float cellVoltage[16] = { 0 };
  float wireResist[16] = { 0 };
  float Average_Cell_Voltage = 0;
  float Delta_Cell_Voltage = 0;
  float Battery_Voltage = 0;
  float Battery_Power = 0;
  float Charge_Current = 0;
  float Battery_T1 = 0;
  float Battery_T2 = 0;
  float MOS_Temp = 0;
  int Percent_Remain = 0;
  float Capacity_Remain = 0;
  float Nominal_Capacity = 0;
  float Cycle_Count = 0;
  float Cycle_Capacity = 0;
  uint32_t Uptime;
  uint8_t sec, mi, hr, days;
  float Balance_Curr = 0;
  bool Balance = false;
  bool Charge = false;
  bool Discharge = false;
  int Balancing_Action = 0;

  float balance_trigger_voltage = 0;
  float cell_voltage_undervoltage_protection = 0;
  float cell_voltage_undervoltage_recovery = 0;
  float cell_voltage_overvoltage_protection = 0;
  float cell_voltage_overvoltage_recovery = 0;
  float power_off_voltage = 0;
  float max_charge_current = 0;
  float charge_overcurrent_protection_delay = 0;
  float charge_overcurrent_protection_recovery_time = 0;
  float max_discharge_current = 0;
  float discharge_overcurrent_protection_delay = 0;
  float discharge_overcurrent_protection_recovery_time = 0;
  float short_circuit_protection_recovery_time = 0;
  float max_balance_current = 0;
  float charge_overtemperature_protection = 0;
  float charge_overtemperature_protection_recovery = 0;
  float discharge_overtemperature_protection = 0;
  float discharge_overtemperature_protection_recovery = 0;
  float charge_undertemperature_protection = 0;
  float charge_undertemperature_protection_recovery = 0;
  float power_tube_overtemperature_protection = 0;
  float power_tube_overtemperature_protection_recovery = 0;
  int cell_count = 0;
  float total_battery_capacity = 0;
  float short_circuit_protection_delay = 0;
  float balance_starting_voltage = 0;




  // Methods
  bool connectToServer();
  void parseDeviceInfo();
  void parseData();
  void bms_settings();
  void writeRegister(uint8_t address, uint32_t value, uint8_t length);
  void handleNotification(uint8_t* pData, size_t length);

private:
  uint8_t crc(const uint8_t data[], uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) crc += data[i];
    return crc;
  }
};

//********************************************
// Global Variables and Callbacks
//********************************************
JKBMS jkBmsDevices[] = {
  JKBMS("c8:47:80:43:80:6e"),  // Only one device configured
  // JKBMS("20:aa:08:25:26:8b"),    // Add more devices here
  // JKBMS("MAC_ADDRESS_3")
};

const int bmsDeviceCount = sizeof(jkBmsDevices) / sizeof(jkBmsDevices[0]);  // Dynamic device count

NimBLEScan* pScan;
unsigned long lastScanTime = 0;

class ClientCallbacks : public NimBLEClientCallbacks {
  JKBMS* bms;
public:
  ClientCallbacks(JKBMS* bmsInstance)
    : bms(bmsInstance) {}

  void onConnect(NimBLEClient* pClient) {
    DEBUG_PRINTF("Connected to %s\n", bms->targetMAC.c_str());
    bms->connected = true;
  }

  void onDisconnect(NimBLEClient* pClient, int reason) {
    DEBUG_PRINTF("%s disconnected, reason: %d\n", bms->targetMAC.c_str(), reason);
    bms->connected = false;
    bms->doConnect = false;
  }
};

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    DEBUG_PRINTF("BLE Device found: %s\n", advertisedDevice->toString().c_str());
    for (int i = 0; i < bmsDeviceCount; i++) {
      if (jkBmsDevices[i].targetMAC.empty()) continue;  // Skip empty MAC addresses
      if (advertisedDevice->getAddress().toString() == jkBmsDevices[i].targetMAC && !jkBmsDevices[i].connected && !jkBmsDevices[i].doConnect) {
        DEBUG_PRINTF("Found target device: %s\n", jkBmsDevices[i].targetMAC.c_str());
        jkBmsDevices[i].advDevice = advertisedDevice;
        jkBmsDevices[i].doConnect = true;
        NimBLEDevice::getScan()->stop();
      }
    }
  }
} scanCallbacks;

void notifyCB(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t length, bool isNotify) {
  DEBUG_PRINTLN("Notification received...");
  for (int i = 0; i < bmsDeviceCount; i++) {
    if (jkBmsDevices[i].pChr == pChr) {
      jkBmsDevices[i].handleNotification(pData, length);
      break;
    }
  }
}

//********************************************
// JKBMS Method Implementation
//********************************************
bool JKBMS::connectToServer() {
  DEBUG_PRINTF("Attempting to connect to %s...\n", targetMAC.c_str());
  NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());

  if (!pClient) {
    pClient = NimBLEDevice::createClient();
    DEBUG_PRINTLN("New client created.");
    pClient->setClientCallbacks(new ClientCallbacks(this), true);
    pClient->setConnectionParams(12, 12, 0, 150);
    pClient->setConnectTimeout(5000);
  }

  if (!pClient->connect(advDevice)) {
    DEBUG_PRINTF("Failed to connect to %s\n", targetMAC.c_str());
    return false;
  }

  DEBUG_PRINTF("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

  NimBLERemoteService* pSvc = pClient->getService("ffe0");
  if (pSvc) {
    pChr = pSvc->getCharacteristic("ffe1");
    if (pChr && pChr->canNotify()) {
      if (pChr->subscribe(true, notifyCB)) {
        DEBUG_PRINTF("Subscribed to notifications for %s\n", pChr->getUUID().toString().c_str());
        delay(500);
        writeRegister(0x97, 0x00000000, 0x00);  // COMMAND_DEVICE_INFO
        delay(500);
        writeRegister(0x96, 0x00000000, 0x00);  // COMMAND_CELL_INFO
        return true;
      }
    }
  }
  DEBUG_PRINTLN("Service or Characteristic not found or unable to subscribe.");
  return false;
}

void JKBMS::handleNotification(uint8_t* pData, size_t length) {
  DEBUG_PRINTLN("Handling notification...");
  lastNotifyTime = millis();

  if (ignoreNotifyCount > 0) {
    ignoreNotifyCount--;
    DEBUG_PRINTF("Ignoring notification. Remaining: %d\n", ignoreNotifyCount);
    return;
  }

  // Check for start of data frame
  if (pData[0] == 0x55 && pData[1] == 0xAA && pData[2] == 0xEB && pData[3] == 0x90) {
    DEBUG_PRINTLN("Start of data frame detected.");
    frame = 0;
    received_start = true;
    received_complete = false;

    // Store the received data
    for (int i = 0; i < length; i++) {
      receivedBytes[frame++] = pData[i];
    }
  } else if (received_start && !received_complete) {
    DEBUG_PRINTLN("Continuing data frame...");
    for (int i = 0; i < length; i++) {
      receivedBytes[frame++] = pData[i];
      if (frame >= 300) {
        received_complete = true;
        received_start = false;
        new_data = true;
        DEBUG_PRINTLN("New data available for parsing.");

        // Determine the type of data frame based on pData[4]
        switch (receivedBytes[4]) {  // Use receivedBytes[4] instead of pData[4]
          case 0x01:
            DEBUG_PRINTLN("BMS Settings frame detected.");
            bms_settings();
            break;
          case 0x02:
            DEBUG_PRINTLN("Cell data frame detected.");
            parseData();
            break;
          case 0x03:
            DEBUG_PRINTLN("Device info frame detected.");
            parseDeviceInfo();
            break;
          default:
            DEBUG_PRINTF("Unknown frame type: 0x%02X\n", receivedBytes[4]);
            break;
        }

        break;  // Exit the loop after processing the complete frame
      }
    }
  }
}

void JKBMS::writeRegister(uint8_t address, uint32_t value, uint8_t length) {
  DEBUG_PRINTF("Writing register: address=0x%02X, value=0x%08lX, length=%d\n", address, value, length);
  uint8_t frame[20] = { 0xAA, 0x55, 0x90, 0xEB, address, length };

  // Insert value (Little-Endian)
  frame[6] = value >> 0;  // LSB
  frame[7] = value >> 8;
  frame[8] = value >> 16;
  frame[9] = value >> 24;  // MSB

  // Calculate CRC
  frame[19] = crc(frame, 19);

  // Debug: Print the entire frame in hexadecimal format
  DEBUG_PRINTF("Frame to be sent: ");
  for (int i = 0; i < sizeof(frame); i++) {
    DEBUG_PRINTF("%02X ", frame[i]);
  }
  DEBUG_PRINTF("\n");

  if (pChr) {
    pChr->writeValue((uint8_t*)frame, (size_t)sizeof(frame));
  }
}

void JKBMS::bms_settings() {
  DEBUG_PRINTLN("Processing BMS settings...");
  cell_voltage_undervoltage_protection = ((receivedBytes[13] << 24 | receivedBytes[12] << 16 | receivedBytes[11] << 8 | receivedBytes[10]) * 0.001);
  cell_voltage_undervoltage_recovery = ((receivedBytes[17] << 24 | receivedBytes[16] << 16 | receivedBytes[15] << 8 | receivedBytes[14]) * 0.001);
  cell_voltage_overvoltage_protection = ((receivedBytes[21] << 24 | receivedBytes[20] << 16 | receivedBytes[19] << 8 | receivedBytes[18]) * 0.001);
  cell_voltage_overvoltage_recovery = ((receivedBytes[25] << 24 | receivedBytes[24] << 16 | receivedBytes[23] << 8 | receivedBytes[22]) * 0.001);
  balance_trigger_voltage = ((receivedBytes[29] << 24 | receivedBytes[28] << 16 | receivedBytes[27] << 8 | receivedBytes[26]) * 0.001);
  power_off_voltage = ((receivedBytes[49] << 24 | receivedBytes[48] << 16 | receivedBytes[47] << 8 | receivedBytes[46]) * 0.001);
  max_charge_current = ((receivedBytes[53] << 24 | receivedBytes[52] << 16 | receivedBytes[51] << 8 | receivedBytes[50]) * 0.001);
  charge_overcurrent_protection_delay = ((receivedBytes[57] << 24 | receivedBytes[56] << 16 | receivedBytes[55] << 8 | receivedBytes[54]));
  charge_overcurrent_protection_recovery_time = ((receivedBytes[61] << 24 | receivedBytes[60] << 16 | receivedBytes[59] << 8 | receivedBytes[58]));
  max_discharge_current = ((receivedBytes[65] << 24 | receivedBytes[64] << 16 | receivedBytes[63] << 8 | receivedBytes[62]) * 0.001);
  discharge_overcurrent_protection_delay = ((receivedBytes[69] << 24 | receivedBytes[68] << 16 | receivedBytes[67] << 8 | receivedBytes[66]));
  discharge_overcurrent_protection_recovery_time = ((receivedBytes[73] << 24 | receivedBytes[72] << 16 | receivedBytes[71] << 8 | receivedBytes[70]));
  short_circuit_protection_recovery_time = ((receivedBytes[77] << 24 | receivedBytes[76] << 16 | receivedBytes[75] << 8 | receivedBytes[74]));
  max_balance_current = ((receivedBytes[81] << 24 | receivedBytes[80] << 16 | receivedBytes[79] << 8 | receivedBytes[78]) * 0.001);
  charge_overtemperature_protection = ((receivedBytes[85] << 24 | receivedBytes[84] << 16 | receivedBytes[83] << 8 | receivedBytes[82]) * 0.1);
  charge_overtemperature_protection_recovery = ((receivedBytes[89] << 24 | receivedBytes[88] << 16 | receivedBytes[87] << 8 | receivedBytes[86]) * 0.1);
  discharge_overtemperature_protection = ((receivedBytes[93] << 24 | receivedBytes[92] << 16 | receivedBytes[91] << 8 | receivedBytes[90]) * 0.1);
  discharge_overtemperature_protection_recovery = ((receivedBytes[97] << 24 | receivedBytes[96] << 16 | receivedBytes[95] << 8 | receivedBytes[94]) * 0.1);
  charge_undertemperature_protection = ((receivedBytes[101] << 24 | receivedBytes[100] << 16 | receivedBytes[99] << 8 | receivedBytes[98]) * 0.1);
  charge_undertemperature_protection_recovery = ((receivedBytes[105] << 24 | receivedBytes[104] << 16 | receivedBytes[103] << 8 | receivedBytes[102]) * 0.1);
  power_tube_overtemperature_protection = ((receivedBytes[109] << 24 | receivedBytes[108] << 16 | receivedBytes[107] << 8 | receivedBytes[106]) * 0.1);
  power_tube_overtemperature_protection_recovery = ((receivedBytes[113] << 24 | receivedBytes[112] << 16 | receivedBytes[111] << 8 | receivedBytes[110]) * 0.1);
  cell_count = ((receivedBytes[117] << 24 | receivedBytes[116] << 16 | receivedBytes[115] << 8 | receivedBytes[114]));
  // 118   4   0x01 0x00 0x00 0x00    Charge switch
  // 122   4   0x01 0x00 0x00 0x00    Discharge switch
  // 126   4   0x01 0x00 0x00 0x00    Balancer switch
  total_battery_capacity = ((receivedBytes[133] << 24 | receivedBytes[132] << 16 | receivedBytes[131] << 8 | receivedBytes[130]) * 0.001);
  short_circuit_protection_delay = ((receivedBytes[137] << 24 | receivedBytes[136] << 16 | receivedBytes[135] << 8 | receivedBytes[134]) * 1);
  balance_starting_voltage = ((receivedBytes[141] << 24 | receivedBytes[140] << 16 | receivedBytes[139] << 8 | receivedBytes[138]) * 0.001);




  DEBUG_PRINTF("Cell voltage undervoltage protection: %.2fV\n", cell_voltage_undervoltage_protection);
  DEBUG_PRINTF("Cell voltage undervoltage recovery: %.2fV\n", cell_voltage_undervoltage_recovery);
  DEBUG_PRINTF("Cell voltage overvoltage protection: %.2fV\n", cell_voltage_overvoltage_protection);
  DEBUG_PRINTF("Cell voltage overvoltage recovery: %.2fV\n", cell_voltage_overvoltage_recovery);
  DEBUG_PRINTF("Balance trigger voltage: %.2fV\n", balance_trigger_voltage);
  DEBUG_PRINTF("Power off voltage: %.2fV\n", power_off_voltage);

  DEBUG_PRINTF("Max charge current: %.2fA\n", max_charge_current);
  DEBUG_PRINTF("Charge overcurrent protection delay: %.2fs\n", charge_overcurrent_protection_delay);
  DEBUG_PRINTF("Charge overcurrent protection recovery time: %.2fs\n", charge_overcurrent_protection_recovery_time);
  DEBUG_PRINTF("Max discharge current: %.2fA\n", max_discharge_current);
  DEBUG_PRINTF("Discharge overcurrent protection delay: %.2fs\n", discharge_overcurrent_protection_delay);
  DEBUG_PRINTF("Discharge overcurrent protection recovery time: %.2fs\n", discharge_overcurrent_protection_recovery_time);
  DEBUG_PRINTF("Short circuit protection recovery time: %.2fs\n", short_circuit_protection_recovery_time);
  DEBUG_PRINTF("Max balance current: %.2fA\n", max_balance_current);
  DEBUG_PRINTF("Charge overtemperature protection: %.2fC\n", charge_overtemperature_protection);
  DEBUG_PRINTF("Charge overtemperature protection recovery: %.2fC\n", charge_overtemperature_protection_recovery);
  DEBUG_PRINTF("Discharge overtemperature protection: %.2fC\n", discharge_overtemperature_protection);
  DEBUG_PRINTF("Discharge overtemperature protection recovery: %.2fC\n", discharge_overtemperature_protection_recovery);
  DEBUG_PRINTF("Charge undertemperature protection: %.2fC\n", charge_undertemperature_protection);
  DEBUG_PRINTF("Charge undertemperature protection recovery: %.2fC\n", charge_undertemperature_protection_recovery);
  DEBUG_PRINTF("Power tube overtemperature protection: %.2fC\n", power_tube_overtemperature_protection);
  DEBUG_PRINTF("Power tube overtemperature protection recovery: %.2fC\n", power_tube_overtemperature_protection_recovery);
  DEBUG_PRINTF("Cell count: %.d\n", cell_count);
  DEBUG_PRINTF("Total battery capacity: %.2fAh\n", total_battery_capacity);
  DEBUG_PRINTF("Short circuit protection delay: %.2fus\n", short_circuit_protection_delay);
  DEBUG_PRINTF("Balance starting voltage: %.2fV\n", balance_starting_voltage);
}

void JKBMS::parseDeviceInfo() {
  DEBUG_PRINTLN("Processing device info...");
  new_data = false;

  // Debugging: Ausgabe der empfangenen Bytes
  DEBUG_PRINTLN("Raw data received:");
  for (int i = 0; i < frame; i++) {
    DEBUG_PRINTF("%02X ", receivedBytes[i]);
    if ((i + 1) % 16 == 0) DEBUG_PRINTLN();  // Neue Zeile nach 16 Bytes
  }
  DEBUG_PRINTLN();

  // Überprüfen, ob genügend Daten empfangen wurden
  if (frame < 134) {  // 134 Bytes sind für die Geräteinformationen erforderlich
    DEBUG_PRINTLN("Error: Not enough data received for device info.");
    return;
  }

  // Extrahieren der Geräteinformationen aus den empfangenen Bytes
  std::string vendorID(receivedBytes + 6, receivedBytes + 6 + 16);
  std::string hardwareVersion(receivedBytes + 22, receivedBytes + 22 + 8);
  std::string softwareVersion(receivedBytes + 30, receivedBytes + 30 + 8);
  uint32_t uptime = (receivedBytes[41] << 24) | (receivedBytes[40] << 16) | (receivedBytes[39] << 8) | receivedBytes[38];
  uint32_t powerOnCount = (receivedBytes[45] << 24) | (receivedBytes[44] << 16) | (receivedBytes[43] << 8) | receivedBytes[42];
  std::string deviceName(receivedBytes + 46, receivedBytes + 46 + 16);
  std::string devicePasscode(receivedBytes + 62, receivedBytes + 62 + 16);
  std::string manufacturingDate(receivedBytes + 78, receivedBytes + 78 + 8);
  std::string serialNumber(receivedBytes + 86, receivedBytes + 86 + 11);
  std::string passcode(receivedBytes + 97, receivedBytes + 97 + 5);
  std::string userData(receivedBytes + 102, receivedBytes + 102 + 16);
  std::string setupPasscode(receivedBytes + 118, receivedBytes + 118 + 16);

  // Ausgabe der Geräteinformationen
  DEBUG_PRINTF("  Vendor ID: %s\n", vendorID.c_str());
  DEBUG_PRINTF("  Hardware version: %s\n", hardwareVersion.c_str());
  DEBUG_PRINTF("  Software version: %s\n", softwareVersion.c_str());
  DEBUG_PRINTF("  Uptime: %d s\n", uptime);
  DEBUG_PRINTF("  Power on count: %d\n", powerOnCount);
  DEBUG_PRINTF("  Device name: %s\n", deviceName.c_str());
  DEBUG_PRINTF("  Device passcode: %s\n", devicePasscode.c_str());
  DEBUG_PRINTF("  Manufacturing date: %s\n", manufacturingDate.c_str());
  DEBUG_PRINTF("  Serial number: %s\n", serialNumber.c_str());
  DEBUG_PRINTF("  Passcode: %s\n", passcode.c_str());
  DEBUG_PRINTF("  User data: %s\n", userData.c_str());
  DEBUG_PRINTF("  Setup passcode: %s\n", setupPasscode.c_str());
}

void JKBMS::parseData() {
  DEBUG_PRINTLN("Parsing data...");
  new_data = false;
  ignoreNotifyCount = 10;
  // Cell voltages
  for (int j = 0, i = 7; i < 38; j++, i += 2) {
    cellVoltage[j] = ((receivedBytes[i] << 8 | receivedBytes[i - 1]) * 0.001);
  }

  Average_Cell_Voltage = (((int)receivedBytes[75] << 8 | receivedBytes[74]) * 0.001);

  Delta_Cell_Voltage = (((int)receivedBytes[77] << 8 | receivedBytes[76]) * 0.001);

  for (int j = 0, i = 81; i < 112; j++, i += 2) {
    wireResist[j] = (((int)receivedBytes[i] << 8 | receivedBytes[i - 1]) * 0.001);
  }

  if (receivedBytes[145] == 0xFF) {
    MOS_Temp = ((0xFF << 24 | 0xFF << 16 | receivedBytes[145] << 8 | receivedBytes[144]) * 0.1);
  } else {
    MOS_Temp = ((receivedBytes[145] << 8 | receivedBytes[144]) * 0.1);
  }

  // Battery voltage
  Battery_Voltage = ((receivedBytes[153] << 24 | receivedBytes[152] << 16 | receivedBytes[151] << 8 | receivedBytes[150]) * 0.001);

  Charge_Current = ((receivedBytes[161] << 24 | receivedBytes[160] << 16 | receivedBytes[159] << 8 | receivedBytes[158]) * 0.001);

  Battery_Power = Battery_Voltage * Charge_Current;

  if (receivedBytes[163] == 0xFF) {
    Battery_T1 = ((0xFF << 24 | 0xFF << 16 | receivedBytes[163] << 8 | receivedBytes[162]) * 0.1);
  } else {
    Battery_T1 = ((receivedBytes[163] << 8 | receivedBytes[162]) * 0.1);
  }

  if (receivedBytes[165] == 0xFF) {
    Battery_T2 = ((0xFF << 24 | 0xFF << 16 | receivedBytes[165] << 8 | receivedBytes[164]) * 0.1);
  } else {
    Battery_T2 = ((receivedBytes[165] << 8 | receivedBytes[164]) * 0.1);
  }

  if ((receivedBytes[171] & 0xF0) == 0x0) {
    Balance_Curr = ((receivedBytes[171] << 8 | receivedBytes[170]) * 0.001);
  } else if ((receivedBytes[171] & 0xF0) == 0xF0) {
    Balance_Curr = (((receivedBytes[171] & 0x0F) << 8 | receivedBytes[170]) * -0.001);
  }

  Balancing_Action = receivedBytes[172];
  Percent_Remain = (receivedBytes[173]);
  Capacity_Remain = ((receivedBytes[177] << 24 | receivedBytes[176] << 16 | receivedBytes[175] << 8 | receivedBytes[174]) * 0.001);
  Nominal_Capacity = ((receivedBytes[181] << 24 | receivedBytes[180] << 16 | receivedBytes[179] << 8 | receivedBytes[178]) * 0.001);
  Cycle_Count = ((receivedBytes[185] << 24 | receivedBytes[184] << 16 | receivedBytes[183] << 8 | receivedBytes[182]));
  Cycle_Capacity = ((receivedBytes[189] << 24 | receivedBytes[188] << 16 | receivedBytes[187] << 8 | receivedBytes[186]) * 0.001);

  Uptime = receivedBytes[196] << 16 | receivedBytes[195] << 8 | receivedBytes[194];
  sec = Uptime % 60;
  Uptime /= 60;
  mi = Uptime % 60;
  Uptime /= 60;
  hr = Uptime % 24;
  days = Uptime / 24;

  if (receivedBytes[198] > 0) {
    Charge = true;
  } else if (receivedBytes[198] == 0) {
    Charge = false;
  }
  if (receivedBytes[199] > 0) {
    Discharge = true;
  } else if (receivedBytes[199] == 0) {
    Discharge = false;
  }
  if (receivedBytes[201] > 0) {
    Balance = true;
  } else if (receivedBytes[201] == 0) {
    Balance = false;
  }


  // Output values

  DEBUG_PRINTF("\n--- Data from %s ---\n", targetMAC.c_str());
  DEBUG_PRINTLN("Cell Voltages:");
  for (int j = 0; j < 16; j++) {
    DEBUG_PRINTF("  Cell %02d: %.3f V\n", j + 1, cellVoltage[j]);
  }
  DEBUG_PRINTLN("wire Resist:");
  for (int j = 0; j < 16; j++) {
    DEBUG_PRINTF("  Cell %02d: %.3f Ohm\n", j + 1, wireResist[j]);
  }
  DEBUG_PRINTF("Average Cell Voltage: %.2fV\n", Average_Cell_Voltage);
  DEBUG_PRINTF("Delta Cell Voltage: %.2fV\n", Delta_Cell_Voltage);
  DEBUG_PRINTF("Balance Curr: %.2fA\n", Balance_Curr);
  DEBUG_PRINTF("Battery Voltage: %.2fV\n", Battery_Voltage);
  DEBUG_PRINTF("Battery Power: %.2fW\n", Battery_Power);
  DEBUG_PRINTF("Charge Current: %.2fA\n", Charge_Current);
  DEBUG_PRINTF("Charge: %d%%\n", Percent_Remain);
  DEBUG_PRINTF("Capacity Remain: %.2fAh\n", Capacity_Remain);
  DEBUG_PRINTF("Nominal Capacity: %.2fAh\n", Nominal_Capacity);
  DEBUG_PRINTF("Cycle Count: %.2f\n", Cycle_Count);
  DEBUG_PRINTF("Cycle Capacity: %.2fAh\n", Cycle_Capacity);
  DEBUG_PRINTF("Temperature T1: %.1fC\n", Battery_T1);
  DEBUG_PRINTF("Temperature T2: %.1fC\n", Battery_T2);
  DEBUG_PRINTF("Temperature MOS: %.1fC\n", MOS_Temp);
  DEBUG_PRINTF("Uptime: %dd %dh %dm\n", days, hr, mi);
  DEBUG_PRINTF("Charge: %d\n", Charge);
  DEBUG_PRINTF("Discharge: %d\n", Discharge);
  DEBUG_PRINTF("Balance: %d\n", Balance);
  DEBUG_PRINTF("Balancing Action: %d\n", Balancing_Action);
}


void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleJSON() {
  JsonDocument doc;

  for (int i = 0; i < bmsDeviceCount; i++) {
    JKBMS& bms = jkBmsDevices[i];
    JsonObject device = doc[bms.targetMAC].to<JsonObject>();

    // Allgemeine Informationen
    device["battery_voltage"] = bms.Battery_Voltage;
    device["battery_power"] = bms.Battery_Power;
    device["charge_current"] = bms.Charge_Current;
    device["percent_remain"] = bms.Percent_Remain;
    device["capacity_remain"] = bms.Capacity_Remain;
    device["nominal_capacity"] = bms.Nominal_Capacity;
    device["cycle_count"] = bms.Cycle_Count;
    device["cycle_capacity"] = bms.Cycle_Capacity;
    device["uptime"] = String(bms.days) + "d " + String(bms.hr) + "h " + String(bms.mi) + "m";

    // Temperaturen
    device["battery_t1"] = bms.Battery_T1;
    device["battery_t2"] = bms.Battery_T2;
    device["mos_temp"] = bms.MOS_Temp;

    // Zellspannungen
    JsonArray cell_voltages = device["cell_voltages"].to<JsonArray>();
    for (int j = 0; j < bms.cell_count; j++) {
      cell_voltages.add(bms.cellVoltage[j]);
    }
    device["average_cell_voltage"] = bms.Average_Cell_Voltage;
    device["delta_cell_voltage"] = bms.Delta_Cell_Voltage;

    // Widerstände
    JsonArray wire_resist = device["wire_resist"].to<JsonArray>();
    for (int j = 0; j < bms.cell_count; j++) {
      wire_resist.add(bms.wireResist[j]);
    }

    // Statusflags
    device["charge"] = bms.Charge;
    device["discharge"] = bms.Discharge;
    device["balance"] = bms.Balance;
    device["balancing_action"] = bms.Balancing_Action;
    device["balance_curr"] = bms.Balance_Curr;

    // BMS Einstellungen
    device["cell_count"] = bms.cell_count;
    device["total_battery_capacity"] = bms.total_battery_capacity;
    device["balance_trigger_voltage"] = bms.balance_trigger_voltage;
    device["balance_starting_voltage"] = bms.balance_starting_voltage;
    device["max_charge_current"] = bms.max_charge_current;
    device["max_discharge_current"] = bms.max_discharge_current;
    device["max_balance_current"] = bms.max_balance_current;
    device["cell_undervoltage_protection"] = bms.cell_voltage_undervoltage_protection;
    device["cell_undervoltage_recovery"] = bms.cell_voltage_undervoltage_recovery;
    device["cell_overvoltage_protection"] = bms.cell_voltage_overvoltage_protection;
    device["cell_overvoltage_recovery"] = bms.cell_voltage_overvoltage_recovery;
    device["power_off_voltage"] = bms.power_off_voltage;
    device["charge_overcurrent_protection_delay"] = bms.charge_overcurrent_protection_delay;
    device["charge_overcurrent_protection_recovery_time"] = bms.charge_overcurrent_protection_recovery_time;
    device["discharge_overcurrent_protection_delay"] = bms.discharge_overcurrent_protection_delay;
    device["discharge_overcurrent_protection_recovery_time"] = bms.discharge_overcurrent_protection_recovery_time;
    device["short_circuit_protection_recovery_time"] = bms.short_circuit_protection_recovery_time;
    device["charge_overtemperature_protection"] = bms.charge_overtemperature_protection;
    device["charge_overtemperature_protection_recovery"] = bms.charge_overtemperature_protection_recovery;
    device["discharge_overtemperature_protection"] = bms.discharge_overtemperature_protection;
    device["discharge_overtemperature_protection_recovery"] = bms.discharge_overtemperature_protection_recovery;
    device["charge_undertemperature_protection"] = bms.charge_undertemperature_protection;
    device["charge_undertemperature_protection_recovery"] = bms.charge_undertemperature_protection_recovery;
    device["power_tube_overtemperature_protection"] = bms.power_tube_overtemperature_protection;
    device["power_tube_overtemperature_protection_recovery"] = bms.power_tube_overtemperature_protection_recovery;
    device["short_circuit_protection_delay"] = bms.short_circuit_protection_delay;
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void handleControl() {
  if (server.method() == HTTP_POST) {
    // JSON-Daten aus dem Request lesen
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    // Daten aus dem JSON-Objekt extrahieren
    String mac = doc["mac"];
    String action = doc["action"];
    String state = doc["state"];

    // Parameter für writeRegister() bestimmen
    uint8_t address;
    uint32_t value;

    if (action == "charging") {
      address = 0x1D;
    } else if (action == "discharging") {
      address = 0x1E;
    } else if (action == "balancing") {
      address = 0x1F;
    } else {
      server.send(400, "text/plain", "Invalid action");
      return;
    }

    if (state == "on") {
      value = 0x0000001;
    } else if (state == "off") {
      value = 0x00000000;
    } else {
      server.send(400, "text/plain", "Invalid state");
      return;
    }

    // writeRegister() für das entsprechende BMS aufrufen
    for (int i = 0; i < bmsDeviceCount; i++) {
      if (jkBmsDevices[i].targetMAC == mac.c_str()) {
        jkBmsDevices[i].writeRegister(address, value, 0x04);
        server.send(200, "text/plain", "Command executed");
        return;
      }
    }

    server.send(404, "text/plain", "BMS not found");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleSketchInfo() {
  String sketchInfo = getSketchInfo();
  server.send(200, "application/json", sketchInfo);
}

void handleFreeHeap() {
  char formattedHeap[20];
  formatBytes(minFreeHeap, formattedHeap, sizeof(formattedHeap));

  // JSON-Objekt erstellen
  JsonDocument doc;
  doc["free_heap"] = formattedHeap;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void handleUptime() {
  calculateUptime();
  JsonDocument doc;
  doc["uptime_seconds"] = totalSeconds;
  doc["uptime_formatted"] = formatUptime(totalSeconds);

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void handleCss() {
  server.send(200, "text/css", style_css);
}





//********************************************
// Main Program
//********************************************
void setup() {
  Serial.begin(115200);
  lastMillis = millis();
  // WiFi initialisieren
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  configTime(0, 0, "fritz.box", "de.pool.ntp.org");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/main", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCss);
  server.on("/data", HTTP_GET, handleJSON);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/sketchinfo", HTTP_GET, handleSketchInfo);
  server.on("/freeheap", HTTP_GET, handleFreeHeap);
  server.on("/uptime", HTTP_GET, handleUptime);
  server.begin();


  DEBUG_PRINTLN("Initializing NimBLE Client...");

  NimBLEDevice::init("MultiJKBMS-Client");
  NimBLEDevice::setPower(3);

  pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(true);
}

void loop() {
  calculateUptime();
  monitorFreeHeap();

  server.handleClient();
  // Connection management
  int connectedCount = 0;  // Counter for connected devices

  for (int i = 0; i < bmsDeviceCount; i++) {
    if (jkBmsDevices[i].targetMAC.empty()) continue;  // Skip empty MAC addresses

    if (jkBmsDevices[i].doConnect && !jkBmsDevices[i].connected) {
      if (jkBmsDevices[i].connectToServer()) {
        DEBUG_PRINTF("%s connected successfully\n", jkBmsDevices[i].targetMAC.c_str());
      }
      jkBmsDevices[i].doConnect = false;
    }

    if (jkBmsDevices[i].connected) {
      connectedCount++;  // Increment counter for each connected device
      if (millis() - jkBmsDevices[i].lastNotifyTime > 20000) {
        DEBUG_PRINTF("%s connection timeout\n", jkBmsDevices[i].targetMAC.c_str());
        NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(jkBmsDevices[i].advDevice->getAddress());
        if (pClient) pClient->disconnect();
      }
    }
  }

  // Start scan only if not all devices are connected
  if (connectedCount < bmsDeviceCount && (millis() - lastScanTime >= 10000)) {
    DEBUG_PRINTLN("Starting scan...");
    pScan->start(5000, false, true);
    lastScanTime = millis();
  }

  delay(10);
}