class LogViewer {
  constructor() {
      this.logContainer = document.getElementById('logContainer');
      this.autoScrollBtn = document.getElementById('autoScrollBtn');
      this.autoScroll = true;
      this.maxLogEntries = 100;
      this.logLevel = 'INFO';
      this.reconnectAttempts = 0;
      this.maxReconnectAttempts = 5;
      this.reconnectDelay = 3000; // 3 seconds
      this.ws = null;
      this.levelColors = {
          debug: '#569cd6',    // Light blue
          info: '#6a9955',     // Green
          warning: '#dcdcaa',  // Yellow
          error: '#f44747',    // Red
          system: '#808080'    // Gray
      };

      this.initWebSocket();
      this.setupEventListeners();
  }

  setupEventListeners() {
    // Auto-scroll button handler
    if (this.autoScrollBtn) {
        this.autoScrollBtn.addEventListener('click', () => {
            this.autoScroll = !this.autoScroll;
            this.autoScrollBtn.textContent = `Auto-scroll: ${this.autoScroll ? 'ON' : 'OFF'}`;
            if (this.autoScroll) {
                requestAnimationFrame(() => {
                    this.logContainer.scrollTop = this.logContainer.scrollHeight;
                });
            }
        });
    }

    // Log container scroll handler
    if (this.logContainer) {
        this.logContainer.addEventListener('scroll', () => {
            // Calculate if we're near the bottom (within 30px)
            const isNearBottom =
                this.logContainer.scrollHeight - this.logContainer.clientHeight - this.logContainer.scrollTop <= 30;

            // Only update autoScroll if we're actually scrolling manually
            if (!this.autoScroll && isNearBottom) {
                this.autoScroll = true;
                this.autoScrollBtn.textContent = 'Auto-scroll: ON';
            } else if (this.autoScroll && !isNearBottom) {
                this.autoScroll = false;
                this.autoScrollBtn.textContent = 'Auto-scroll: OFF';
            }
        });
    }
  }

  initWebSocket() {
    // Close existing connection if any
    if (this.ws) {
        this.ws.close();
        this.ws = null;
    }

    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProtocol}//${window.location.hostname}:81`;
    console.log('Attempting WebSocket connection...');

    try {
        this.ws = new WebSocket(wsUrl);

        // Add a timeout for the connection attempt
        const connectionTimeout = setTimeout(() => {
            if (this.ws && this.ws.readyState !== WebSocket.OPEN) {
                console.error('WebSocket connection timeout');
                this.ws.close();
            }
        }, 5000);

        this.ws.addEventListener('open', () => {
            clearTimeout(connectionTimeout);
            console.log('WebSocket connected successfully');
            this.addLogEntry('system', 'WebSocket connected');
            const wsStatus = document.getElementById('wsStatusCard');
            wsStatus.className = 'ws-status connected';
            wsStatus.textContent = 'Connected';
            this.reconnectAttempts = 0;

            // Send initialization message
            this.sendMessage('init', 'log_client');
        });

        this.ws.addEventListener('message', (event) => {
            try {
                const data = JSON.parse(event.data);
                console.log('Received message:', data);

                if (data.type === 'cleanup') {
                    this.logContainer.innerHTML = '';
                    return;
                }

                // Handle heartbeat
                if (data.type === 'heartbeat') {
                    this.ws.send(JSON.stringify({ type: 'pong' }));
                    return;
                }

                // Handle real-time log entries (type: 'log')
                if (data.type === 'log' && data.level && data.message) {
                    const level = (data.level || '').toLowerCase();
                    this.addLogEntry(level, data.message, data.color);
                    return;
                }

                // Optionally handle warnings or errors
                if (data.type === 'warning' && data.message) {
                    this.addLogEntry('warning', data.message, '#dcdcaa');
                    return;
                }
                if (data.type === 'error' && data.message) {
                    this.addLogEntry('error', data.message, '#f44747');
                    return;
                }

                if (data.type === 'log_level_changed' && data.data) {
                    updateLogLevelButtons(data.data);
                    return;
                }

            } catch (error) {
                console.error('Error processing message:', error);
                console.log('Raw message:', event.data);
            }
        });

        this.ws.addEventListener('error', (error) => {
            console.error('WebSocket error:', error);
            const wsStatus = document.getElementById('wsStatusCard');
            wsStatus.className = 'ws-status error';
            wsStatus.textContent = 'Error';
            this.addLogEntry('error', 'WebSocket error occurred');
        });

        this.ws.addEventListener('close', (event) => {
            console.log('WebSocket closed:', event);
            const wsStatus = document.getElementById('wsStatusCard');
            wsStatus.className = 'ws-status disconnected';
            wsStatus.textContent = 'Disconnected';
            this.addLogEntry('system', `WebSocket disconnected (code: ${event.code})`);

            // Attempt to reconnect if not at max attempts
            if (this.reconnectAttempts < this.maxReconnectAttempts) {
                this.reconnectAttempts++;
                const delay = this.reconnectDelay * this.reconnectAttempts;
                this.addLogEntry('system', `Reconnecting in ${delay/1000}s (attempt ${this.reconnectAttempts})`);
                setTimeout(() => this.initWebSocket(), delay);
            } else {
                this.addLogEntry('error', 'Max reconnection attempts reached');
            }
        });

        // Update status display
        const wsStatus = document.getElementById('wsStatusCard');
        wsStatus.className = 'ws-status';
        wsStatus.textContent = 'Connecting...';

    } catch (error) {
        console.error('Error creating WebSocket:', error);
        this.addLogEntry('error', `Failed to create WebSocket: ${error.message}`);
        const wsStatus = document.getElementById('wsStatusCard');
        wsStatus.className = 'ws-status error';
        wsStatus.textContent = 'Failed';
    }
}

