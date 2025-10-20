"""
Local UE5 Web Server - Enables browser → UE5 communication
Turns UE5 into a controllable server that responds to dashboard commands.

Usage in UE5:
    import AIAssistant.local_server
    AIAssistant.local_server.start_server()
"""
import json
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

import unreal


class UE5CommandHandler(BaseHTTPRequestHandler):
    """Handles HTTP requests from the browser dashboard."""
    
    def do_GET(self):
        """Handle GET requests."""
        if self.path == '/health':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            response = {
                'status': 'running',
                'project': unreal.SystemLibrary.get_game_name(),
                'engine_version': unreal.SystemLibrary.get_engine_version()
            }
            self.wfile.write(json.dumps(response).encode())
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        """Handle POST requests from dashboard."""
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode()
        
        try:
            data = json.loads(body) if body else {}
            command = self.path.strip('/')
            
            # CORS headers
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Access-Control-Allow-Methods', 'POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers', 'Content-Type')
            
            if command == 'execute':
                # Execute AI command
                user_input = data.get('command', '')
                from ..core.main import send_command
                response = send_command(user_input)
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({
                    'success': True,
                    'response': response
                }).encode())
                
            elif command == 'register':
                # Register project
                from ..network.api_client import get_client
                from ..system.project_registration import auto_register_project
                
                result = auto_register_project(get_client())
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(result).encode())
                
            elif command == 'update':
                # Auto-update client
                from . import auto_update
                success = auto_update.check_and_update()
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({
                    'success': success
                }).encode())
            
            else:
                self.send_response(404)
                self.end_headers()
                
        except Exception as e:
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({
                'error': str(e)
            }).encode())
    
    def do_OPTIONS(self):
        """Handle CORS preflight."""
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
    
    def log_message(self, format, *args):
        """Suppress default logging."""
        pass


_server = None
_server_thread = None

def start_server(port=8765):
    """Start local UE5 web server for browser communication."""
    global _server, _server_thread
    
    if _server:
        unreal.log("⚠️  Server already running")
        return
    
    try:
        _server = HTTPServer(('localhost', port), UE5CommandHandler)
        
        def run():
            unreal.log(f"🌐 UE5 Local Server started on http://localhost:{port}")
            unreal.log("📡 Browser dashboard can now send commands to UE5!")
            unreal.log("💡 Configure dashboard to use: http://localhost:8765")
            _server.serve_forever()
        
        _server_thread = threading.Thread(target=run, daemon=True)
        _server_thread.start()
        
        return True
    except Exception as e:
        unreal.log_error(f"❌ Failed to start server: {e}")
        return False


def stop_server():
    """Stop the local server."""
    global _server, _server_thread
    
    if _server:
        _server.shutdown()
        _server = None
        _server_thread = None
        unreal.log("🛑 UE5 Local Server stopped")
        return True
    return False
