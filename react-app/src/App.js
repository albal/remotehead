import React, { useState, useEffect, useCallback } from 'react';

// Main App component for the ESP32 Bluetooth Redial Controller
const App = () => {
  // Function to get initial IP from current URL
  const getInitialIp = () => {
    // If we're not on localhost/development, use the hostname from the current URL
    if (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1') {
      return window.location.hostname;
    }
    // Default IP for AP mode when in development
    return '192.168.4.1';
  };

  // State variables for managing the UI and communication
  const [esp32Ip, setEsp32Ip] = useState(getInitialIp());
  const [homeWifiSsid, setHomeWifiSsid] = useState(''); // User's home Wi-Fi SSID
  const [homeWifiPassword, setHomeWifiPassword] = useState(''); // User's home Wi-Fi password
  const [dialNumber, setDialNumber] = useState(''); // Number to dial
  const [statusMessage, setStatusMessage] = useState('Connect to "REMOTEHEAD" Wi-Fi, then configure home network.'); // Status messages for the user
  const [isConnectedToEsp32, setIsConnectedToEsp32] = useState(false); // HTTP connection status to ESP32
  const [isBluetoothConnected, setIsBluetoothConnected] = useState(false); // Bluetooth connection status on ESP32
  const [esp32WifiMode, setEsp32WifiMode] = useState('AP'); // ESP32's current Wi-Fi mode

  // New state variables for automatic redial
  const [autoRedialEnabled, setAutoRedialEnabled] = useState(false);
  const [redialPeriod, setRedialPeriod] = useState(60); // Default to 60 seconds

  // Function to send commands to the ESP32
  const sendCommand = useCallback(async (endpoint, method = 'GET', body = null) => {
    setStatusMessage('Sending command...');
    try {
      const url = `http://${esp32Ip}/${endpoint}`;
      const options = {
        method: method,
        headers: {
          'Content-Type': 'application/json',
        },
      };
      if (body) {
        options.body = JSON.stringify(body);
      }

      const response = await fetch(url, options);
      const data = await response.json();

      if (response.ok) {
        setStatusMessage(`Command "${endpoint}" successful: ${data.message || JSON.stringify(data)}`);
        if (endpoint === 'status') {
          setIsConnectedToEsp32(true); // Successfully fetched status, so connected to ESP32 HTTP server
          setIsBluetoothConnected(data.bluetooth_connected);
          setEsp32WifiMode(data.wifi_mode);
          setAutoRedialEnabled(data.auto_redial_enabled); // Update auto redial state
          setRedialPeriod(data.redial_period); // Update redial period state
          // If ESP32 is in STA mode, update IP to the one reported by ESP32 (if available)
          if (data.wifi_mode === 'STA' && data.ip_address) {
            setEsp32Ip(data.ip_address);
          }
        } else if (endpoint === 'configure_wifi') {
          // After configuring, assume ESP32 will reboot or switch, so clear connection status and let status polling update IP
          setStatusMessage('Wi-Fi configured. ESP32 is switching to home network. The IP address will be automatically updated when the device reconnects.');
          setIsConnectedToEsp32(false);
          // Don't set a hardcoded IP - let the status polling update it automatically
        }
      } else {
        setStatusMessage(`Error sending "${endpoint}" command: ${data.error || response.statusText}`);
        setIsConnectedToEsp32(false); // If error, assume connection lost
      }
    } catch (error) {
      setStatusMessage(`Network error for "${endpoint}": ${error.message}. Ensure ESP32 IP is correct and device is reachable.`);
      setIsConnectedToEsp32(false); // If network error, assume connection lost
    }
  }, [esp32Ip]);

  // Handler for the "Redial Last Number" button
  const handleRedial = () => {
    sendCommand('redial');
  };

  // Handler for the "Dial Number" button
  const handleDial = () => {
    if (dialNumber.trim() === '') {
      setStatusMessage('Please enter a number to dial.');
      return;
    }
    sendCommand(`dial?number=${encodeURIComponent(dialNumber.trim())}`);
  };

  // Handler for configuring home Wi-Fi
  const handleConfigureWifi = () => {
    if (homeWifiSsid.trim() === '') {
      setStatusMessage('Please enter your home Wi-Fi SSID.');
      return;
    }
    sendCommand('configure_wifi', 'POST', {
      ssid: homeWifiSsid.trim(),
      password: homeWifiPassword.trim(),
    });
  };

  // Handler for toggling automatic redial
  const handleAutoRedialToggle = () => {
    const newEnabledState = !autoRedialEnabled;
    setAutoRedialEnabled(newEnabledState);
    sendCommand('set_auto_redial', 'POST', {
      enabled: newEnabledState,
      period: redialPeriod,
    });
  };

  // Handler for changing redial period
  const handleRedialPeriodChange = (e) => {
    let value = parseInt(e.target.value, 10);
    if (isNaN(value)) {
      value = 10; // Default to min if input is empty or invalid
    }
    // Clamp value between 10 and 84600
    value = Math.max(10, Math.min(value, 84600));
    setRedialPeriod(value);

    // Only send command if auto redial is enabled, otherwise it will be sent when toggled
    if (autoRedialEnabled) {
      sendCommand('set_auto_redial', 'POST', {
        enabled: autoRedialEnabled,
        period: value,
      });
    }
  };

  // Handler for checking ESP32 status
  const checkStatus = useCallback(() => {
    sendCommand('status');
  }, [sendCommand]);

  // Check status automatically when component mounts and every 5 seconds
  useEffect(() => {
    checkStatus(); // Initial check
    const interval = setInterval(checkStatus, 5000); // Check every 5 seconds
    return () => clearInterval(interval); // Cleanup interval on unmount
  }, [esp32Ip, checkStatus]); // Added checkStatus to dependency array

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-100 to-purple-200 flex items-center justify-center p-4 font-sans">
      <div className="bg-white p-8 rounded-xl shadow-2xl w-full max-w-md border border-gray-200">
        <h1 className="text-3xl font-bold text-center text-gray-800 mb-6">
          ESP32 Headset Controller
        </h1>

        {/* ESP32 IP Address Input */}
        <div className="mb-4">
          <label htmlFor="esp32-ip" className="block text-sm font-medium text-gray-700 mb-2">
            Current ESP32 IP Address:
          </label>
          <input
            type="text"
            id="esp32-ip"
            className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-blue-500 focus:border-blue-500 transition duration-150 ease-in-out shadow-sm"
            value={esp32Ip}
            onChange={(e) => setEsp32Ip(e.target.value)}
            placeholder="e.g., 192.168.4.1 (AP) or 192.168.1.100 (STA)"
          />
        </div>

        {/* ESP32 Wi-Fi Mode & Bluetooth Connection Status */}
        <div className="mb-6 text-center space-y-2">
          <p className="text-sm font-medium text-gray-700">
            ESP32 Wi-Fi Mode: <span className={`px-3 py-1 rounded-full text-xs font-semibold ${esp32WifiMode === 'AP' ? 'bg-orange-100 text-orange-800' : 'bg-blue-100 text-blue-800'}`}>{esp32WifiMode}</span>
          </p>
          <p className="text-sm font-medium text-gray-700">
            ESP32 Bluetooth Status: <span className={`px-3 py-1 rounded-full text-xs font-semibold ${isBluetoothConnected ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>{isBluetoothConnected ? 'Connected to Phone' : 'Disconnected from Phone'}</span>
          </p>
          <p className="text-sm font-medium text-gray-700">
            App connected to ESP32: <span className={`px-3 py-1 rounded-full text-xs font-semibold ${isConnectedToEsp32 ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>{isConnectedToEsp32 ? 'Yes' : 'No'}</span>
          </p>
        </div>

        {/* Wi-Fi Configuration Section */}
        <div className="bg-gray-50 p-6 rounded-lg mb-6 border border-gray-200">
          <h2 className="text-xl font-bold text-gray-700 mb-4 text-center">
            Configure Home Wi-Fi
          </h2>
          <p className="text-sm text-gray-600 mb-4">
            First, connect your device to the ESP32's Wi-Fi network: <strong className="text-blue-700">"REMOTEHEAD"</strong> (no password).
            Then, enter your home Wi-Fi details below.
          </p>
          <div className="mb-3">
            <label htmlFor="home-ssid" className="block text-sm font-medium text-gray-700 mb-1">
              Home Wi-Fi SSID:
            </label>
            <input
              type="text"
              id="home-ssid"
              className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-blue-500 focus:border-blue-500 shadow-sm"
              value={homeWifiSsid}
              onChange={(e) => setHomeWifiSsid(e.target.value)}
              placeholder="Your Home Wi-Fi Name"
            />
          </div>
          <div className="mb-4">
            <label htmlFor="home-password" className="block text-sm font-medium text-gray-700 mb-1">
              Home Wi-Fi Password:
            </label>
            <input
              type="password"
              id="home-password"
              className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-blue-500 focus:border-blue-500 shadow-sm"
              value={homeWifiPassword}
              onChange={(e) => setHomeWifiPassword(e.target.value)}
              placeholder="Your Home Wi-Fi Password"
            />
          </div>
          <button
            onClick={handleConfigureWifi}
            className="w-full bg-green-600 text-white py-3 px-6 rounded-lg shadow-md hover:bg-green-700 focus:outline-none focus:ring-2 focus:ring-green-500 focus:ring-offset-2 transition duration-200 ease-in-out transform hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
            disabled={!isConnectedToEsp32 || homeWifiSsid.trim() === ''}
          >
            Configure ESP32 Wi-Fi
          </button>
        </div>

        {/* Automatic Redial Configuration */}
        <div className="bg-gray-50 p-6 rounded-lg mb-6 border border-gray-200">
          <h2 className="text-xl font-bold text-gray-700 mb-4 text-center">
            Automatic Redial Settings
          </h2>
          <div className="flex items-center mb-4">
            <input
              type="checkbox"
              id="auto-redial-checkbox"
              className="h-5 w-5 text-blue-600 rounded border-gray-300 focus:ring-blue-500 mr-2"
              checked={autoRedialEnabled}
              onChange={handleAutoRedialToggle}
              disabled={!isConnectedToEsp32 || esp32WifiMode !== 'STA'} // Only enabled if connected and in STA mode
            />
            <label htmlFor="auto-redial-checkbox" className="text-sm font-medium text-gray-700">
              Enable Automatic Redial
            </label>
          </div>
          <div className="mb-4">
            <label htmlFor="redial-period" className="block text-sm font-medium text-gray-700 mb-1">
              Redial Period (seconds):
            </label>
            <input
              type="number"
              id="redial-period"
              className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-blue-500 focus:border-blue-500 shadow-sm"
              value={redialPeriod}
              onChange={handleRedialPeriodChange}
              min="10"
              max="84600"
              step="1"
              disabled={!isConnectedToEsp32 || esp32WifiMode !== 'STA'} // Only enabled if connected and in STA mode
            />
            <p className="text-xs text-gray-500 mt-1">
              Minimum: 10 seconds, Maximum: 84600 seconds (1 day)
            </p>
          </div>
        </div>


        {/* Action Buttons (for Redial/Dial) */}
        <div className="space-y-4 mb-6">
          <button
            onClick={handleRedial}
            className="w-full bg-blue-600 text-white py-3 px-6 rounded-lg shadow-md hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2 transition duration-200 ease-in-out transform hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
            disabled={!isBluetoothConnected || esp32WifiMode !== 'STA'} // Only enable if BT connected AND in STA mode
          >
            Redial Last Number
          </button>

          <div className="flex items-center space-x-2">
            <input
              type="tel" // Use type="tel" for phone numbers
              className="flex-grow px-4 py-2 border border-gray-300 rounded-lg focus:ring-blue-500 focus:border-blue-500 transition duration-150 ease-in-out shadow-sm"
              value={dialNumber}
              onChange={(e) => setDialNumber(e.target.value)}
              placeholder="e.g., +447911123456, *123#, 555-1234" // Updated placeholder
              disabled={!isBluetoothConnected || esp32WifiMode !== 'STA'} // Only enable if BT connected AND in STA mode
            />
            <button
              onClick={handleDial}
              className="bg-purple-600 text-white py-2.5 px-5 rounded-lg shadow-md hover:bg-purple-700 focus:outline-none focus:ring-2 focus:ring-purple-500 focus:ring-offset-2 transition duration-200 ease-in-out transform hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed"
              disabled={!isBluetoothConnected || dialNumber.trim() === '' || esp32WifiMode !== 'STA'} // Only enable if BT connected AND in STA mode
            >
              Dial
            </button>
          </div>
        </div>

        {/* Status Message Display */}
        <div className="mt-4 p-3 bg-gray-50 rounded-lg border border-gray-100 text-sm text-gray-600">
          <p className="font-semibold">Status:</p>
          <p>{statusMessage}</p>
        </div>

        {/* Instructions */}
        <div className="mt-6 text-xs text-gray-500 text-center">
          <p>Initially, connect your phone to the ESP32's Wi-Fi AP named "REMOTEHEAD" (no password). Its IP will be 192.168.4.1.</p>
          <p>Once configured, the ESP32 will connect to your home Wi-Fi. You'll then need to reconnect your phone to your home Wi-Fi. The ESP32's new IP address will be automatically detected and updated.</p>
          <p>The ESP32 must always be paired with your mobile phone as a Bluetooth headset for redial/dial commands to work.</p>
        </div>
      </div>
    </div>
  );
};

export default App;
