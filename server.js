const WebSocket = require('ws');

// Create WebSocket server on port 8080
const wss = new WebSocket.Server({ port: 8080 });

// Handle incoming connections
wss.on('connection', (ws) => {
    console.log('A new client connected');

    // Listen for messages from the client
    ws.on('message', (message) => {
        console.log(`Received: ${message}`);
        // Echo the message back to the client
        ws.send(`Hello, you sent: ${message}`);
    });

    // Send a welcome message to the client
    ws.send('Welcome to the WebSocket server!');
});

console.log('WebSocket server is running on ws://localhost:8080');
