#!/usr/bin/env python3
"""
Genera PageIndex.h a partir de device_page.html (comprimido con gzip).

La pagina se sirve comprimida para no agotar el heap del ESP8266: sin gzip
son ~18 KB por conexion y con varios clientes a la vez AsyncWebServer se
queda sin memoria, trunca las respuestas y el servidor muere.

Uso:  python3 web/gzip_page.py   (desde la raiz del proyecto)
Ejecutar cada vez que se edite device_page.html, luego recompilar el sketch.
"""

import gzip
import io
import pathlib

HERE = pathlib.Path(__file__).parent          # web/
RAIZ = HERE.parent
SRC = HERE / "device_page.html"
DST = RAIZ / "src" / "PageIndex.h"

def main():
    html = SRC.read_bytes()

    # mtime=0 para que el archivo generado sea reproducible entre ejecuciones
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as gz:
        gz.write(html)
    packed = buf.getvalue()

    lines = []
    for i in range(0, len(packed), 16):
        chunk = packed[i:i + 16]
        lines.append("    " + " ".join(f"0x{b:02X}," for b in chunk))
    body = "\n".join(lines)

    ratio = 100 * len(packed) / len(html)
    DST.write_text(
        "// ARCHIVO GENERADO POR gzip_page.py - NO EDITAR A MANO\n"
        "// Fuente: device_page.html   Editar ese archivo y volver a ejecutar el script.\n"
        f"// {len(html)} bytes -> {len(packed)} bytes gzip ({ratio:.1f}%)\n"
        "\n"
        "#ifndef PAGE_INDEX_H\n"
        "#define PAGE_INDEX_H\n"
        "\n"
        "#include <Arduino.h>\n"
        "\n"
        f"const size_t index_html_gz_len = {len(packed)};\n"
        "\n"
        "const uint8_t index_html_gz[] PROGMEM = {\n"
        f"{body}\n"
        "};\n"
        "\n"
        "#endif // PAGE_INDEX_H\n",
        encoding="utf-8",
    )

    print(f"{SRC.name}: {len(html)} bytes")
    print(f"{DST.name}: {len(packed)} bytes gzip ({ratio:.1f}% del original)")

if __name__ == "__main__":
    main()
