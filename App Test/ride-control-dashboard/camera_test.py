import cv2
from flask import Flask, Response

app = Flask(__name__)

# Initialize the Mac's built-in camera (0 is usually the default webcam)
camera = cv2.VideoCapture(0)

def generate_frames():
    while True:
        # Read the camera frame
        success, frame = camera.read()
        if not success:
            break
        else:
            # Encode the frame in JPEG format
            ret, buffer = cv2.imencode('.jpg', frame)
            frame = buffer.tobytes()
            
            # Yield the frame in byte format
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

@app.route('/stream')
def video_feed():
    # Return the response generated along with the specific media type (mime type)
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == "__main__":
    print("Starting camera stream...")
    print("Test it by going to: http://192.168.1.100/stream (assuming .100 is your Mac's IP)")
    
    # Running on port 80 (default HTTP port) so you don't have to specify a port in the URL.
    app.run(host='0.0.0.0', port=80)