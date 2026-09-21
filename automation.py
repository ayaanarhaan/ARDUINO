import cv2
import serial
import time
import sqlite3
import pytesseract
import face_recognition

pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'

# Connect to ESP32
try:
    esp32 = serial.Serial('COM3', 115200, timeout=1)
    time.sleep(2)
    print("ESP32 Connected.")
except Exception as e:
    print(f"Serial Error: {e}")
    esp32 = None

# Offline Local SQLite Database
conn = sqlite3.connect('local_attendance.db')
cursor = conn.cursor()
cursor.execute('''
    CREATE TABLE IF NOT EXISTS attendance (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        student_name TEXT,
        date DATE DEFAULT CURRENT_DATE,
        time TIME DEFAULT CURRENT_TIME
    )
''')
conn.commit()

# Load Profiles
known_encodings = []
known_names = []

def register_student(img_path, name):
    try:
        img = face_recognition.load_image_file(img_path)
        encoding = face_recognition.face_encodings(img)[0]
        known_encodings.append(encoding)
        known_names.append(name)
        print(f"Loaded Profile: {name}")
    except Exception as e:
        print(f"Error loading {name}: {e}")

register_student("arhaan.jpg", "ARHAAN AJEESH")

cap = cv2.VideoCapture(0)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    cv2.imshow("Robot Camera View", frame)

    if esp32 and esp32.in_waiting > 0:
        cmd = esp32.readline().decode('utf-8').strip()

        if cmd == "START_SCAN":
            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            faces = face_recognition.face_encodings(rgb_frame)

            matched_name = None
            for face in faces:
                matches = face_recognition.compare_faces(known_encodings, face, tolerance=0.5)
                if True in matches:
                    matched_name = known_names[matches.index(True)]
                    break

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            ocr_text = pytesseract.image_to_string(gray).upper()

            if matched_name and (matched_name in ocr_text or len(faces) > 0):
                cursor.execute("SELECT id FROM attendance WHERE student_name=? AND date=CURRENT_DATE", (matched_name,))
                if cursor.fetchone():
                    esp32.write("DUPLICATE\n".encode('utf-8'))
                else:
                    cursor.execute("INSERT INTO attendance (student_name) VALUES (?)", (matched_name,))
                    conn.commit()
                    esp32.write(f"SUCCESS:{matched_name}\n".encode('utf-8'))
            else:
                esp32.write("UNKNOWN\n".encode('utf-8'))

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
conn.close()
