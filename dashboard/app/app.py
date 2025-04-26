from flask import Flask, render_template, send_from_directory, request, jsonify
from flask_socketio import SocketIO
import os

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")
IMAGE_DIR = "static/images"

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/images/<filename>')
def get_image(filename):
    return send_from_directory(IMAGE_DIR, filename)

def emit_new_image(filename):
    socketio.emit("new_image", {"filename": filename})
    print(f"📡 Emitted: {filename}")

@app.route('/notify', methods=["POST"])
def notify():
    data = request.get_json()
    filename = data["filename"]
    emit_new_image(filename)
    return jsonify({"status": "ok"})

@app.route('/images/list')
def list_images():
    files = sorted(os.listdir(IMAGE_DIR), reverse=True)
    jpgs = [f for f in files if f.endswith('.jpg')]
    return jsonify(jpgs)

if __name__ == '__main__':
    socketio.run(app, host="0.0.0.0", port=5000, debug=True, allow_unsafe_werkzeug=True)