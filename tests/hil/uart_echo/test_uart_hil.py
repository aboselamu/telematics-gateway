import serial
import time
import sys

# --- CONFIGURATION ---
PORT = 'COM9'      # Windows: 'COM3', Linux: '/dev/ttyACM0', Mac: '/dev/cu.usbmodem...'
BAUD_RATE = 115200
TIMEOUT_SEC = 1.0  # How long to wait for the board to reply

def run_hil_test():
    print(f"Connecting to Gateway on {PORT} at {BAUD_RATE} baud...\n")
    
    try:
        # Open the serial connection
        with serial.Serial(PORT, BAUD_RATE, timeout=TIMEOUT_SEC) as gateway:
            time.sleep(2) # Give the board 2 seconds to boot up
            
            # ---------------------------------------------------------
            # TEST 1: The Basic Echo
            # ---------------------------------------------------------
            # --- Replace the Test 1 logic in your Python script ---
            print("Sending Raw GPS NMEA Sentence...")

            # A real example of an NMEA GGA sentence
            gps_sentence = b"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n"

            gateway.write(gps_sentence)
            reply = gateway.readline()

            print(f"Sent: {gps_sentence}")
            print(f"Echoed by Board: {reply}")

            if reply == gps_sentence:
                print(" -> [PASS] Board successfully passed through the GPS data.")
            else:
                print(" -> [FAIL] Data mismatch!")

            # ---------------------------------------------------------
            # TEST 2: The Rapid Fire Stress Test
            # ---------------------------------------------------------
            print("Running Test 2: Rapid Fire (Stress Test)...")
            success_count = 0
            
            # Send 50 messages as fast as possible
            for i in range(500):
                msg = f"MSG_NUMBER_{i}\n".encode('utf-8')
                gateway.write(msg)
                
                reply = gateway.readline()
                if reply == msg:
                    success_count += 1
                    
            if success_count == 500:
                print(" -> [PASS] Board handled 500 rapid-fire messages perfectly.\n")
            else:
                print(f" -> [FAIL] Board missed frames! Only caught {success_count}/500.\n")

    except serial.SerialException as e:
        print(f"Error: Could not open {PORT}. Is the board plugged in or used by another program?")
        print(e)
        sys.exit(1)

if __name__ == "__main__":
    run_hil_test()