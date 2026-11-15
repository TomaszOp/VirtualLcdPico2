# Generuje naglowki *.pio.h z plikow *.pio (pioasm z pakietu arduino-pico) przed kompilacja.
import glob
import os
import subprocess

Import("env")  # noqa: F821

pioasm_dir = env.PioPlatform().get_package_dir("tool-pioasm-rp2040-earlephilhower")  # noqa: F821
pioasm = os.path.join(pioasm_dir, "pioasm")
src_dir = env.subst("$PROJECT_SRC_DIR")  # noqa: F821

for pio in glob.glob(os.path.join(src_dir, "**", "*.pio"), recursive=True):
    header = pio + ".h"
    if not os.path.exists(header) or os.path.getmtime(header) < os.path.getmtime(pio):
        print("pioasm: %s" % os.path.relpath(pio, src_dir))
        subprocess.check_call([pioasm, "-o", "c-sdk", pio, header])
