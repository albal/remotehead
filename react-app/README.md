# React App Testing

This directory contains tests for the ESP32 Headset Controller React application.

## Running Tests

```bash
# Install dependencies
npm install

# Run tests once
npm test

# Run tests with coverage
npm test -- --coverage

# Run tests in CI mode (no watch)
npm test -- --watchAll=false --ci
```

## Test Coverage

The test suite covers:

- **UI Rendering**: Main components and elements are rendered correctly
- **Status Display**: Connection status indicators and badges work properly  
- **Configuration**: WiFi setup functionality and form validation
- **API Communication**: HTTP requests to ESP32 with proper error handling
- **State Management**: Button enable/disable logic based on connection state
- **User Interactions**: Input changes, button clicks, and form submissions

## GitHub Actions

Tests automatically run on:
- Pull requests to `main` branch
- Pushes to `main` branch  
- Manual workflow dispatch

The workflow:
1. Sets up Node.js 18
2. Installs dependencies
3. Runs tests with coverage
4. Uploads coverage reports

## Test Files

- `src/App.test.js` - Main test suite for the App component
- `src/setupTests.js` - Test setup and configuration