// Helper method to send messages
sendMessage(type, data) {
  if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      try {
          const message = JSON.stringify({ type, data });
          this.ws.send(message);
      } catch (error) {
          console.error('Error sending message:', error);
      }
  }
}

addLogEntry(level, message, color = null) {
  const entry = document.createElement('div');
  entry.className = `log-entry ${level}`;

  // Create timestamp
  const timestamp = new Date().toLocaleTimeString('de-DE', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
  });

  // Create timestamp span
  const timestampSpan = document.createElement('span');
  timestampSpan.textContent = timestamp;
  timestampSpan.style.color = '#808080';

  // Create message span
  const messageSpan = document.createElement('span');
  messageSpan.textContent = ` ${message}`;
  messageSpan.style.color = this.levelColors[level] || color || 'inherit';

  // Assemble entry (no level tag at all)
  entry.appendChild(timestampSpan);
  entry.appendChild(document.createTextNode(' '));
  entry.appendChild(messageSpan);

  this.logContainer.appendChild(entry);

  // Maintain maximum entries
  while (this.logContainer.children.length > this.maxLogEntries) {
      this.logContainer.removeChild(this.logContainer.firstChild);
  }

  // Fix autoscroll: Only scroll if we're already at the bottom or autoscroll is enabled
  if (this.autoScroll) {
      // Use requestAnimationFrame for smooth scrolling
      requestAnimationFrame(() => {
          this.logContainer.scrollTop = this.logContainer.scrollHeight;
      });
  }
}
}

function updateLogLevelButtons(activeLevel) {
  document.querySelectorAll('.log-level-btn').forEach(btn => {
    btn.classList.remove('active', 'pulse');
    const btnLevel = btn.textContent.trim().toUpperCase();
    if (btnLevel === activeLevel.toUpperCase()) {
      btn.classList.add('active', 'pulse');
      // Remove pulse after animation
      setTimeout(() => btn.classList.remove('pulse'), 350);
    }
  });
}

// Initialize on load
window.addEventListener('load', () => {
  window.logViewer = new LogViewer();
  updateLogLevelButtons('INFO');
});

// Add global function for log level setting
window.setLogLevel = function(level) {
  if (window.logViewer && window.logViewer.ws) {
      window.logViewer.ws.send(JSON.stringify({
          type: 'log_level',
          data: level
      }));
      updateLogLevelButtons(level); // Immediate feedback
  }
};

// Also update on backend confirmation (log_level_changed)
const origAddLogEntry = LogViewer.prototype.addLogEntry;
LogViewer.prototype.addLogEntry = function(level, message, color) {
  // Listen for backend log_level_changed
  if (level === 'system' && typeof message === 'string' && message.includes('log_level_changed')) {
    try {
      const match = message.match(/log_level_changed.*"data":"(\w+)"/);
      if (match && match[1]) {
        updateLogLevelButtons(match[1]);
      }
    } catch (e) {}
  }
  origAddLogEntry.call(this, level, message, color);
};
