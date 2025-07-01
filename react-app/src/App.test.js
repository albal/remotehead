import React from 'react';
import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import App from './App';

// Mock fetch globally
global.fetch = jest.fn();

describe('App Component', () => {
  // Store original window.location
  const originalLocation = window.location;

  beforeEach(() => {
    fetch.mockClear();
    
    // Reset window.location to localhost for most tests
    delete window.location;
    window.location = { hostname: 'localhost' };
    
    // Default mock response
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: false,
        wifi_mode: 'AP',
        ip_address: '192.168.4.1',
        auto_redial_enabled: false,
        redial_period: 60,
        message: 'ESP32 Bluetooth disconnected.'
      })
    });
  });

  afterEach(() => {
    // Restore original window.location
    window.location = originalLocation;
  });

  test('renders main UI elements', async () => {
    render(<App />);
    
    // Wait for component to load
    await waitFor(() => {
      expect(screen.getByText('ESP32 Headset Controller')).toBeInTheDocument();
    });
    
    expect(screen.getByLabelText(/Current ESP32 IP Address/i)).toBeInTheDocument();
    expect(screen.getByText('Configure Home Wi-Fi')).toBeInTheDocument();
    expect(screen.getByText('Automatic Redial Settings')).toBeInTheDocument();
    expect(screen.getByText('Redial Last Number')).toBeInTheDocument();
    expect(screen.getByText('Dial')).toBeInTheDocument();
  });

  test('displays status information correctly', async () => {
    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByText('AP')).toBeInTheDocument();
      expect(screen.getByText('Disconnected from Phone')).toBeInTheDocument();
    });
  });

  test('allows IP address input changes', async () => {
    const user = userEvent.setup();
    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByDisplayValue('192.168.4.1')).toBeInTheDocument();
    });

    const ipInput = screen.getByLabelText(/Current ESP32 IP Address/i);
    
    await user.clear(ipInput);
    await user.type(ipInput, '192.168.1.100');
    
    expect(ipInput).toHaveValue('192.168.1.100');
  });

  test('makes initial status API call', async () => {
    render(<App />);
    
    await waitFor(() => {
      expect(fetch).toHaveBeenCalledWith('http://192.168.4.1/status', {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
        },
      });
    });
  });

  test('handles successful WiFi configuration', async () => {
    const user = userEvent.setup();
    
    // Mock successful configuration response
    fetch
      .mockResolvedValueOnce({
        ok: true,
        json: async () => ({
          bluetooth_connected: false,
          wifi_mode: 'AP',
          ip_address: '192.168.4.1',
          auto_redial_enabled: false,
          redial_period: 60,
          message: 'ESP32 Bluetooth disconnected.'
        })
      })
      .mockResolvedValueOnce({
        ok: true,
        json: async () => ({ message: 'Wi-Fi configured successfully' })
      });

    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByText('ESP32 Headset Controller')).toBeInTheDocument();
    });

    const ssidInput = screen.getByLabelText(/Home Wi-Fi SSID/i);
    const passwordInput = screen.getByLabelText(/Home Wi-Fi Password/i);
    const configureButton = screen.getByText('Configure ESP32 Wi-Fi');
    
    await user.type(ssidInput, 'TestNetwork');
    await user.type(passwordInput, 'testpassword');
    await user.click(configureButton);

    await waitFor(() => {
      expect(fetch).toHaveBeenCalledWith('http://192.168.4.1/configure_wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: 'TestNetwork', password: 'testpassword' }),
      });
    });
  });

  test('disables controls in AP mode', async () => {
    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByText('AP')).toBeInTheDocument();
    });

    const redialButton = screen.getByText('Redial Last Number');
    const dialButton = screen.getByText('Dial');
    const phoneInput = screen.getByPlaceholderText(/e.g., \+447911123456/i);
    const autoRedialCheckbox = screen.getByLabelText(/Enable Automatic Redial/i);

    expect(redialButton).toBeDisabled();
    expect(dialButton).toBeDisabled();
    expect(phoneInput).toBeDisabled();
    expect(autoRedialCheckbox).toBeDisabled();
  });

  test('enables controls when bluetooth connected in STA mode', async () => {
    // Mock response for STA mode with bluetooth connected
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: true,
        wifi_mode: 'STA',
        ip_address: '192.168.1.100',
        auto_redial_enabled: false,
        redial_period: 60,
        message: 'ESP32 connected.'
      })
    });

    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByText('STA')).toBeInTheDocument();
      expect(screen.getByText('Connected to Phone')).toBeInTheDocument();
    });

    const redialButton = screen.getByText('Redial Last Number');
    const phoneInput = screen.getByPlaceholderText(/e.g., \+447911123456/i);
    const autoRedialCheckbox = screen.getByLabelText(/Enable Automatic Redial/i);

    expect(redialButton).not.toBeDisabled();
    expect(phoneInput).not.toBeDisabled();
    expect(autoRedialCheckbox).not.toBeDisabled();
  });

  test('automatically updates IP address when ESP32 reports new IP in STA mode', async () => {
    // Mock response for STA mode with a specific IP address
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: false,
        wifi_mode: 'STA',
        ip_address: '192.168.1.55',
        auto_redial_enabled: false,
        redial_period: 60,
        message: 'ESP32 connected to home network.'
      })
    });

    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByText('STA')).toBeInTheDocument();
      // Check that the IP input field has been updated to the ESP32's reported IP
      expect(screen.getByDisplayValue('192.168.1.55')).toBeInTheDocument();
    });
  });

  test('handles API errors gracefully', async () => {
    fetch.mockResolvedValue({
      ok: false,
      statusText: 'Internal Server Error',
      json: async () => ({ error: 'Something went wrong' })
    });

    render(<App />);

    await waitFor(() => {
      expect(screen.getByText(/Error sending "status" command/i)).toBeInTheDocument();
    });
  });

  test('handles network errors gracefully', async () => {
    fetch.mockRejectedValue(new Error('Network error'));

    render(<App />);

    await waitFor(() => {
      expect(screen.getByText(/Network error for "status"/i)).toBeInTheDocument();
    });
  });

  test('displays connection status badges correctly', async () => {
    render(<App />);
    
    await waitFor(() => {
      // Check that status badges are displayed
      expect(screen.getByText('AP')).toBeInTheDocument();
      expect(screen.getByText('Disconnected from Phone')).toBeInTheDocument();
      expect(screen.getByText('Yes')).toBeInTheDocument(); // App connected to ESP32
    });
  });

  test('renders WiFi configuration form', async () => {
    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByLabelText(/Home Wi-Fi SSID/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/Home Wi-Fi Password/i)).toBeInTheDocument();
      expect(screen.getByText('Configure ESP32 Wi-Fi')).toBeInTheDocument();
    });
  });

  test('automatically detects IP from browser URL when not on localhost', async () => {
    // Mock window.location for a real ESP32 IP
    delete window.location;
    window.location = { hostname: '192.168.1.100' };

    render(<App />);
    
    await waitFor(() => {
      // Check that the IP input field shows the detected IP from the URL
      expect(screen.getByDisplayValue('192.168.1.100')).toBeInTheDocument();
    });

    // Verify that the status API call was made to the detected IP
    await waitFor(() => {
      expect(fetch).toHaveBeenCalledWith('http://192.168.1.100/status', {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
        },
      });
    });
  });

  test('uses default IP when on localhost', async () => {
    // localhost is already set in beforeEach
    render(<App />);
    
    await waitFor(() => {
      // Check that the IP input field shows the default IP
      expect(screen.getByDisplayValue('192.168.4.1')).toBeInTheDocument();
    });
  });

  test('renders automatic redial settings', async () => {
    render(<App />);
    
    await waitFor(() => {
      expect(screen.getByLabelText(/Enable Automatic Redial/i)).toBeInTheDocument();
      expect(screen.getByLabelText(/Redial Period \(seconds\)/i)).toBeInTheDocument();
    });
  });
});

