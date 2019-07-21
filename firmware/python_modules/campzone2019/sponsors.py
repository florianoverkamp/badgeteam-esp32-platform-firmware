import rgb, time, system

rgb.clear()
rgb.setfont(rgb.FONT_7x5)

rgb.scrolltext("Your badge was sponsored by:")
time.sleep(10)
rgb.clear()
time.sleep(1)

rgb.scrolltext("Espressif")
time.sleep(4)
rgb.clear()
time.sleep(1)

rgb.scrolltext("AllNet")
time.sleep(3.5)
rgb.clear()
time.sleep(1)

rgb.scrolltext("and")
time.sleep(2.5)
rgb.clear()
time.sleep(1)

rgb.setfont(rgb.FONT_6x3)

for i in range(1, 60):
    intensity = int(float(255) / 60 * i)
    rgb.disablecomp()
    rgb.clear()
    rgb.text("Deloitte", (intensity,intensity,intensity), (1, 1))
    rgb.enablecomp()
    time.sleep(0.03)

time.sleep(2)
rgb.pixel((114, 159, 30), (29, 4))
rgb.pixel((114, 159, 30), (29, 5))
rgb.pixel((114, 159, 30), (30, 4))
rgb.pixel((114, 159, 30), (30, 5))
time.sleep(3)