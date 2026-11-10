"""Open native USB CDC without toggling ESP32-S3 reset control lines."""
import serial


def open_device(path, timeout=0.2):
    # dsrdtr/rtscts suppress pyserial's explicit DTR/RTS ioctls on open.
    port = serial.Serial(path, 115200, timeout=timeout, rtscts=True, dsrdtr=True)
    try:
        import termios
        attributes = termios.tcgetattr(port.fileno())
        attributes[2] &= ~termios.HUPCL
        termios.tcsetattr(port.fileno(), termios.TCSANOW, attributes)
    except (ImportError, OSError):
        pass
    return port