// --- Tests for Random Delay Spinner and Readout ---
describe('Random Delay Spinner and Readout', () => {
  beforeEach(() => {
    fetch.mockClear();
  });

  test('renders random delay spinner and last random value', async () => {
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: true,
        wifi_mode: 'STA',
        ip_address: '192.168.1.100',
        auto_redial_enabled: true,
        redial_period: 60,
        redial_random_delay: 15,
        last_random_delay: 7,
        message: 'ESP32 connected.'
      })
    });
    render(<App />);
    await waitFor(() => {
      expect(screen.getByLabelText(/Random Extra Delay/i)).toHaveValue(15);
    });
    expect(screen.getByText(/Last random value used:/i)).toBeInTheDocument();
    expect(
      screen.getAllByText((content, node) =>
        node.tagName === 'P' && node.textContent.trim().endsWith('7 seconds')
      ).length
    ).toBeGreaterThan(0);
  });

  test('changing random delay spinner sends correct API call', async () => {
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: true,
        wifi_mode: 'STA',
        ip_address: '192.168.1.100',
        auto_redial_enabled: true,
        redial_period: 60,
        redial_random_delay: 10,
        last_random_delay: 3,
        message: 'ESP32 connected.'
      })
    });
    render(<App />);
    await waitFor(() => {
      expect(screen.getByLabelText(/Random Extra Delay/i)).toBeInTheDocument();
    });
    const spinner = screen.getByLabelText(/Random Extra Delay/i);
    await userEvent.clear(spinner);
    await userEvent.type(spinner, '20');
    // Wait for debounce and API call
    await waitFor(() => {
      expect(fetch).toHaveBeenCalledWith(
        expect.stringContaining('/set_auto_redial'),
        expect.objectContaining({
          method: 'POST',
          body: expect.stringContaining('"random_delay":20')
        })
      );
    });
  });

  test('shows 0 if no last random value is reported', async () => {
    fetch.mockResolvedValue({
      ok: true,
      json: async () => ({
        bluetooth_connected: true,
        wifi_mode: 'STA',
        ip_address: '192.168.1.100',
        auto_redial_enabled: true,
        redial_period: 60,
        redial_random_delay: 0,
        last_random_delay: 0,
        message: 'ESP32 connected.'
      })
    });
    render(<App />);
    await waitFor(() => {
      expect(screen.getByText(/Last random value used:/i)).toBeInTheDocument();
      expect(screen.getByText(/0 seconds/)).toBeInTheDocument();
    });
  });
});