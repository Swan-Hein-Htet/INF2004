"""Render the documented wiring as SVG/PNG sheets and a printable PDF.

Requires reportlab and pdftoppm. Coordinates group contacts by function;
they deliberately do not imply a connector's physical contact order.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

from reportlab.graphics.shapes import Drawing, Rect, String, Line, Circle, PolyLine
from reportlab.graphics import renderPDF, renderSVG
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/wiring"
W, H = 1600, 1100
INK = "#182b42"
MUTED = "#53677d"
RED = "#c83d50"
ORANGE = "#b55b08"
GND = "#344154"
SIGNAL = "#007d79"
ANALOG = "#245cc4"
PURPLE = "#784ab1"
SHEETS = []

for name, filename in [("DV", "DejaVuSans.ttf"), ("DVB", "DejaVuSans-Bold.ttf")]:
    pdfmetrics.registerFont(TTFont(name, "/usr/share/fonts/truetype/dejavu/" + filename))


def text(d, x, y, value, size=25, color=INK, bold=False, anchor="start"):
    font = "DVB" if bold else "DV"
    width = pdfmetrics.stringWidth(value, font, size)
    left = x - (width if anchor == "end" else width / 2 if anchor == "middle" else 0)
    if left < 0 or left + width > W or not 0 < y < H:
        raise ValueError(f"Text outside sheet: {value}")
    d.add(String(x, H - y, value, fontName=font,
                 fontSize=size, fillColor=HexColor(color), textAnchor=anchor))


def box(d, x, y, width, height, fill="#ffffff", stroke="#ccd8e5", radius=18):
    d.add(Rect(x, H - y - height, width, height, rx=radius, ry=radius,
               fillColor=HexColor(fill), strokeColor=HexColor(stroke), strokeWidth=2))


def lines(d, x, y, values, size=24, color=MUTED, gap=36, bold=False):
    for i, value in enumerate(values):
        text(d, x, y + i * gap, value, size, color, bold)


def wire(d, x1, y1, x2, y2, color=SIGNAL, points=None, dashed=False):
    coords = points or [(x1, y1), (x2, y2)]
    flat = [v for x, y in coords for v in (x, H - y)]
    d.add(PolyLine(flat, strokeColor=HexColor(color), strokeWidth=5,
                   strokeDashArray=[10, 6] if dashed else None, fillColor=None))
    for x, y in [(x1, y1), (x2, y2)]:
        d.add(Circle(x, H - y, 6, fillColor=HexColor(color), strokeColor=None))


def note(d, y, title, detail, warn=False):
    box(d, 60, y, 1480, 96, "#fff3dd" if warn else "#eaf3fb",
        "#e1b461" if warn else "#c5d9ed")
    text(d, 85, y + 35, title, 25, INK, True)
    text(d, 85, y + 72, detail, 22, MUTED)


def sheet(number, title, subtitle, source):
    d = Drawing(W, H)
    box(d, 0, 0, W, H, "#f5f8fc", "#f5f8fc", 0)
    text(d, 60, 70, f"{number:02d}  /  {title}", 42, INK, True)
    text(d, 60, 120, subtitle, 25, MUTED)
    text(d, 60, 1030, "Follow printed pin labels. Contacts are grouped by function, not physical position.", 21, INK)
    text(d, 60, 1071, source, 18, MUTED)
    return d


def pair(d, y, title, board, rows, height=None, subtitle=None):
    """Rows are (module pin, board contact, color). No implied plug ordering."""
    height = height or 85 + len(rows) * 55
    box(d, 70, y, 475, height)
    box(d, 1055, y, 475, height, "#eaf0ff", "#aabfe7")
    text(d, 95, y + 43, title, 28, INK, True)
    text(d, 1080, y + 43, board, 27, INK, True)
    if subtitle:
        text(d, 95, y + 76, subtitle, 19, MUTED)
    row_start = y + (115 if subtitle else 100)
    for index, (module_pin, contact, color) in enumerate(rows):
        yy = row_start + index * 55
        text(d, 520, yy + 8, module_pin, 24, color, anchor="end")
        text(d, 1080, yy + 8, contact, 24, color)
        wire(d, 545, yy, 1055, yy, color)


def save(name, d):
    SHEETS.append((name, d))


def overview():
    d = sheet(1, "What connects where", "Start here. Each group has its own detailed sheet.",
              "ROBO-PICO datasheet pp. 3–4 + PicoCar slides + current board.c assignments")
    box(d, 560, 200, 480, 795, "#e9e8fb", "#b5b0de")
    text(d, 800, 248, "ROBO PICO", 35, PURPLE, True, "middle")
    text(d, 800, 280, "Carrier holding the Pico W", 20, MUTED, anchor="middle")
    box(d, 60, 295, 430, 230)
    text(d, 85, 340, "Three ST188 IR sensors", 27, INK, True)
    for i, (a, b) in enumerate([("Left line  ·  A0", "Grove 5 / GP26"),
                               ("Right line  ·  A0", "Grove 6 / GP27"),
                               ("Barcode  ·  A0", "Grove 7 / GP28")]):
        yy = 385 + i * 55
        text(d, 85, yy + 8, a, 24)
        wire(d, 490, yy, 560, yy, ANALOG)
        text(d, 580, yy + 8, b, 21, ANALOG, True)
    box(d, 60, 605, 430, 125)
    text(d, 85, 648, "GY-511 / OKY3231-2", 27, INK, True)
    text(d, 85, 693, "SDA + SCL", 25, SIGNAL)
    wire(d, 490, 678, 560, 678)
    lines(d, 580, 664, ["Grove 3", "GP4 / GP5"], 22, SIGNAL)
    box(d, 60, 815, 430, 125)
    text(d, 85, 858, "HC-SR04 ultrasonic", 27, INK, True)
    text(d, 85, 903, "TRIG + divided ECHO", 25, SIGNAL)
    wire(d, 490, 888, 560, 888)
    lines(d, 580, 875, ["Grove 2", "GP2 / GP3"], 22, SIGNAL)
    box(d, 1110, 295, 430, 230)
    text(d, 1135, 340, "Two encoder motors", 27, INK, True)
    for i, (a, b) in enumerate([("Grove 4", "A: GP16 / GP17"),
                               ("GPIO breakout", "B: GP13 / GP19"),
                               ("M1 / M2 terminals", "Motor power leads")]):
        yy = 385 + i * 55
        text(d, 1020, yy + 8, a, 20, PURPLE, True, "end")
        wire(d, 1040, yy, 1110, yy, PURPLE)
        text(d, 1135, yy + 8, b, 23)
    box(d, 1110, 605, 430, 125)
    text(d, 1135, 648, "SG90 scanner servo", 27, INK, True)
    text(d, 1135, 693, "Signal + V+ + GND", 25)
    wire(d, 1040, 678, 1110, 678, PURPLE)
    text(d, 1020, 664, "Servo port 1", 22, PURPLE, True, "end")
    text(d, 1020, 700, "GP12 signal", 22, PURPLE, anchor="end")
    box(d, 1110, 815, 430, 125)
    text(d, 1135, 858, "UART adapter (optional)", 25, INK, True)
    text(d, 1135, 903, "3.3 V logic only", 25)
    wire(d, 1040, 888, 1110, 888)
    text(d, 1020, 875, "Grove 1", 22, SIGNAL, True, "end")
    text(d, 1020, 911, "GP0 / GP1", 22, SIGNAL, anchor="end")
    text(d, 800, 969, "Buttons GP20 / GP21 are already wired.", 20, INK, anchor="middle")
    text(d, 60, 180, "Signal overview only. Follow sheet 07 for power and common ground.", 24, RED, True)
    save("01-overview", d)


def infrared():
    d = sheet(2, "ST188 sensors: three wires each", "Use A0 (analogue). Leave D0 (digital) disconnected.",
              "PicoCar slide 4 labels + ROBO-PICO Grove mapping + IR manual supply limits")
    for y, title, port, gp in [(175, "LEFT line sensor", 5, 26),
                                (420, "RIGHT line sensor", 6, 27),
                                (665, "BARCODE sensor", 7, 28)]:
        pair(d, y, title, f"Robo Pico · Grove {port}",
             [("VCC", "+3V3", RED), ("GND", "GND", GND),
              ("A0  ·  analogue", f"GP{gp}  ·  ADC{gp - 26}", ANALOG)], height=230)
    note(d, 910, "Disconnect unused signal wires; use a repinned/breakout cable.",
         "Grove 5: no GP6 wire. Grove 6: no GP26 wire. Grove 7: no GP7 wire. Open GP28 VBAT jumper.", True)
    save("02-ir-sensors", d)


def imu():
    d = sheet(3, "GY-511 / OKY3231-2 IMU", "LSM303DLHC accelerometer · I2C address 0x19", 
              "OKY3231-2 module sheet: 3–5 V input + PicoCar slide 5 labels + LSM303DLHC datasheet")
    pair(d, 205, "GY-511 module", "Robo Pico · Grove 3",
         [("VIN  ·  supply input", "+3V3", RED), ("GND", "GND", GND),
          ("SDA  ·  data", "GP4  ·  I2C0 SDA", SIGNAL),
          ("SCL  ·  clock", "GP5  ·  I2C0 SCL", SIGNAL)], height=350)
    note(d, 615, "Leave these module pins unconnected:",
         "3.3 pad, INT1, INT2 and DRDY. VIN and the separate 3.3 pad are not interchangeable labels.")
    note(d, 745, "Use this module's VIN input with the 3.3 V arrangement shown.",
         "Verify operation and pull-up voltage. Do not feed 5 V onto SDA or SCL; do not attach two supplies.", True)
    lines(d, 85, 920, ["The module has an accelerometer and magnetometer, not a gyroscope.",
                        "Use the printed SDA/SCL/VIN/GND labels; do not count positions from the drawing."], 25)
    save("03-imu", d)


def ultrasonic():
    d = sheet(4, "HC-SR04: ECHO needs two resistors", "HC-SR04 supply = regulated nominal 5 V. Pico signal level = 3.3 V.",
              "HC-SR04 datasheet: 5 V supply; RP2040/Robo Pico input limits; divider 15 / (10 + 15)")
    box(d, 70, 225, 425, 425)
    text(d, 95, 270, "HC-SR04 module", 30, INK, True)
    box(d, 1110, 225, 420, 425, "#eaf0ff", "#aabfe7")
    text(d, 1135, 270, "Robo Pico · Grove 2", 27, INK, True)
    text(d, 470, 353, "TRIG", 27, SIGNAL, anchor="end")
    text(d, 1135, 353, "GP2", 27, SIGNAL)
    wire(d, 495, 345, 1110, 345)
    text(d, 470, 463, "ECHO", 27, SIGNAL, anchor="end")
    text(d, 1135, 463, "GP3", 27, SIGNAL)
    wire(d, 495, 455, 610, 455)
    box(d, 610, 432, 165, 46, "#ffffff", SIGNAL, 0)
    text(d, 692, 466, "10 kΩ", 25, SIGNAL, anchor="middle")
    wire(d, 775, 455, 1110, 455)
    wire(d, 925, 455, 925, 515)
    box(d, 900, 515, 50, 100, "#ffffff", GND, 0)
    text(d, 974, 575, "15 kΩ", 25, GND)
    text(d, 470, 573, "VCC", 27, ORANGE, anchor="end")
    wire(d, 495, 565, 550, 565, ORANGE)
    box(d, 550, 530, 315, 65, "#fff3dd", "#e1b461")
    text(d, 570, 558, "REGULATED +5 V", 23, ORANGE, True)
    text(d, 570, 585, "Supply output", 19, GND)
    wire(d, 750, 595, 750, 615, GND)
    text(d, 1135, 623, "GND", 27, GND)
    text(d, 470, 623, "GND", 27, GND, anchor="end")
    wire(d, 495, 615, 1110, 615, GND)
    wire(d, 925, 615, 925, 615, GND)
    text(d, 680, 703, "COMMON GROUND", 24, GND, True)
    text(d, 800, 790, "Supply GND, sensor GND and Robo Pico GND are connected together.", 26, GND, anchor="middle")
    text(d, 1135, 640, "+3V3: leave unused", 21, RED)
    text(d, 800, 407, "At this junction: 5.0 V ECHO becomes 3.0 V", 24, SIGNAL, anchor="middle")
    note(d, 910, "ECHO → 10 kΩ → GP3 junction; 15 kΩ from that same junction to GND.",
         "Do not connect ECHO directly to GP3. Servo V+ is only an alternative if verified as a suitable 5 V supply.", True)
    save("04-ultrasonic", d)


def motors():
    d = sheet(5, "Encoder motors: six wires per motor", "Motor-connector numbers are from the PicoCar diagram. Confirm its orientation on your unit.",
              "PicoCar slide 2: PH2.0-6P functions; owner marking KH2.0-6P differs; ROBO-PICO M1/M2 mapping")
    for y, title, a, b, driver in [
        (180, "LEFT motor + encoder", 16, 13, "M1"),
        (575, "RIGHT motor + encoder", 17, 19, "M2")]:
        pair(d, y, title, "Robo Pico carrier",
             [("1  ·  encoder GND", "Grove 4 / GND", GND),
              ("2  ·  encoder A", f"Grove 4 / GP{a}", SIGNAL),
              ("3  ·  encoder B", f"GPIO breakout / GP{b}", SIGNAL),
              ("4  ·  encoder supply", "Grove 4 / +3V3 *", RED),
              ("5  ·  motor + lead", f"{driver}A motor terminal", PURPLE),
              ("6  ·  motor − lead", f"{driver}B motor terminal", PURPLE)], height=390)
    text(d, 60, 1002, "* 3.3 V encoder supply is proposed in the slides; verify the actual encoder's voltage/current rating.", 23, RED)
    save("05-motors", d)


def servo_uart():
    d = sheet(6, "SG90 servo + optional console", "The START/RESET and E-STOP buttons are already connected on the carrier.",
              "PicoCar slide 3 lead identification + ROBO-PICO servo/UART/button mapping")
    pair(d, 175, "SG90 scanner servo", "Robo Pico · Servo port 1",
         [("Orange  ·  signal / PWM", "S / GP12", PURPLE),
          ("Red  ·  positive supply", "V+  ·  verify servo rating", RED),
          ("Brown  ·  ground", "− / GND", GND)], height=240)
    pair(d, 465, "UART adapter (optional)", "Robo Pico · Grove 1",
         [("RX  ·  adapter receives", "GP0  ·  Pico TX", SIGNAL),
          ("TX  ·  optional", "GP1  ·  Pico RX", SIGNAL),
          ("GND", "GND", GND)], height=240)
    note(d, 745, "Servo colours match the slide; verify your unit before inserting its plug.",
         "Servo port 2 / GP13 is reserved for encoder B. UART: 115200 baud, 3.3 V logic; leave adapter VCC off.", True)
    box(d, 60, 875, 1480, 105, "#e9e8fb", "#b5b0de")
    text(d, 90, 918, "ONBOARD GP20 BUTTON", 27, INK, True)
    text(d, 90, 957, "START / RESET · no jumper needed", 24)
    text(d, 860, 918, "ONBOARD GP21 BUTTON", 27, INK, True)
    text(d, 860, 957, "E-STOP · no jumper needed", 24)
    save("06-servo-console", d)


def power():
    d = sheet(7, "Power: keep these supplies distinct", "All sensor, controller and external-supply grounds must share common GND.",
              "ROBO-PICO power limits: input 3.6–6 V, Grove 3V3 total 300 mA, motor 1 A continuous / channel")
    box(d, 60, 190, 610, 275, "#ffedf0", "#dfa2ad")
    text(d, 90, 235, "GROVE +3V3", 32, RED, True)
    lines(d, 90, 285, ["ST188 VCC × 3 (3.3 V-compatible boards)", "GY-511 VIN (module input rated 3–5 V)",
                        "Encoder pin 4 × 2, only if rated for 3.3 V"], 24, INK, 43)
    text(d, 90, 430, "Total Grove 3.3 V output budget: 300 mA", 23, RED, True)
    box(d, 760, 190, 780, 275, "#fff3dd", "#e1b461")
    text(d, 790, 235, "REGULATED NOMINAL 5 V", 32, ORANGE, True)
    lines(d, 790, 285, ["HC-SR04 VCC", "Use an external regulated supply, OR servo V+ only", 
                         "after measuring a suitable voltage under motor load."], 24, INK, 43)
    text(d, 790, 430, "Not Grove +3V3. Keep the ECHO divider installed.", 23, ORANGE, True)
    box(d, 60, 520, 610, 285, "#eee8f8", "#c0acd9")
    text(d, 90, 565, "SERVO V+ / MOTOR DRIVER", 31, PURPLE, True)
    lines(d, 90, 615, ["Servo V+ follows the board's power source.", "It is not a fixed 5 V regulator output.",
                         "SG90 must tolerate the measured servo supply.", "Motor leads go to M1/M2, never directly to GPIO."], 23, INK, 40)
    box(d, 760, 520, 780, 285, "#eaf0ff", "#aabfe7")
    text(d, 790, 565, "ROBO PICO BOARD INPUT", 31, INK, True)
    lines(d, 790, 615, ["Datasheet input range: 3.6–6 V.", "Use its intended power connector and correct polarity.",
                         "M1 / M2: 1 A continuous per channel, 1.5 A <5 s peak.", "Do not tie an external 5 V output to Grove +3V3."], 23, INK, 40)
    wire(d, 120, 875, 1480, 875, GND)
    for x, label in [(150, "ROBO PICO"), (600, "ALL MODULES"), (1120, "EXTERNAL 5 V SUPPLY")]:
        wire(d, x, 875, x, 935, GND)
        text(d, x, 978, label + " GND", 22, GND, True)
    text(d, 800, 850, "ONE COMMON GROUND", 27, GND, True, "middle")
    save("07-power", d)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    overview()
    infrared()
    imu()
    ultrasonic()
    motors()
    servo_uart()
    power()
    page_width, page_height = landscape(A4)
    scale = min(page_width / W, page_height / H)
    pdf = canvas.Canvas(str(OUT / "WIRING.pdf"), pagesize=(page_width, page_height))
    pdf.setTitle("Pico W + Robo Pico — visual wiring guide")
    pdf.setAuthor("INF2004 project")
    for name, drawing in SHEETS:
        svg = OUT / f"{name}.svg"
        renderSVG.drawToFile(drawing, str(svg))
        markup = svg.read_text()
        markup = markup.replace("font-family: DVB;", "font-family: 'DejaVu Sans', Arial, sans-serif; font-weight: bold;")
        markup = markup.replace("font-family: DV;", "font-family: 'DejaVu Sans', Arial, sans-serif;")
        markup = markup.replace("fill: None;", "fill: none;")
        markup = markup.replace("<title>...</title>", f"<title>Robo Pico wiring: {name}</title>")
        svg.write_text(markup)
        pdf.bookmarkPage(name)
        pdf.addOutlineEntry(name.replace("-", " ").title(), name)
        pdf.saveState()
        pdf.translate((page_width - W * scale) / 2, (page_height - H * scale) / 2)
        pdf.scale(scale, scale)
        renderPDF.draw(drawing, pdf, 0, 0)
        pdf.restoreState()
        pdf.showPage()
    pdf.save()
    with tempfile.TemporaryDirectory() as temp:
        prefix = str(Path(temp) / "sheet")
        subprocess.run(["pdftoppm", "-scale-to", "1600", "-png", str(OUT / "WIRING.pdf"), prefix], check=True)
        pages = sorted(Path(temp).glob("sheet-*.png"), key=lambda p: int(p.stem.split("-")[-1]))
        assert len(pages) == len(SHEETS)
        for (name, _), page in zip(SHEETS, pages):
            shutil.copyfile(page, OUT / f"{name}.png")
    print(f"Rendered {len(SHEETS)} SVG/PNG diagrams and WIRING.pdf into {OUT}")


if __name__ == "__main__":
    main()
